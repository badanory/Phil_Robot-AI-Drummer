# drum_intheloop TODO

`drum_intheloop`는 `DrumRobot2`의 command-level 출력을 `named pipe`로 받아
`PyBullet`에 적용하는 SIL 디렉터리다.

이 보드는 두 층을 같이 적는다.

- `drum_intheloop` 안에서 직접 고치는 simulator 작업
- simulator를 켰을 때만 드러나는 통합 blocker

단, 통합 blocker를 적더라도 수정 책임이 이 디렉터리 밖이면 그 사실을 같이 적는다.

현재 방향:

- 현재 공식 경계는 `LLM 플래너 -> 로봇 제어기 -> command-level simulator`이다.
- 현재 ingress는 `/tmp/drum_command.pipe` NDJSON이다.
- 현재 backend는 `resetJointState()` 기반 즉시 적용형 viewer다.
- **단기 목표**: open-loop → close-loop 전환. PyBullet 시뮬레이션 joint state를 `vcan0`
  으로 C++ recv loop에 돌려줘 `motor.jointAngle`을 갱신한다.
- 장기 목표는 command-level seam을 유지한 채, 이후 `vcan` 또는 `struct can_frame`
  기반 CAN-frame SIL로 넘어가는 것이다.

운영 규칙:

- `Now`에는 최대 3개만 둔다.
- 원본 URDF/STL 수정 대신 runtime patch를 우선한다.
- 라우팅 문제, 각도 semantic 문제, visual/frame 문제, C++ state 문제를 섞어 쓰지 않는다.
- simulator를 켰을 때만 드러나는 외부 blocker는 따로 적되, simulator 내부 작업과 섞어 쓰지 않는다.

---

## Close-Loop (vcan feedback) 구현 마스터 플랜

현재 문제 요약:

현재 SIL은 open-loop이다.

```text
발화 → LLM → C++ command → pipe → PyBullet    (명령 방향)
C++ motor.jointAngle = 초기값(0 or initial)     ← 피드백 없음
```

C++의 `motor.jointAngle`이 갱신되지 않으므로:
- 다음 명령이 들어오면 trajectory가 잘못된 시작 위치에서 계산된다.
- planner/validator가 current_angles를 snapshot할 때 항상 초기값만 보인다.
- `initializePos("h")` (HOME 자세) 명령이 계속 반복 시도될 수 있다.

목표 구조:

```text
발화 → LLM → C++ command → pipe → PyBullet
                                       ↓  stepSimulation 후 joint state 읽기
                                    vcan0 (struct can_frame)
                                       ↓  recv loop (100us 주기)
                           C++ CanManager → motor.jointAngle 갱신
                                       ↓
                           state JSON broadcast → LLM current_angles 정확
```

---

### Phase 0: conda env `sil` 생성

**목적**: pybullet + python-can 이 함께 필요한 전용 환경.

```bash
conda create -n sil python=3.10 -y
conda activate sil
pip install pybullet python-can
```

완료 기준:
- `conda activate sil && python -c "import pybullet, can; print('OK')"` 가 정상 출력.

---

### Phase 1: vcan0 커널 모듈 & 인터페이스 설정

**목적**: `vcan` 드라이버를 올리고 `vcan0` 인터페이스를 만든다.

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

확인:
```bash
ip link show vcan0   # UP,LOWER_UP 이어야 한다.
```

**부팅 후 자동화 (선택)**: `/etc/rc.local` 또는 systemd service에 위 3줄 추가.

**파일**: 없음 (시스템 설정).

완료 기준:
- `ip link show vcan0` 에서 `state UNKNOWN mode DEFAULT ... UP` 확인.
- `candump vcan0` 실행 후 `cansend vcan0 001#0000000000000000` 으로 수신 확인.

---

### Phase 2: Python vcan state writer 구현

**파일**: `sil/vcan_state_writer.py`

**역할**: PyBullet의 joint state를 읽어 C++이 기대하는 CAN 피드백 프레임으로 인코딩하여 vcan0에 쓴다.

#### 2-1. 모터 스펙 테이블

C++ `DrumRobot.cpp`의 `initializeMotors()`와 일치해야 한다.

