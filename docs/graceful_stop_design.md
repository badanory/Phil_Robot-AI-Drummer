# 드럼 로봇의 자연스러운 연주 종료 (Graceful Stop) 설계 문서

이 문서는 드럼 로봇이 연주 중에 '정지' 명령을 받았을 때, 로봇이 갑자기 굳어버리지 않고 실제 드러머처럼 자연스럽게 연주를 마무리하도록 만드는 방법에 대해 설명합니다.

---

## 1. 왜 이 기능이 필요한가요? (배경 및 문제점)

현재 로봇의 연주 방식은 다음과 같습니다.
1. **IK 스레드(수학 계산):** 악보 파일을 읽고 로봇이 어떻게 움직여야 할지(궤적)를 매우 빠른 속도로 한 번에 계산해 버립니다.
2. **SEND 스레드(모터 전송):** 계산된 궤적 데이터를 5ms(0.005초)마다 하나씩 꺼내서 모터로 보냅니다.

**문제점:**
수학 계산(IK)이 워낙 빠르다 보니 전체 곡의 움직임이 이미 메모리에 다 쌓여 있습니다. 이때 중간에 **"그만 쳐!"(정지)** 라는 명령이 들어오면, 로봇은 쌓여있던 데이터를 그냥 몽땅 지워버리고 제자리에 멈춰버립니다. 
결과적으로 로봇이 치던 도중에 허공에서 뻣뻣하게 굳어버리거나, 모터에 무리가 가는 부자연스러운 모습을 보이게 됩니다. 박사님께서 원하시는 것은 **"퉁.. 퉁. 퉁.." 하면서 마디를 끝맺고 천천히 멈추는 실제 사람 같은 마무리**입니다.

---

## 2. 어떻게 해결할 것인가요? (핵심 아이디어)

비동기(각자 알아서 빠르게 일함)로 돌아가던 두 스레드의 속도를 **동기화(서로 눈치를 보며 일함)** 시키는 것이 핵심입니다.

* **마디 단위로 쪼개기:** 곡 전체를 한 번에 계산하지 않고, 1~2마디 단위로만 잘라서 계산합니다.
* **제한된 바구니(Bounded Buffer) 사용:** IK 스레드가 계산한 마디를 담을 바구니를 만듭니다. 바구니의 크기는 딱 '2마디'로 제한합니다.
* **마무리 모션(Modifier) 적용:** 정지 명령이 들어오면 즉시 멈추지 않고, '마무리용 부드러운 궤적' 하나를 마지막으로 만들어 바구니에 넣고 계산을 종료합니다. SEND 스레드는 바구니가 완전히 빌 때까지 연주하고 자연스럽게 홈(HOME)으로 돌아갑니다.

---

## 3. 구체적인 동작 방식 (쉽게 풀어쓴 원리)

### 3.1. 바구니를 통한 눈치 게임 (생산자와 소비자 패턴)
* **IK 스레드 (요리사):** 악보를 보고 궤적 요리를 만듭니다. 만든 요리를 바구니에 올려둡니다. 만약 바구니에 요리가 2개(2마디) 꽉 차 있으면, 서빙이 끝날 때까지 요리를 멈추고 **기다립니다 (Wait)**.
* **SEND 스레드 (서빙 종업원):** 5ms마다 바구니에서 요리를 꺼내 모터로 서빙합니다. 바구니에 있는 한 마디를 다 서빙하면 다음 마디를 꺼냅니다. 꺼내고 나서 자리가 생기면 요리사에게 **"자리 났어요!" 하고 알려줍니다 (Notify)**.

이 방식을 사용하면 IK 스레드가 미리 곡 전체를 다 계산해버리는 일이 없어집니다. 항상 현재 연주하는 마디와 바로 다음 마디, 딱 2마디만 메모리에 존재하게 됩니다.

### 3.2. 정지(Stop) 명령이 들어왔을 때의 마법
1. 외부에서 **"정지!"** 명령이 들어옵니다. (플래그 변경: `is_graceful_stopping = true`)
2. **IK 스레드**가 다음 마디를 계산하려다가 이 플래그를 발견합니다.
3. 원래 쳐야 할 세게 치는 궤적 대신, **점점 힘을 빼고 천천히 치는 마무리 궤적(Modifier 적용)**을 특별히 계산합니다.
4. 이 마무리 궤적에 **"이게 진짜 마지막 마디야(is_last_measure = true)"** 라는 꼬리표를 붙여서 바구니에 넣고, 요리사는 퇴근합니다.
5. **SEND 스레드**는 평소처럼 바구니에서 궤적을 꺼내 서빙합니다. 
6. 마지막 꼬리표가 붙은 마무리 궤적까지 전부 모터로 보내고 나면, "아, 연주가 진짜 끝났구나" 하고 인식한 뒤 로봇을 **안전한 기본 자세(HOME)**로 부드럽게 되돌려 보냅니다.

