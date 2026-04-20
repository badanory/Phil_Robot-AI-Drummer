# 드럼 로봇 연주 중 일시정지/재개 시 멈춤(Hang) 현상 해결 플랜

## 문제 원인 (막힌 상태가 되는 이유)

분석 결과, 로봇이 일시정지 후 재개할 때 아무 동작도 하지 못하고 "막힌 상태"에 빠지는 가장 큰 이유는 **스레드 간 메모리 충돌(Race Condition)**과 **내부 상태값 꼬임** 때문입니다.

1. **명령 버퍼(commandBuffer)의 스레드 충돌:**
   - 메인 스레드에서 "멈춰(pause)" 명령을 받으면 `clearCommandBuffers()`를 호출해 모든 모터의 `commandBuffer`를 즉시 비웁니다.
   - 하지만 이와 동시에 1ms 주기로 동작하는 **송신 스레드(sendLoopForThread / setCANFrame)**가 이 버퍼에서 데이터를 읽고(`empty()`, `pop()`) 있습니다.
   - 두 스레드가 자물쇠(`bufferMutex`) 없이 동시에 `std::queue`를 건드리면 큐가 심각하게 오작동하여 무한 대기나 교착 상태에 빠지게 됩니다.

2. **재개(resume) 시 완료 플래그 초기화 누락:**
   - 버퍼를 모두 비운 후 재개하면, 처음 한 마디를 생성하자마자 큐가 비어버려 `pathManager.endOfPlayCommand = true`가 조기에 활성화됩니다.
   - 재개 시 이 변수를 다시 `false`로 돌려놓지 않아 파일이 넘어갈 때 흐름이 꼬이는 현상이 발생합니다.

## 제안하는 수정 방향

이 문제를 완벽히 해결하기 위해 다음 부분들을 수정하고자 합니다.

### 1. `bufferMutex`를 이용한 버퍼 스레드 락(Lock) 적용
- 모터 버퍼에 접근하는 **모든 곳**에 `std::lock_guard<std::mutex> lock(motor->bufferMutex);`를 씌웁니다.
- **수정 파일:** 
  - `DrumRobot2/src/PathManager.cpp` (`clearCommandBuffers`, `pushCommandBuffer` 내부)
  - `DrumRobot2/src/CanManager.cpp` (`setCANFrame` 내부 `pop()` 부분)

### 2. Resume 시 내부 상태 플래그 초기화
- 일시정지 후 재개(`is_resuming`) 시 `pathManager.endOfPlayCommand = false;`를 명시적으로 초기화하여 조기 종료를 막습니다.
- **수정 파일:**
  - `DrumRobot2/src/DrumRobot.cpp` (`runPlayProcess` 내부)

## User Review Required

> [!IMPORTANT]
> 스레드 락(Lock)을 1ms 주기의 송신 스레드인 `setCANFrame`에 씌우면 아주 미세한 지연이 생길 수 있으나, 현재 버퍼 처리 속도상 모터 움직임에 영향을 줄 정도는 아닙니다. 이 구조로 수정하는 것에 동의하시나요? 동의하신다면 바로 작업을 진행하겠습니다.
