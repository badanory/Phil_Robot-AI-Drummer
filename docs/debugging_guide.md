# DrumRobot2 디버깅 가이드

힙 오염, segfault, 메모리 문제를 GDB / AddressSanitizer 로 추적하는 방법을 정리한다.  
`tempFrames.clear()` segfault 분석 과정을 예시로 삼아 설명한다.

---

## 1. 빌드 설정

### 1-1. 기본 빌드 (현재)

```makefile
CFLAGS = -Wall -O2 -g -std=c++17 ...
```

`-O2` 최적화가 켜져 있으면 GDB 에서 변수가 인라인·소거되어 값을 못 읽는 경우가 많다.

### 1-2. Makefile — debug 타겟 추가

`Makefile` 의 `clean:` 블록 아래에 아래를 추가한다.

```makefile
.PHONY: debug
debug: CFLAGS := -Wall -g3 -O0 -fno-omit-frame-pointer -std=c++17 -fPIC \
                 $(shell pkg-config --cflags opencv4)
debug: all
```

빌드:

```bash
cd DrumRobot2
make clean
make debug
```

### 1-3. AddressSanitizer (ASAN) 타겟 추가

```makefile
.PHONY: asan
asan: CFLAGS  := -Wall -g3 -O0 -fno-omit-frame-pointer -fsanitize=address \
                 -std=c++17 -fPIC $(shell pkg-config --cflags opencv4)
asan: LDFLAGS := $(LDFLAGS) -fsanitize=address
asan: all
```

빌드 및 실행:

```bash
make clean && make asan
sudo ASAN_OPTIONS=detect_stack_use_after_return=1 \
     DRUM_SIL_MODE=1 ./bin/main.out
```

crash 직후 어느 스레드가 어느 주소를 잘못 접근했는지,  
**할당 위치 / 해제 위치 / 접근 위치** 세 곳의 스택이 모두 출력된다.  
GDB 보다 훨씬 빠르게 root cause 를 찾을 수 있다.

---

## 2. GDB 사용법

### 2-1. 기본 실행

```bash
cd DrumRobot2/bin
sudo gdb ./main.out
```

```gdb
set pagination off
set print thread-events on
handle SIGSEGV stop print   # segfault 발생 시 자동 정지
run
```

### 2-2. Crash 발생 후 분석 명령

| 목적 | 명령 |
|------|------|
| 현재 스레드 스택 | `bt full` |
| **전체 스레드 스택** | `thread apply all bt` |
| 특정 프레임 이동 | `frame <N>` |
| 로컬 변수 출력 | `info locals` |
| 특정 변수 출력 | `print <변수명>` |
| 특정 변수 변경 감시 | `watch -l <변수명>` |

### 2-3. 브레이크포인트 설정 예시

```gdb
# 함수 진입 시 정지
break CanManager::distributeFramesToMotors
break CanManager::readFramesFromAllSockets

# 조건부 브레이크 (motor->socket 이 -1 일 때만)
break CanManager.cpp:1096 if motor->socket == -1
```

### 2-4. 멀티스레드 race condition 추적

```gdb
# 특정 스레드로 이동
info threads
thread <N>

# 다른 스레드 잠시 멈추고 현재 스레드만 실행
set scheduler-locking on
continue
```

---

## 3. tempFrames segfault 분석 사례

### 증상

`tempFrames.clear()` 를 쓰면 segfault, `pair.second.clear()` 로 바꾸면 정상.

### 분석 과정

**Step 1 — 두 함수의 동작 차이 확인**

| 함수 | 동작 |
|------|------|
| `tempFrames.clear()` | map 의 모든 node 파괴 (key + vector 전체 `free()`) |
| `pair.second.clear()` | vector 내용만 비움, map node 는 메모리에 유지 |

유일한 차이: `free()` 호출 여부.  
heap 이 오염된 상태에서 `free()` 를 부르면 allocator 내부 일관성 검사가 crash 를 유발한다.

**Step 2 — spurious entry 생성 경로 추적**

```cpp
// distributeFramesToMotors() 내부
for (auto &frame : tempFrames[motor->socket])  // operator[] 사용
```

`std::map::operator[]` 는 키가 없으면 **기본값으로 새 entry 를 생성**한다.

`Motor.hpp:29` 에서 `int socket = -1` 이 기본값이므로,  
소켓이 할당되지 않은 모터마다 `tempFrames[-1]` spurious entry 가 만들어진다.

**Step 3 — 반복 횟수 계산**

`recvLoopForThread` 주기: **100 µs = 초당 10,000 회**

매 루프마다:
1. `distributeFramesToMotors()` 에서 `tempFrames[-1]` 생성 (malloc)
2. `tempFrames.clear()` 에서 `tempFrames[-1]` 삭제 (free)

STL map 의 red-black tree node alloc/free 가 초당 만 번 이상 반복되며  
heap allocator 내부 state 가 오염됨.

**Step 4 — 근본 원인 확인**

`operator[]` 대신 `find()` 를 쓰면 키가 없을 때 entry 를 만들지 않는다:

```cpp
// 수정 전
for (auto &frame : tempFrames[motor->socket])

// 수정 후
std::map<int, std::vector<struct can_frame>>::iterator sock_frames =
    tempFrames.find(motor->socket);
if (sock_frames == tempFrames.end()) continue;
for (struct can_frame &frame : sock_frames->second)
```

clear 도 함께 수정:

```cpp
// 수정 전
tempFrames.clear();

// 수정 후
for (auto &kv : tempFrames)
{
    kv.second.clear();
}
```

그리고 소켓 생성 시점에 `tempFrames` entry 를 미리 만들어 두면  
런타임에서 map node alloc/free 가 완전히 없어진다:

```cpp
// initializeCAN() 및 openSilVcan() 에서 소켓 생성 직후
tempFrames.emplace(hsocket, std::vector<struct can_frame>{});
```

---

## 4. 자주 쓰는 ASAN 옵션

```bash
ASAN_OPTIONS=detect_stack_use_after_return=1   # 스택 변수 해제 후 접근 감지
ASAN_OPTIONS=halt_on_error=0                    # 첫 오류 후 계속 실행
ASAN_OPTIONS=log_path=/tmp/asan.log             # 로그 파일로 저장
```

---

## 5. Valgrind (ASAN 대안)

ASAN 빌드가 어려울 때 대안으로 사용한다. 속도는 느리다.

```bash
sudo valgrind --tool=memcheck --leak-check=full \
              --track-origins=yes \
              env DRUM_SIL_MODE=1 ./bin/main.out
```
