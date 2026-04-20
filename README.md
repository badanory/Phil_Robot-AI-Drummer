# Phil Robot / Drum Robot Project

Jetson AGX Orin 기반의 로컬 AI brain, C++ 로봇 제어기, command-level SIL viewer를 한 저장소에 모아 둔 드럼 로봇 프로젝트입니다.

![Phil Robot](./docs/DrumRobot.jpg)

현재 저장소의 공식 표현은 아래 흐름입니다.

```text
LLM 플래너 -> 로봇 제어기 -> command-level 시뮬레이터
```

- `phil_robot/`는 Whisper STT, Ollama 기반 Qwen classifier/planner, MeloTTS를 묶은 Python brain입니다.
- `DrumRobot2/`는 CAN/TMotor/Maxon/DXL 제어와 상태머신을 담당하는 C++ body입니다.
- `Drum_intheloop/`는 `DrumRobot2`의 command-level 출력을 `/tmp/drum_command.pipe`로 받아 PyBullet에 적용하는 느슨한 SIL 경로입니다.
- `legacy/phil_intheloop/`는 이전 simulator 경로를 보관하는 legacy 영역입니다.

## 현재 구조

```text
robot_project/
├── DrumRobot2/          # 실시간 C++ 로봇 제어기
├── Drum_intheloop/      # named pipe 기반 PyBullet SIL
├── phil_robot/          # Python LLM brain / STT / TTS / eval
├── DrumRobot_data/      # 저장/생성 데이터와 코드 산출물
├── docs/                # 루트 문서 자산
├── legacy/              # 이전 자산 보관
└── log.md               # 작업 로그
```

### `DrumRobot2/`

- `src/main.cpp`: 제어기 엔트리포인트입니다. `initializeDrumRobot()`가 끝난 뒤에야 state/send/recv/music/python/broadcast thread를 시작합니다.
- `src/DrumRobot.cpp`: 로봇 상태머신, brain 대기, 초기 자세, state JSON broadcast, Magenta 연동 경로를 담당합니다.
- `src/AgentSocket.cpp`: TCP server를 열고 brain 명령 큐와 state JSON 송신을 처리합니다.
- `src/CanManager.cpp`: CAN 포트 초기화, 하드웨어 송수신, `DRUM_SIL_MODE` 판별을 담당합니다.
- `src/SilCommandPipeWriter.cpp`: SIL 모드일 때 command-level NDJSON을 `/tmp/drum_command.pipe`로 내보냅니다.
- `include/codes/*.txt`: 현재 드럼 악보 txt 파일 위치입니다. 첫 줄은 `bpm <number>` 형식이며, 이후 줄은 상대 대기시간과 손/발 악기 번호, velocity를 담습니다.

### `phil_robot/`

- `phil_brain.py`: 마이크 녹음, Whisper STT, 상태 스냅샷 조회, LLM 턴 실행, 검증된 명령 전송, TTS 재생을 묶는 메인 엔트리포인트입니다.
- `pipeline/`: classifier, planner, validator, skill expansion, motion resolver 같은 판단 계층입니다.
  - **LangGraph 기반 상태 기계(State Machine)**: `process → execute → return_home` 구조로 설계되어 비동기 실행 및 동작 간 전이를 유연하게 관리합니다.
  - **InterruptibleExecutor**: 백그라운드 스레드에서 로봇 명령을 실행하며, Enter 키 입력 시 이전 동작을 즉각 중단하고 새 명령을 처리합니다.
- `runtime/`: TCP client와 MeloTTS 같은 런타임 계층입니다.
- `eval/`: smoke case와 오프라인 평가 러너가 있습니다.
- `init_phil.sh`: `jetson_clocks`와 Ollama keep-alive를 이용해 Jetson 런타임을 예열합니다.

현재 기본 모델 설정은 아래 파일에 있습니다.

- classifier: `qwen3:4b-instruct-2507-q4_K_M`
- planner: `qwen3:30b-a3b-instruct-2507-q4_K_M`

### `Drum_intheloop/`

- `run_sil.py`: SIL reader 진입점입니다.
- `sil/SilCommandPipeReader.py`: FIFO 생성/삭제, NDJSON parse, startup pose 적용을 담당합니다.
- `sil/command_applier.py`: production motor 이름과 각도 의미를 URDF joint target으로 바꿉니다.
- `sil/joint_map.py`: CAN joint 의미 보정을 정의합니다.
- `sil/urdf_tools.py`: 체크인된 URDF/STL 원본을 건드리지 않고 runtime URDF patch를 적용합니다.
- 현재 backend는 `resetJointState()` 기반 즉시 반영 viewer에 가깝고, frame-accurate simulator는 아닙니다.

## 빠른 시작

### 1. Python brain 환경

```bash
cd /home/shy/robot_project/phil_robot
conda env create -f environment.yml
conda activate drum4
```

