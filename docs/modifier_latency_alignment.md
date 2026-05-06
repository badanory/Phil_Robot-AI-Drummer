# Modifier Latency 정렬: velocity_delta를 IK 경계로 이동

## 1. 문제: stop과 modifier의 latency 불일치

이전 구현에서 두 명령의 적용 시점이 달랐다.

| 명령 | 적용 위치 | 통과 버퍼 | 지연 |
|---|---|---|---|
| `stop` | `PathManager::solveIKandPushCommand()` 진입부 | motor buffer (2줄) | ~1.2s |
| `velocity_delta` modifier | `DrumRobot::readMeasure()` 파싱 단계 | task-space queue (3줄) + motor buffer (2줄) | ~3s |

`stop`과 `velocity_delta`를 동시에 보내면 stop이 먼저 도달하고, modifier는 3초 뒤 도달한다.  
결과: **감속 없이 원래 속도로 치다가 급정거**.

## 2. 파이프라인 구조 (실측 기반)

```
readMeasure()        processLine()         solveIKandPushCommand()     sendLoopForThread()
[txt 파싱]    →   [genTrajectory]   →   [IK + commandBuffer push]  →  [5ms 모터 송신]
                waistParameterQueue         motor commandBuffer
                 depth: preCreatedLine=3     depth: MAX_MEASURE_BUFFER=2
                    (~1.8s)                       (~1.2s)
```

- 1줄(line) ≈ 0.6초 (100 BPM 기준 8분음표)
- `lineOfScore`는 line 단위로 증가 (measure 단위 아님)
- 총 파이프라인 깊이: 3줄 + 2줄 = **5줄 ≈ 3초**

## 3. 변경 내용

### 3.1 velocity_delta → IK 경계에서 kpRatio 스케일링

**PathManager.hpp**: `pending_vel_scale` / `active_vel_scale` 추가.

**PathManager::solveIKandPushCommand()**: `cv_full.wait()` 직후 pending을 commit하고 kpRatio에 적용.

```cpp
// cv_full.wait() 직후 — stop 플래그 체크와 동일한 지점
active_vel_scale = pending_vel_scale;

for (int i = 0; i < n; i++) {
    VectorXd q = getJointAngles(q0, KpRatioR, KpRatioL);

    // velocity modifier: kpRatio 스케일링 (궤적 형태 불변, 타격력만 조정)
    KpRatioR *= active_vel_scale;
    KpRatioL *= active_vel_scale;

    pushCommandBuffer(q, KpRatioR, KpRatioL, is_measure_end, send_last_flag);
}
```

**DrumRobot::checkPlayInterrupts()**: velocity_delta를 vel_scale로 변환해 PathManager에 전달.

```cpp
double vel_scale = std::max(0.1, 1.0 + static_cast<double>(active_modifier.velocity_delta) / 10.0);
pathManager.pending_vel_scale = vel_scale;
```

변환 공식 `1.0 + delta / 10.0`:
- delta = 0   → scale = 1.0 (변화 없음)
- delta = +5  → scale = 1.5 (Kp 1.5배, 더 강한 타격)
- delta = -5  → scale = 0.5 (Kp 절반, 부드러운 타격)

**DrumRobot::readMeasure()**: `applyVelocityDelta()` 호출 제거 (velocity 컬럼 4, 5 raw 값 유지).

### 3.2 수정된 latency

| 명령 | 이전 | 이후 |
|---|---|---|
| `stop` | ~1.2s (motor buffer) | ~1.2s (변화 없음) |
| `velocity_delta` | ~3s (full pipeline) | ~1.2s (motor buffer 만 통과) |
| `tempo_scale` | ~3s (full pipeline) | ~3s (변화 없음 — 아래 제약 참고) |

### 3.3 함께 수정된 pre-existing 버그

- **`pushCommandBuffer` 시그니처**: `is_last_measure` 하나였던 bool 파라미터를 `is_measure_end` / `is_last_measure` 두 개로 분리.  
  - 이전: `is_measure_end = is_last_measure` (잘못된 동작)
  - 이후: 두 의미가 명확히 분리됨
- **`pathManager.Kp`**: `PathManager::pushCommandBuffer()` 내부에서 `pathManager.Kp` → `Kp` (self 참조 수정)
- **`setCANFrame`의 `pathManager`/`state` 직접 참조**: CanManager가 PathManager와 circular dependency 없이는 접근 불가.  
  bounded buffer 로직(`cv_empty.wait`, `measure_count--`, `cv_full.notify_one`, `state.main = Ideal`)을 전부 `sendLoopForThread()`로 이동.  
  `setCANFrame`은 `bool &out_measure_ended, bool &out_song_ended` 출력 파라미터만 반환.

## 4. 제약 사항 (tempo_scale)

`tempo_scale`은 **readMeasure에 머물러야** 한다.

이유: tempo_scale은 `bpmOfScore`에 적용되고, bpmOfScore는 악보 줄의 절대 시간(column 8, `measureTotalTime`)을 계산하는 데 사용된다. 이 시간 값은 `genTrajectory`가 모션 프로파일의 n(5ms 샘플 수)을 결정하는 데 필수다. IK 단계에서는 이미 시간 이산화가 끝난 뒤라 변경 불가.

따라서 tempo_scale과 stop은 여전히 latency가 다르다(~3s vs ~1.2s). stop과 동시에 tempo를 바꾸는 사용 시나리오는 현재 구조로는 완전한 동기화가 어렵다.

## 5. 적용 범위

velocity_delta가 kpRatio를 통해 영향을 주는 범위:
- **CST 모드(Maxon)**: `newData.kp = Kp * kpRatioR` → 직접 반영됨
- **CSP 모드**: kp = 0으로 고정되어 kpRatio 영향 없음
- **TMotor(position/velocity 모드)**: kpRatio는 내부 PD 파라미터에 미반영 (TMotor 자체 제어 루프 사용)

현재 메인 연주 모드(Maxon CSP)에서는 kpRatio 효과가 없다.  
velocity_delta는 **Maxon CST 모드**에서만 타격력 조절로 의미가 있다.  
CSP 모드에서 velocity 세기를 바꾸려면 trajectory 형태 자체를 readMeasure 단계에서 수정해야 하며, 이는 3초 latency를 감수해야 한다.

## 6. 파일 변경 목록

| 파일 | 변경 내용 |
|---|---|
| `DrumRobot2/include/managers/PathManager.hpp` | `pending_vel_scale`, `active_vel_scale` 필드 추가; `pushCommandBuffer` 시그니처 수정 |
| `DrumRobot2/include/managers/CanManager.hpp` | `setCANFrame` 시그니처에 출력 파라미터 추가 |
| `DrumRobot2/src/PathManager.cpp` | `solveIKandPushCommand`: vel_scale commit + kpRatio 적용; `pushCommandBuffer`: 시그니처 수정, `pathManager.Kp` → `Kp` 수정 |
| `DrumRobot2/src/CanManager.cpp` | `setCANFrame`: pathManager/state 참조 제거, 출력 파라미터로 대체 |
| `DrumRobot2/src/DrumRobot.cpp` | `checkPlayInterrupts`, `pauseStateRoutine`: vel_scale 계산 및 PathManager 전달; `readMeasure`: `applyVelocityDelta` 제거; `runPlayProcess`: 시작/재개 시 vel_scale 초기화; `sendLoopForThread`: bounded buffer 로직 이동 |