```text
모터명       nodeId  cwDir  initialJointAngle_deg  종류
waist          0x00    1.0         10.0            TMotor
R_arm1         0x01   -1.0         90.0            TMotor
L_arm1         0x02   -1.0         90.0            TMotor
R_arm2         0x03    1.0          0.0            TMotor
R_arm3         0x04   -1.0         90.0            TMotor
L_arm2         0x05   -1.0          0.0            TMotor
L_arm3         0x06    1.0         90.0            TMotor
R_wrist        0x07   -1.0         90.0            Maxon  rxPdoIds[0]=0x187
L_wrist        0x08   -1.0         90.0            Maxon  rxPdoIds[0]=0x188
maxonForTest   0x09    1.0          0.0            Maxon  rxPdoIds[0]=0x189
R_foot         0x0A    1.0          0.0            Maxon  rxPdoIds[0]=0x18A
L_foot         0x0B   -1.0          0.0            Maxon  rxPdoIds[0]=0x18B
```

#### 2-2. URDF joint angle → production joint angle 역변환

`joint_map.py`의 PRODUCTION_TO_URDF_CAN_TRANSFORM 순방향 수식:
```
mapped_deg = bias + ref + sign * (prod_deg - ref)
```

역변환:
```
prod_deg = ref + sign * (mapped_deg - bias - ref)
         = sign * (mapped_deg - bias) + ref * (1 - sign)
```
(sign = ±1이므로 1/sign = sign)

#### 2-3. production joint angle → motor position (rad)

TMotor / Maxon 모두 (useFourBarLinkage=false):
```
motor_pos_rad = (joint_angle_rad - initial_joint_angle_rad) * cwDir
```

#### 2-4. TMotor CAN 피드백 프레임 인코딩

C++의 `TMotorServoCommandParser::motor_receive()` 가 읽는 포맷 (역):
```
can_id  = nodeId
data[0..1] = int16_t big-endian   pos_int = round(motor_pos_rad * 1800 / π)
data[2..7] = 0  (속도 / 전류 / 온도 / 에러 → SIL에서는 0)
dlc = 8
```

#### 2-5. Maxon CAN 피드백 프레임 인코딩

C++의 `MaxonCommandParser::parseRecieveCommand()` 가 읽는 포맷 (역):
```
can_id  = rxPdoIds[0]
data[0] = 0
data[1] = 0x37  (statusBit = 동작 정상)
data[2..5] = int32_t little-endian
    pos_enc = round(motor_pos_rad * 35 * 4096 / (2π))
data[6..7] = 0  (torque)
dlc = 8
```

#### 2-6. vcan0 소켓 열기 (python-can)

```python
import can
bus = can.interface.Bus(channel="vcan0", bustype="socketcan")
bus.send(can.Message(arbitration_id=can_id, data=data, is_extended_id=False))
```

**완료 기준**:
- `candump vcan0` 실행 상태에서 Python writer가 waist feedback 1회 쓸 때
  `vcan0  001   [8]  00 00 ...` 와 같이 노드 ID 0x00 프레임이 보인다.

---

### Phase 3: SilCommandPipeReader에 vcan writer 통합

**파일**: `sil/SilCommandPipeReader.py`

tick을 처리할 때 (`frame_targets`가 있는 경우):

```python
backend.apply_targets(frame_targets)
backend.step()
# ★ close-loop: PyBullet joint state → vcan0 피드백
if vcan_writer is not None:
    joint_states = backend.read_joint_states()  # Dict[urdf_joint_name, deg]
    vcan_writer.send_all(joint_states)
frame_targets.clear()
```

`backend.read_joint_states()` 는 PyBulletBackend에 추가하는 새 메서드:
```python
def read_joint_states(self) -> Dict[str, float]:
    # getJointState → position (rad) → deg 변환
    return {joint_name: math.degrees(p.getJointState(self._robot_id, idx)[0])
            for joint_name, idx in self._joint_index_by_name.items()}
```

**완료 기준**:
- reader 실행 중 tick 수신 후 `candump vcan0` 에서 다수 CAN 프레임이 연속 수신된다.

---

### Phase 4: C++ CanManager vcan0 소켓 추가

**파일**: `DrumRobot2/src/CanManager.cpp`, `DrumRobot2/include/managers/CanManager.hpp`

목적: SIL 모드에서 vcan0을 열고, 연결 안 된 모터의 `socket` 필드를 vcan0 FD로 교체.