---

## 4. 코드로 보는 설계 밑그림

C++의 `std::queue`, `std::mutex`, `std::condition_variable`을 사용하여 바구니(버퍼)를 안전하게 관리합니다.

```cpp
// 1. 바구니에 담길 하나의 '마디' 데이터 꾸러미
struct MeasureTrajectory {
    std::vector<JointState> points; // 5ms 단위로 쪼개진 모터 각도들
    bool is_last_measure;           // 이게 마지막 마무리 마디인가요?
};

// 2. 동기화를 위한 도구들
std::queue<MeasureTrajectory> trajectoryQueue; // 마디를 담을 바구니
std::mutex queueMutex;                         // 바구니에 접근할 때 쓰는 자물쇠
std::condition_variable queueCV;               // 서로에게 신호를 보내는 알람벨

const int MAX_QUEUE_SIZE = 2;                  // 바구니는 최대 2마디까지만!
bool is_graceful_stopping = false;             // 정지 명령이 들어왔는지 확인하는 깃발
```

위 구조를 바탕으로 `DrumRobot.cpp`의 IK 해석 부분과 `sendLoopForThread` 부분을 수정하여 서로 신호(`wait`와 `notify`)를 주고받도록 구현합니다.

---

## 5. 이 방식으로 얻는 장점 (기대 효과)

1. **진짜 사람 같은 로봇 (자연스러움):** 뚝 끊기는 기계적인 멈춤이 사라지고, 드러머가 박자를 타며 연주를 끝맺는 듯한 전문적인 시각적 효과를 줍니다.
2. **로봇 보호 (안전성):** 고속으로 움직이던 모터가 갑자기 궤적을 잃어버리면 전류가 튀거나 기어에 충격이 갈 수 있습니다. 부드럽게 감속하며 끝나기 때문에 하드웨어가 보호됩니다.
3. **메모리 절약:** 몇십 분짜리 긴 곡을 쳐도 메모리에는 항상 딱 2마디의 데이터만 올라가 있으므로, 컴퓨터 자원을 훨씬 적게 차지합니다.
4. **미래를 위한 확장성:** '바구니' 방식이 완성되면, 나중에 연주 도중 실시간으로 속도(BPM)를 올리거나 내리는 등 '즉흥적인 변화'를 주기가 아주 쉬워집니다. 다음 마디를 계산할 때 바로 적용하면 되기 때문입니다.

## 6. 실제 C++ 코드 구현 매핑 (코딩 테스트 스타일)

Pioneer님이 제안해주신 논리를 실제 현재 드럼 로봇의 클래스 구조(`PathManager`, `DrumRobot`, `GenericMotor` 등)에 맞게 코드로 어떻게 구현할지 1:1로 매핑한 내용입니다.

### 6.1. 추가해야 할 전역/멤버 변수 (동기화 객체)
`PathManager.hpp` 클래스 멤버 변수로 아래의 동기화 객체와 플래그를 추가합니다.

```cpp
// PathManager.hpp 내부에 추가
std::mutex measureMutex;               // 자물쇠 (Pioneer님의 'lock을 올려')
std::condition_variable cv_full;       // 꽉 찼을 때 대기하는 알람
std::condition_variable cv_empty;      // 비었을 때 대기하는 알람

const int MAX_MEASURE_BUFFER = 2;      // 큐에 쌓을 수 있는 최대 마디 수 (Pioneer님의 '큐에 두개를 쌓아')
int current_measure_count = 0;         // 현재 버퍼에 쌓인 마디의 개수

bool is_graceful_stopping = false;     // Pioneer님의 '정지 명령이 들어오면'
```

### 6.2. 생산자 (Producer) : IK 계산 및 궤적 밀어넣기
현재 `PathManager::solveIKandPushCommand()` (또는 `processLine()`) 부분에서 모터의 `commandBuffer`로 궤적을 밀어 넣습니다. 여기에 상호배제 로직을 추가합니다.

