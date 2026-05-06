# 일시정지 및 재개(Pause/Resume) 시 IK 역운동학 에러 해결 과정

이 문서는 드럼 연주 로봇(Phil) 프로젝트에서 연주 중 **일시정지(pause) 후 재개(resume) 시 궤적이 꺾이거나 `IKFUN is not solved!!` 에러와 함께 로봇 팔이 꼬이는 현상**을 해결한 과정을 기록한 기술 문서입니다. 

---

## 1. 문제 증상 (Symptom)
* 연주 중 `pause` 명령을 내리면 로봇이 즉시 멈추는 데는 성공함.
* 하지만 이어서 `resume` 명령을 내리면, 로봇이 연주를 부드럽게 이어나가지 못하고 팔이 비정상적으로 꺾임.
* C++ 컨트롤러 터미널(`DrumRobot2`)에 무수히 많은 `IKFUN is not solved!! (sqrt)`, `Error : No solution (IK is not solved!!)` 에러가 발생하며 통신이 단절되거나 로봇이 동작을 거부함.

---

## 2. 근본 원인 분석 (Root Cause)
본 문제는 **"C++ 컨트롤러(PathManager)가 기억하는 목표 궤적"과 "실제 물리적 로봇 팔의 현재 위치" 간의 괴리(Mismatch)** 때문에 발생했습니다.

### (1) 일시정지(Pause) 시점의 동작
연주 중 `pause` 명령이 수신되면 C++의 `pathManager.clearCommandBuffers()`가 호출됩니다. 
이 함수는 CAN 통신용 명령 버퍼를 즉시 비우게 되며, **로봇의 팔은 허공(물리적 중간 지점)에 그대로 멈춥니다.**
하지만 이때 C++ 내부의 궤적 생성기(`PathManager`)는 자신이 마지막으로 생성했던 마디의 **끝점 목표 위치(예: `measureStateR`, 허리 각도 등)**를 여전히 '자신의 현재 상태'로 기억하고 있습니다.

### (2) 재개(Resume) 시점의 동작 (수정 전)
재개 명령이 수신되면 C++은 새로운 궤적(`measureMatrix`)을 읽어들인 뒤, 궤적 생성을 시작합니다.
이때 **궤적의 출발점을 '허공에 멈춰있는 물리적 위치'가 아니라, '아까 기억해둔 이전 마디의 끝점(기억 속 목표 위치)'으로 상정**하고 역운동학(IK) 수식을 전개합니다.

### (3) 에러 발생 (IKFUN is not solved!!)
결과적으로 C++은 "기억 속의 자세 -> 다음 타격 지점"으로 가는 궤적을 만들어 CAN 프레임으로 쏴줍니다. 
하지만 실제 모터의 `current angle`과 CAN 프레임의 첫 번째 `target angle` 사이의 격차가 물리적으로 불가능할 정도로 크기 때문에:
1. `CanManager::safetyCheckSendT()`의 `POS_DIFF_LIMIT` 안전 검사에 걸려 차단되거나,
2. 너무 먼 거리를 0.005초(dt) 만에 이동하려는 말도 안 되는 궤적이 나와 IK 연산 루트(sqrt) 안의 값이 음수가 되어 `IKFUN is not solved!!` 에러를 뱉게 됩니다.

---

## 3. 해결 방안 (Solution)
이 딜레마를 해결하려면 **재개 시점에 뇌(C++)와 몸(물리적 로봇)의 싱크를 다시 맞춰주는 과정**이 필요했습니다. `DrumRobot.cpp`의 `runPlayProcess()` 내 `is_resuming` 블록을 다음과 같이 개선했습니다.

### 수정된 로직 (`is_resuming`)
```cpp
    if (is_resuming)
    {
        is_resuming = false;
        endOfScore = false;
        
        // 1. C++ 궤적 생성기의 내부 상태를 Snare(기본 대기) 위치로 초기화
        pathManager.initPlayStateValue();
        
        // 2. 현재 허공에 멈춰있는 물리적 로봇 팔을 기본 Ready 자세로 부드럽게 이동
        cout << ">>> [Auto] 연주 재개를 위해 Ready 자세로 부드럽게 복귀합니다..." << endl;
        flagObj.setAddStanceFlag(FlagClass::READY);
        runAddStanceProcess(); 
        
        // 3. 내부 상태와 물리적 상태가 동기화되었으므로 연주 궤적 루프 시작
        pathManager.startOfPlay = true;
        // ...
    }
```