#### 4-1. vcan0 소켓 열기 함수 추가

```cpp
// CanManager.cpp
int CanManager::openVcanSocket(const std::string& ifname) {
    int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) return -1;
    struct ifreq ifr;
    strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ - 1);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) { close(sock); return -1; }
    struct sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(sock); return -1; }
    // non-blocking 모드
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    return sock;
}
```

#### 4-2. SIL 모드 초기화 시 vcan0 소켓을 disconnected 모터에 할당

`checkCanPortsStatus()` 또는 `setMotorsSocket()` 뒤에:

```cpp
if (silModeEnabled) {
    int vcan_fd = openVcanSocket("vcan0");
    if (vcan_fd >= 0) {
        sockets["vcan0"] = vcan_fd;
        for (auto& [name, motor] : motors) {
            if (!isConnected[name]) {
                motor->socket = vcan_fd;
            }
        }
        std::cout << "[SIL] vcan0 opened, assigned to disconnected motors\n";
    }
}
```

#### 4-3. readFramesFromAllSockets 동작 확인

기존 코드는 `motor->socket` 기준 unique FD 집합을 순회한다.
vcan_fd가 할당되면 자동으로 포함된다 → 코드 변경 없이 동작.

`distributeFramesToMotors`도 `frame.can_id & 0xFF == tMotor->nodeId` 비교로
TMotor를, `frame.can_id == maxonMotor->rxPdoIds[0]` 로 Maxon을 구분한다 → 변경 없이 동작.

**완료 기준**:
- C++ recv loop에서 `tMotor->jointAngle` 이 SIL 명령 후 지속 갱신되는 것을
  `cout << tMotor->jointAngle` 로 확인 (디버그 출력 일시 추가).
- `DrumRobot.makeStateJson()` 의 `current_angles` 가 초기값이 아닌 실제 시뮬 값을 반영한다.

---

### Phase 5: idle return 조건 재검토

**파일**: `sil/SilCommandPipeReader.py`

기존 `IDLE_RETURN_SEC = 5.0` 은 close-loop 이후에도 유지 가능하지만,
C++ 쪽에서 joint state가 올바르게 반영되면 HOME 복귀 명령이 반복되지 않아야 한다.

검증 항목:
- close-loop 활성화 후 idle 5초 대기 → PyBullet이 startup pose로 복귀
- 이 복귀 시 vcan 피드백도 startup pose 각도로 전송되는지 확인
- C++가 불필요하게 HOME/h 명령을 재생성하지 않는지 확인

---

### Phase 6: 동작 중 명령 인터럽트 (smooth transition)

**목표**: 동작 와중에 새 명령이 들어와도 현재 위치(PyBullet 기준)에서 부드럽게 이어짐.

이 Phase는 Phase 4 완료 후에 시작한다.

조건이 되는 이유:
- close-loop 이전: 새 명령이 들어오면 C++가 `motor.jointAngle`(=초기값)을 시작점으로 계산
  → 현재 SIL 위치와 달라서 갑작스러운 점프 발생.
- close-loop 이후: `motor.jointAngle`이 PyBullet 실제 위치 → trajectory 시작점 정확 → 자연스러운 전환.

추가 구현 사항:
- `AgentSocket::isInterruptCmd` 가 새 body command도 즉시 통과시킬지 검토.
- `AgentAction::executeCommand` 가 이전 trajectory buffer를 클리어하고 새 trajectory를 시작하는지 확인.
- C++ `commandBuffer` clear → PyBullet `frame_targets` clear 동기화 불필요 (pipe는 단방향).

---

### Phase 7: 검증 시나리오

1. **기본 close-loop 검증**
   - reader + `DRUM_SIL_MODE=1 ./main.out` + phil_brain.py 실행
   - "인사해" 발화 → PyBullet에서 인사 동작
   - `candump vcan0` 으로 피드백 프레임 연속 수신 확인
   - C++ `current_angles` log 확인 (초기값이 아닌 실제 값)

2. **idle return 검증**
   - 5초 이상 대기 → startup pose 복귀 → vcan 피드백 startup pose 전송 확인

3. **인터럽트 검증**
   - 인사 동작 중 "스네어 쳐" 발화 → 인사 중단 위치에서 스네어 동작으로 이어지는지 확인
   - PyBullet에서 끊김 없이 전환 확인