```cpp
void PathManager::solveIKandPushCommand()
{
    // ... [기존 로직: 마디 궤적 계산] ...

    // 1. 자물쇠 획득 (Pioneer님의 'lock을 올려')
    std::unique_lock<std::mutex> lock(measureMutex);

    // 2. 큐가 두 개 채워졌으면 대기 (Pioneer님의 '두개 채우면 그때부터 진행시켜 (대기)')
    cv_full.wait(lock, [this]() { 
        return current_measure_count < MAX_MEASURE_BUFFER; 
    });

    // 3. 정지 명령 확인 및 Modifier 적용 (Pioneer님의 '정지 명령이 들어오면 ... modifier에서 집어 넣어')
    bool is_last = false;
    if (is_graceful_stopping) {
        // [Modifier 로직 적용: 속도 감소, 마지막 타격 등]
        is_last = true; // Pioneer님의 '닫은 지점을 기억해'
    }

    // 4. 모터 버퍼에 데이터 밀어 넣기 (Pioneer님의 '하나 밀어 넣어')
    for (int i = 0; i < n; i++) {
        // q 계산 및 모터별 commandBuffer.push(q) 진행
        // (이 때, 방금 넣은 점들이 묶여서 1개의 마디로 카운트됨)
    }
    
    // 만약 이 마디가 마지막 마디라면, 모터 버퍼의 끝에 플래그를 달아줍니다.
    if (is_last) {
        // 모터 큐 마지막 데이터에 is_last_measure = true 플래그 표시
    }

    current_measure_count++; // 마디 개수 증가

    // 5. 자물쇠 풀기 (Pioneer님의 '다시 닫아' - lock 해제)
    lock.unlock();

    // 6. 소비자에게 데이터가 들어갔음을 알림
    cv_empty.notify_one();

    // 마지막이었다면 생산자 스레드는 스스로 종료
    if (is_last) return;
}
```

### 6.3. 소비자 (Consumer) : SEND 스레드의 데이터 소비
`DrumRobot::sendLoopForThread()` 내부 또는 `CanManager::setCANFrame()`에서 모터 버퍼를 읽어가는(소비하는) 로직에 매핑됩니다.

```cpp
void DrumRobot::sendLoopForThread()
{
    while (state.main != Main::Shutdown)
    {
        // 1. 자물쇠 획득
        std::unique_lock<std::mutex> lock(pathManager.measureMutex);

        // 2. 버퍼가 완전히 비었으면 대기 (데이터가 올 때까지)
        pathManager.cv_empty.wait(lock, [this]() { 
            return pathManager.current_measure_count > 0; 
        });

        // 3. 하나 소비 (Pioneer님의 '한 개 소비해')
        // (실제로는 5ms 루프마다 한 Point씩 빼서 쓰다가, 한 마디가 끝나면 카운트를 줄임)
        bool is_measure_ended = canManager.setCANFrame(...); 
        
        bool found_last_flag = false; // '닫은 지점'을 찾았는지 여부

        if (is_measure_ended) {
            pathManager.current_measure_count--; // 마디 하나 완전 소비 완료!
            
            // 마지막 닫은 지점 플래그를 읽었다면 기록 (Pioneer님의 '닫은 지점을 기억해 그리고 소비해')
            if (/* 큐에서 꺼낸 데이터.is_last_measure == true */) {
                found_last_flag = true;
            }

            // 4. 자물쇠 풀고, 생산자에게 "공간 하나 생겼어!" 알림 (Pioneer님의 '그럼 하나 밀어 넣어')
            lock.unlock();
            pathManager.cv_full.notify_one();
        } else {
            lock.unlock();
        }

        // 5. 만약 방금 소비한 마디가 마지막이었다면 완전히 연주 종료
        if (found_last_flag) {
            state.main = Main::AddStance; 
            flagObj.setAddStanceFlag(FlagClass::HOME);
            break; // SEND 스레드도 HOME으로 가면서 깔끔하게 끝
        }

        // 5ms 대기
        this_thread::sleep_until(sendLoopPeriod);
    }
}
```

### 6.4. Pioneer님 설계 검증 요약
* **"큐에 두개를 쌓아. lock을 올려. 두개 채우면 진행시켜."** ➡️ `cv_full.wait(...)` 와 `current_measure_count < MAX_MEASURE_BUFFER` 로 완벽하게 구현됩니다.
* **"한 개 소비해. 하나 밀어 넣어. 다시 닫아."** ➡️ 소비자가 1마디를 다 읽으면 카운트를 내리고 `notify`를 호출하여, 생산자가 `wait`에서 깨어나 다음 마디를 계산(`push`)하도록 만듭니다.
* **"정지 명령이 들어오면 modifier에서 집어넣어"** ➡️ `is_graceful_stopping` 플래그를 체크해 마지막 궤적을 튜닝합니다.
* **"닫은 지점을 기억해. 그리고 소비해"** ➡️ 마지막 데이터 구조체 내에 `is_last_measure = true` 플래그를 달아두고, 소비자가 이 플래그가 달린 데이터를 읽는 순간을 캐치하여 로봇을 HOME으로 돌려보냅니다.