### 개선 원리
1. **`pathManager.initPlayStateValue()`**: 
   C++이 기억하던 이전 궤적의 잔재를 깨끗이 지우고, 다음 궤적의 출발점을 기본 대기 자세(SN, Snare)로 리셋합니다.
2. **`runAddStanceProcess()`**: 
   현재 허공에 멈춰 있는 로봇 팔의 실제 각도에서부터, C++이 새 출발점으로 삼은 `READY` 자세까지 부드럽게 이어지는 복귀 궤적을 생성하여 이동시킵니다.
3. 이 두 가지가 완료되면 로봇은 물리적으로나 소프트웨어적으로 완벽한 `READY` 상태가 됩니다. 이후 파일에서 읽어들인 다음 마디의 궤적(예: Ready -> 하이햇)을 매끄럽게 이어나갈 수 있습니다.

---

## 4. 결론 및 향후 과제
* **결과**: 이 수정을 통해 시뮬레이터(SIL) 및 실물 로봇 환경 모두에서 연주 일시정지 후 재개 시 발생하던 궤적 꺾임 현상과 통신 단절, IK 연산 오류가 완벽히 해결되었습니다.
---

## 4. 한계점 및 추가 개선 필요 사항 (Limitations & Future Work)

### (1) 단일 파일 악보(Single-File Score)에서의 재개 한계
현재 로봇의 일시정지/재개 아키텍처는 **점진적 파일 기반 실행(Incremental File-Based Execution)**에 최적화되어 있습니다. 악보가 `FIS_div0.txt` ~ `FIS_div3.txt`처럼 여러 조각으로 나뉘어 있는 경우, `play_file_index`를 통해 멈춘 마디의 첫 부분부터 정확히 다시 시작할 수 있습니다.
* **한계**: 하지만 `TIM0.txt` 처럼 14KB짜리 단일 파일에 곡 전체가 들어있는 경우, 재개 시 `TIM0.txt`를 다시 처음부터 열어 읽게 되므로 **사실상 곡 전체를 처음부터 다시 연주**하게 되는 한계가 있습니다.
* **개선 방향**: 진정한 마디/박자 단위의 이어 치기를 구현하려면, 악보 파일을 일괄적으로 작게 쪼개거나, C++ 내에서 파일 포인터(`tellg()`) 또는 현재 읽고 있던 `lineOfScore`를 저장해 두었다가 `resume` 시 해당 줄로 점프(Seek)하는 기능을 추가해야 합니다.

### (2) 목 관절(Dynamixel) 피드백 부재로 인한 부자연스러운 움직임
재개 시 로봇 팔은 CAN 통신을 통한 피드백 기반 위치 제어로 부드럽게 `Ready` 자세로 돌아가지만, **목 관절(DXL 모터) 부분은 준비 자세로 갈 때 이상하게 꺾이거나 튀는 현상**이 나타납니다.
* **원인**: Dynamixel(DXL) 모터는 현재 C++ `PathManager`에서 실시간 피드백(Current Angle)을 받지 않고, 단순히 목표 위치(Position)와 도달 시간(Time)만 하위 제어기로 던져주는 개방형 루프(Open-loop)에 가깝게 동작합니다. 일시정지 시 허공에 멈췄던 목 관절이, 현재 위치에 대한 고려 없이 맹목적으로 초기화 명령을 받다 보니 보간(Interpolation)이 매끄럽지 않게 튀는 것입니다.
* **개선 방향**: DXL 모터 제어 시에도 현재 실측 각도를 읽어와서(`feedback`), 현재 위치부터 목표 위치까지의 부드러운 스플라인 궤적을 C++ 단에서 직접 생성해 주거나, DXL 컨트롤러의 Profile Velocity/Acceleration 파라미터를 적절히 튜닝해야 합니다.