4. **HOME 반복 방지 검증**
   - close-loop 이전: HOME 자세로 계속 돌아가던 문제 재현
   - close-loop 이후: 동일 시나리오에서 HOME 복귀 없음 확인

---

## Frame-Accurate SIL (가상 Serial & VCAN 송신) 설계 방향

현재 파이프(pipe) 기반의 명령어 수준(Command-level) SIL을 완벽한 **프레임 수준(Frame-accurate)** SIL로 고도화하기 위한 아키텍처 방향이다.
정지 보간(감속 제어)과 같은 저수준(Low-level) 하드웨어 제어 결과를 시뮬레이터에 그대로 반영하기 위함이다.

### 1. 다이나믹셀(Neck) 가상 시리얼 SIL (우선 진행)
* **목표**: `socat`을 이용해 가상 시리얼 포트(PTY)를 열고, Python 시뮬레이터가 다이나믹셀 모터(Slave) 역할을 수행한다.
* **C++ 제어기**: `DRUM_SIL_MODE=1`일 때 `/dev/ttyUSB0` 대신 가상 포트(예: `/dev/pts/1`) 연결.
* **Python 시뮬레이터**: 
  - `pyserial`로 반대쪽 가상 포트(예: `/dev/pts/2`)를 열어 Raw 바이트 읽기.
  - Protocol 2.0 헤더(`0xFF 0xFF 0xFD 0x00`)를 찾아 디코딩하여 목표 각도 추출 후 PyBullet 적용.
  - **주의(SyncRead 대응)**: C++이 `GroupSyncRead`를 호출할 경우 모터가 응답할 때까지 블로킹되므로, 시뮬레이터가 모터인 척 Status Packet 양식을 조립하여 응답(ACK)해야 Timeout 에러를 방지할 수 있다.

### 2. TMotor / Maxon 제어 명령 VCAN 송신 전환
* **목표**: `SilCommandPipeWriter`를 통한 파이프라인 송신을 폐기하고, `CanManager`가 처리한 최종 `can_frame`을 `vcan0`로 직접 쏜다.
* **Python 시뮬레이터**: `vcan0`에서 패킷을 수신하고, 각 모터 데이터시트에 맞춰 비트 마스킹/시프팅 파서(Parser)를 직접 작성하여 목표 제어값을 PyBullet에 적용한다.

---

## Now

- [ ] **[WIP] 다이나믹셀(Neck) 가상 시리얼(Virtual Serial) SIL 구축** (최우선 작업)
  - 목표: `socat` PTY 페어를 생성하고 Python이 슬레이브 모터 역할을 하여 완벽한 프레임 수준 통신 동기화를 이룬다.
  - 메모: 진행 간 SyncRead 응답(ACK) 처리에 특히 주의한다. 세부 사항은 'Frame-Accurate SIL 설계 방향' 참고.

- [x] `READY -> snare` 자세 mismatch를 층별로 분리
  - 목표: READY/시작 자세가 스네어 방향으로 모이지 않는 원인이 startup preset인지,
    joint 의미 매핑인지, visual frame patch인지 분리한다.
  - 확인 포인트:
    - `sil/robot_spec.py` startup preset
    - `sil/joint_map.py` CAN -> URDF transform
    - `sil/urdf_tools.py` runtime pose patch
  - 완료 기준:
    - 원인 층이 문서로 명시된다.
    - 기준 스크린샷 또는 joint target 값이 함께 남는다.

- [ ] **[WIP] close-loop vcan feedback 구현** ← 현재 작업
  - Phase 0~4가 완료 기준.
  - 수정 파일:
    - `sil/vcan_state_writer.py` (신규)
    - `sil/pybullet_backend.py` (`read_joint_states()` 추가)
    - `sil/SilCommandPipeReader.py` (vcan writer 통합)
    - `DrumRobot2/src/CanManager.cpp` (openVcanSocket, SIL vcan 할당)
    - `DrumRobot2/include/managers/CanManager.hpp` (선언)
    - `requirements_sil.txt` 또는 conda `sil` env
  - 완료 기준:
    - C++ `motor.jointAngle`이 PyBullet 시뮬 각도와 일치 (오차 < 1deg).
    - `makeStateJson()`의 `current_angles`가 실제 PyBullet 자세를 반영.
    - 인사 후 새 명령이 들어올 때 H 자세로 돌아가지 않음.