추가로 Ollama 서버와 `phil_robot/config.py`에 적힌 Qwen 모델이 준비되어 있어야 합니다. `init_phil.sh`는 Jetson 환경에서 `jetson_clocks`와 Ollama keep-alive를 실행합니다.

### 2. C++ 제어기 빌드

```bash
cd /home/shy/robot_project/DrumRobot2
make clean
make
```

`Makefile`은 `opencv4`, `sfml`, `realsense2`, USBIO 라이브러리, Dynamixel 라이브러리가 있는 Jetson/Ubuntu 계열 환경을 전제로 합니다.

실행은 상대경로 의존성 때문에 `DrumRobot2/bin` 기준으로 하는 편이 안전합니다. CSV 로그는 `../../DrumRobot_data/`, 악보/음원/마젠타 산출물은 `../include/...`, `../magenta/...`를 전제로 합니다.

## 실행 모드

### 하드웨어 + brain

터미널 1:

```bash
cd /home/shy/robot_project/DrumRobot2/bin
sudo ./main.out
```

터미널 2:

```bash
cd /home/shy/robot_project/phil_robot
conda activate drum4
./init_phil.sh
python phil_brain.py
```

주의:

- `DrumRobot2`는 brain TCP 연결이 성공할 때까지 `initializeDrumRobot()` 안에서 대기합니다.
- brain이 연결되면 C++ 쪽이 내부적으로 `initializePos("o")`를 호출해 초기 자세 절차를 진행합니다.
- 그 다음에도 안전 키를 제거하고 C++ 터미널에서 `k`를 입력해 gate를 열기 전까지 명령은 폐기될 수 있습니다.
- state broadcast는 TCP `9999` 경로이며, SIL pipe와는 별개입니다.

### command-level SIL

터미널 1:

```bash
cd /home/shy/robot_project/Drum_intheloop
python -m pip install -r requirements.txt
python run_sil.py --mode gui
```

터미널 2:

```bash
cd /home/shy/robot_project/DrumRobot2/bin
sudo env DRUM_SIL_MODE=1 ./main.out
```

터미널 3:

```bash
cd /home/shy/robot_project/phil_robot
conda activate drum4
python phil_brain.py
```

중요:

- `Drum_intheloop` reader가 `/tmp/drum_command.pipe`를 만들고 종료 시 정리합니다.
- `DrumRobot2` writer는 FIFO를 만들지 않으며, `DRUM_SIL_MODE=1`일 때만 export를 시도합니다.
- reader를 먼저 띄워야 writer가 pipe를 열 수 있습니다.
- TCP brain 연결이 별도로 필요하므로, pipe reader만 실행해도 `main.out`의 전체 동작이 자동으로 시작되지는 않습니다.

## 평가와 보조 문서

- 오프라인 LLM 평가: `python phil_robot/eval/run_eval.py --suite smoke`
- Python brain 구조 문서: `phil_robot/docs/PROJECT_STRUCTURE_KR.md`
- LLM 파이프라인 문서: `phil_robot/docs/LLM_PIPELINE_ARCHITECTURE_KR.md`
- SIL 상세 문서: `Drum_intheloop/README.md`

## 재생 제어 및 안전 메커니즘 (Pause/Resume/Stop)

현재 로봇의 모터 제어 및 악보 재생은 전역 궤적 버퍼링(Global Trajectory Buffering)을 배제하고 **점진적 파일 기반 실행(Incremental File-Based Execution)** 모델을 사용합니다.

- 긴급 정지 및 일시정지(`s` 명령): 명령 수신 시 `std::mutex`로 보호되는 각 모터의 pending 버퍼(`PathManager`)와 `TestManager` 큐를 즉각 플러시하여 로봇이 추가 동작 없이 즉시 멈춥니다.
- 재생 재개(`p` 명령): 정지된 위치를 내부적으로 추적하여, 재시작 시 음악의 처음이 아닌 중단된 마디/위치부터 이어서(Resume) 재생합니다.

## 현재 문서화해 둘 제한 사항

- `main.cpp`는 `initializeDrumRobot()`가 끝난 뒤에야 주요 thread를 시작하므로, brain TCP 연결이 안 되면 이후 경로가 함께 지연될 수 있습니다.
- `openCSVFile()`은 startup 초기에 메타데이터를 기록하므로, CSV가 생겼다고 body trajectory 생성까지 성공한 것은 아닙니다.
- `legacy/phil_intheloop/`는 현재 공식 경로가 아니라 이전 자산 보관용입니다.

## 참고 이미지

- SIL GUI 예시: [Simulation_intheloop.png](./Drum_intheloop/artifacts/Simulation_intheloop.png)
- SIL GUI 예시 2: [SIL2.png](./Drum_intheloop/artifacts/SIL2.png)