- [ ] arm visual 보정값을 `눈대중`과 `확정값`으로 분리
  - 목표: 현재 xyz/rpy 값이 eyeballing 기반이라는 사실을 남기고, 추후 측정값으로
    교체하기 쉽게 정리한다.
  - 완료 기준:
    - 링크별 patch 값에 임시/확정 상태가 드러난다.
    - 값이 왜 필요한지 한 줄 설명이 붙는다.

## Next

- [ ] command-level seam에서 CAN-frame SIL로 넘어갈 migration checklist 작성
  - 목표: 지금 seam이 무엇을 보존하고 무엇을 잃는지 정리해, 다음 단계가
    `frame-accurate SIL` 쪽으로 이어지게 한다.
  - 포함 항목:
    - 현재 pipe payload가 보존하는 것
    - 현재 pipe payload가 잃는 것
    - `vcan` vs `struct can_frame` replay 후보
    - timing fidelity requirements

- [ ] named pipe trace capture/replay 러너 추가
  - command-level ingress를 파일로 저장하고 재생해 mapping/visual regression을 반복 확인한다.

- [ ] startup pose와 실제 첫 pipe command 사이의 덮어쓰기 흐름을 더 잘 보이게 만들기
  - startup preset이 잠깐 보이는 문제와 실제 HOME/READY 흐름을 눈으로 분리해서 확인한다.

- [ ] joint target / URDF joint name overlay 또는 로그 정리
  - `move:L_arm2`가 실제로 어떤 URDF joint에 몇 도로 적용됐는지 바로 확인할 수 있게 만든다.

## Simulator-Triggered Integration Blockers

- [ ] simulator ON일 때 `robot_state.current_angles`가 planner/validator를 흔드는 경로 정리
  - 현상:
    - simulator를 켜지 않았을 때는 `current_angles`가 단순 0도 스냅샷 수준으로 들어와도
      planner/validator 해석이 비교적 예측 가능하다.
    - simulator를 켜면 미연결 모터 유지, startup pose, state broadcast, validator 입력이
      함께 얽히면서 `garbage angle` 또는 `잘못 믿을 current angle` 문제가 드러난다.
  - 이 TODO에 적는 이유:
    - 책임 코드는 `DrumRobot2`/`phil_robot` 쪽일 수 있어도,
      문제가 실제로 surfaced 되는 트리거가 `drum_intheloop` 시뮬레이터 실행이기 때문이다.
  - 확인할 것:
    - simulator ON/OFF에서 `current_angles` 스냅샷 차이
    - startup preset이 state 해석에 주는 영향
    - planner resolved command와 validator reject reason의 차이
  - 목표:
    - `simulator 때문에 생긴 것`과 `원래부터 있던 state 문제`를 분리한다.
  - **close-loop vcan 완료 후 이 blocker가 자동 해소될 가능성이 높다.**

## Timing Facts To Preserve

- `DrumRobot2` send loop 주기는 `1ms`다.
- `TMotor` command 소비/송신은 `5ms` 주기다.
- `Maxon` command 송신은 `1ms` 주기다.
- `DXL`은 현재 `cycleCounter == 0` 경로라 `5ms` 주기다.
- CAN receive loop는 `100us` 주기다.
- 현재 `drum_intheloop` backend는 timing 없는 `resetJointState()` 기반이다.
- 발 두 개를 제외하고 현재 cadence를 그대로 보면, `5ms` 창에서 대략
  `TMotor 7회 + Maxon 10회 + DXL 2개 joint target = 19개 actuator update`
  수준을 의식해야 한다.

## Later

- [ ] PyBullet 쪽 timestamped replay 실험
  - 현재 즉시 적용형 backend에서 한 단계 나아가, 최소한의 시간축 재생을 시험한다.

- [ ] command-level pipe와 frame-level ingress를 공존시키는 구조 검토
  - 빠른 시각 검증 경로와 더 높은 fidelity 경로를 분리할지 검토한다.

- [ ] frame-accurate SIL에서 필요한 state/schema 목록 정리
  - 단순 joint target 외에 frame id, send order, timestamp, feedback validity가
    얼마나 필요한지 미리 정리한다.
