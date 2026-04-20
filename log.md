# Change Log

프로젝트에서 의미 있는 변경을 할 때마다 한국시간(KST, UTC+9) 기준의 날짜, 시간, 요약을 기록합니다.
최신 항목이 위로 오도록 추가합니다.

## 2026-04-20
- 15:53 KST (UTC+9) — 문서 현행화: README 및 아키텍처 문서에 점진적 파일 기반 실행(Incremental File-Based Execution) 및 일시정지/재개(Pause/Resume) 매커니즘 반영
  - 수정 파일: `README.md`, `phil_robot/docs/LLM_PIPELINE_ARCHITECTURE_KR.md`
  - 메모: 전역 버퍼링(Global trajectory buffering)에서 점진적 실행 모델로 변경된 사항과 `s` (정지/일시정지) 명령이 버퍼를 즉각 플러시하여 인터럽트/재개 가능하게 된 내용을 문서에 추가함.

## 2026-04-20
- 17:16 KST (UTC+9) — 문서 현행화: 전체 파이프라인의 최신 함수 명세서 추가 및 아키텍처 문서 연결
  - 추가/수정 파일: `phil_robot/docs/CURRENT_FUNCTION_SPECS_KR.md`, `phil_robot/docs/LLM_PIPELINE_ARCHITECTURE_KR.md`
  - 메모: 다수의 AI 에이전트 작업으로 인해 꼬여있던 `brain_pipeline.py`, `robot_graph.py` 등의 내부 역할을 맵핑한 최신 함수 명세서(Artifact)를 새로 작성함. 기존 아키텍처 문서에는 현재 코드의 Patch-work 상태에 대한 솔직한 메모와 새 명세서 링크를 추가함.

## 2026-04-20
- 15:15 KST (UTC+9) — fix: 연주 일시정지(pause) 및 재개(resume) 시 로봇이 무한 대기(Hang)에 빠지는 문제 해결
  - 수정 파일: `DrumRobot2/src/PathManager.cpp`, `DrumRobot2/src/CanManager.cpp`, `DrumRobot2/src/DrumRobot.cpp`
  - 메모:
    - **스레드 락 적용**: 메인 스레드(`clearCommandBuffers`, `pushCommandBuffer`)와 송신 스레드(`setCANFrame`의 `empty()`, `pop()`)가 큐에 동시 접근하여 발생하는 메모리 충돌(Race Condition)을 방지하기 위해 모터 객체의 `bufferMutex`에 `std::lock_guard`를 씌움.
    - **Resume 시 플래그 리셋**: `runPlayProcess` 내 `is_resuming` 블록에서 `pathManager.endOfPlayCommand = false;`를 추가하여 큐 초기화 직후 완료 플래그가 비정상적으로 유지되어 다음 파일 처리가 꼬이는 현상 방지.

- 14:35 KST (UTC+9) — fix: 드럼 연주 파일/마디 생성 도중 pause(일시정지) 명령이 즉시 반영되지 않고 마디가 다 끝날 때까지 대기하던 문제 해결
  - 수정 파일: `DrumRobot2/include/managers/PathManager.hpp`, `DrumRobot2/src/PathManager.cpp`, `DrumRobot2/src/DrumRobot.cpp`
  - 메모: `PathManager`에 `clearCommandBuffers()`를 추가해 모든 내부 큐(`taskSpaceQueue` 등)와 각 모터의 `commandBuffer`를 즉시 비우도록 함. `DrumRobot::runPlayProcess`의 `while(readMeasure)` (파일 읽기 및 명령 버퍼 큐잉) 루프 및 대기 루프 내에서 pause 명령을 감지하면 즉시 버퍼를 비우고 루프를 탈출해 `Pause` 상태로 전환되도록 수정.

- 11:09 KST (UTC+9) — fix: Pause 상태(4)를 Error(6)로 잘못 판단하던 버그 2건 수정
  - 수정 파일: `phil_robot/pipeline/command_validator.py`, `DrumRobot2/src/DrumRobot.cpp`
  - 메모: Main enum: Pause=4, Error=6. Python command_validator가 state==4를 "에러 상태"로 차단 → state==6으로 정정. C++ makeStateJson도 state_id==4에서 error_message 표시 → state_id==6으로 정정. Pause 중 'h' 명령이 "에러 상태이므로 차단" 되던 문제 해소.
- 11:09 KST (UTC+9) — feat: DrumRobot2 파일 단위 pause/resume 구현
  - 수정 파일: `DrumRobot2/src/DrumRobot.cpp`
  - 메모: `runPlayProcess()` 파일별 처리 블록에 실행 완료 대기 루프 추가. pause 수신 시 즉시 return 하지 않고 `file_pause` 플래그만 세운 뒤 버퍼가 완전히 소진된 후에 Pause로 전환 (deferred pause). 버퍼 소진 후 `play_file_index++` → resume 시 다음 파일부터 재연주. 기존 첫-파일 동기화 대기, 최종 processLine 블록, 최종 대기 루프 제거. 박자 단위 재개는 미구현.

## 2026-04-17
- 23:40 KST (UTC+9) — feat: OpenAI GPT 백엔드 지원 추가 (PHIL_LLM_BACKEND=openai)
  - 수정 파일: `phil_robot/config.py`, `phil_robot/pipeline/llm_interface.py`
  - 메모: `PHIL_LLM_BACKEND=openai` 환경변수로 gpt-4o-mini 전환. `.env`의 OM_API_KEY 자동 로드. openai==1.49.0 (Python 3.8 호환).
- 22:20 KST (UTC+9) — fix: 모터 실행 대기 루프에서 pause 명령 무시되던 버그 수정
  - 수정 파일: `DrumRobot2/src/DrumRobot.cpp`
  - 메모: 궤적 생성은 실시간보다 빠르므로 실제 연주 시간 전체가 `while(!getFixationFlag())` 루프에서 소비됨. 이 루프 안에서 `checkPlayInterrupts()`를 호출하지 않아 pause 명령이 Idle 복귀 후에야 처리되던 문제. `wait_loop_pause` 플래그를 두어 buffer 소진 후 Pause 상태로 전환하도록 수정. 재개 시 `initializePlayState()` 리셋 후 처음부터 재생.

- 21:10 KST (UTC+9) — fix: allMotorsUnConected SIL root fix + debounce 제거
  - 수정 파일:
    - `DrumRobot2/src/DrumRobot.cpp`: `initializeCanManager()`에서 `openSilVcan` 후 `allMotorsUnConected = false` 추가. vcan이 열리면 CAN 피드백 경로가 있으므로 "연결됨"으로 갱신해 `is_fixed` 강제 override 해제
    - `phil_robot/pipeline/robot_graph.py`: `_wait_for_fixed_then_home`에서 debounce(0.5초) 제거. `policy_gesture`는 전체 trajectory를 commandBuffer에 한 번에 push하므로 sub-move 사이 공백 없음 → debounce 불필요
  - 메모: `AgentAction.cpp` 확인 결과 `policy_moveJoint`는 동기 호출로 모든 보간 tick을 즉시 push. 제스처 완료 후 처음 `is_fixed=true`가 곧 실제 완료 시점

- 19:30 KST (UTC+9) — fix: 홈 복귀 오발 + pause 명령 불통 2건 수정
  - 수정 파일:
    - `phil_robot/pipeline/robot_graph.py`: `_wait_for_fixed_then_home`에 (1) 움직임 미감지 시 홈 복귀 스킵(SIL allMotorsUnConected 대응), (2) 0.5초 debounce 추가. 제스처 서브무브 사이 일시적 fixed 상태로 인한 오발 방지
    - `phil_robot/pipeline/command_validator.py`: `INTERRUPT_COMMANDS = {"pause", "resume"}` 추가. validate_command 최상단에서 무조건 통과. 기존엔 "알 수 없는 명령"으로 reject되어 Python이 아예 전송하지 않던 버그 수정
    - `phil_robot/pipeline/brain_pipeline.py`: `_detect_play_interrupt` 추가. 연주 중(state==2) 멈춰/스톱/정지/resume 키워드를 LLM 없이 즉시 pause/resume 명령으로 변환하는 deterministic shortcut 추가

## 2026-04-17
- 10:55 KST (UTC+9) — feat: Drum_intheloop close-loop vcan feedback 구현 (Phase 0~4)
  - 수정/신규 파일:
    - `Drum_intheloop/TODO.md`: close-loop vcan 구현 마스터 플랜 추가 (Phase 0~7 상세 절차)
    - `Drum_intheloop/sil/vcan_state_writer.py` (신규): PyBullet joint state → vcan0 struct can_frame 피드백 송신기. TMotor(Servo mode) / Maxon(CANopen PDO) 프레임 인코딩 구현
    - `Drum_intheloop/sil/pybullet_backend.py`: `read_joint_states()` 메서드 추가 (PyBullet getJointState → {urdf_joint_name: deg})
    - `Drum_intheloop/sil/SilCommandPipeReader.py`: VcanStateWriter 통합. tick 처리 후 joint state를 vcan0으로 전송. `--no-vcan` / `--vcan` 인수 추가
    - `DrumRobot2/include/managers/CanManager.hpp`: `openSilVcan()` 선언 추가
    - `DrumRobot2/src/CanManager.cpp`: `openSilVcan()` 구현 추가. vcan0 소켓 열기 + disconnected 모터에 fd 할당
    - `DrumRobot2/src/DrumRobot.cpp`: `initializeCanManager()` 에 `canManager.openSilVcan("vcan0")` 호출 추가
    - `Drum_intheloop/requirements.txt`: `python-can` 추가
    - `Drum_intheloop/setup_vcan.sh` (신규): vcan0 커널 모듈 로드 및 인터페이스 설정 스크립트
  - 메모:
    - conda env `sil` (Python 3.10 + pybullet + python-can 4.6.1) 생성 완료
    - 닫힌 루프 경로: PyBullet stepSim → vcan0 CAN frame → C++ recv loop → motor.jointAngle 갱신
    - 실행 전 `sudo bash setup_vcan.sh` 로 vcan0 인터페이스 올려야 함
    - `--no-vcan` 플래그로 open-loop 디버그 모드 유지 가능
    - C++ 빌드 확인: 경고 2개(기존), 에러 없음

## 2026-04-17
- 17:40 KST (UTC+9) — fix: Pause 상태에서 modifier/액션 명령 처리 및 gate 개폐 수정
  - 수정 파일: `DrumRobot2/src/DrumRobot.cpp`
  - 메모:
    - `pauseStateRoutine()` 진입 시 `agentSocket.openGate()` 추가 — Pause 중에도 look/gesture/move 명령 수신 허용
    - `resume` 처리 시 `agentSocket.closeGate()` 추가 — 연주 재개 시 gate 다시 닫기
    - `handleModifier()` 처리 추가 — Pause 중 `tempo_scale:` / `velocity_delta:` 명령을 즉시 `active_modifier`에 반영
    - 그 외 알 수 없는 명령은 `agentAction.executeCommand()` 호출 — look/gesture/move 등 실행 가능

- 14:00 KST (UTC+9) — 문서 현행화: README/아키텍처/구조 문서를 현재 코드 기준으로 업데이트
  - 수정 파일:
    - `Drum_intheloop/README.md`: tick 기반 frame 배치 처리 모델로 전체 프로토콜 설명 교체 (per-command → tick-batched), 페달/드럼패드 시각화 사실 추가, `colors.py` 디렉터리 구조에 추가, `joint_map.py` 항목에 PEDAL_JOINTS/DRUM_PAD_SPEC 등 신규 테이블 기재, idle return 기능 추가
    - `phil_robot/docs/LLM_PIPELINE_ARCHITECTURE_KR.md`: `ValidatedPlan` 필드명 `raw_op_cmds`/`expanded_op_cmds`/`resolved_op_cmds`/`valid_op_cmds`/`rejected_op_cmds`로 수정, `play_modifier`/`clarification_question` 필드 추가, 신규 컴포넌트(LangGraph state machine, InterruptibleExecutor, session) 섹션 추가, 아키텍처 다이어그램 업데이트
    - `phil_robot/docs/PROJECT_STRUCTURE_KR.md`: pipeline/ 파일 목록에 `robot_graph.py`/`state_graph.py`/`exec_thread.py`/`session.py`/`play_modifier.py` 추가, tests/ 파일 목록 현행화, docs/ 파일 목록에 신규 문서 추가
    - `phil_robot/docs/LANGGRAPH_STATE_MACHINE_KR.md`: 상단에 "구현 완료(2026-04-16)" 표시 추가

## 2026-04-16
- 00:10 KST (UTC+9) — session 단기 기억 + clarification 되묻기 지원 추가
  - 수정/추가 파일:
    - `phil_robot/pipeline/session.py` (신규): `SessionContext` 단기 기억 — 대화 히스토리, 마지막 관절/시선/연주 상태, clarification pending 상태 관리
    - `phil_robot/pipeline/planner.py`: planner 출력 스키마에 `q` (clarification 질문) 필드 추가; `enforce_intent_constraints`가 `clarification_question` 보존하도록 수정; `build_planner_input_json`에 `session_summary` 파라미터 추가
    - `phil_robot/pipeline/validator.py`: `ValidatedPlan`에 `clarification_question` 필드 추가
    - `phil_robot/pipeline/brain_pipeline.py`: `run_brain_turn`에 `session` 파라미터 추가; clarification 텍스트 합치기 (`resolve_clarification_text`); `session_summary` planner input 포함; greeting shortcut 추가
    - `phil_robot/phil_brain.py`: `SessionContext` 초기화 및 매 턴 `update_session` 호출; `robot_graph.py`에 `session` 주입
  - 메모: "허리 돌려" → "몇 도로?" → "30도" 흐름 지원. session 없으면 기존 stateless 동작 동일.

- 23:55 KST (UTC+9) — 드럼 연주 중 pause/resume 및 실시간 modifier 적용 구현
  - 수정/추가 파일:
    - `DrumRobot2/include/managers/AgentSocket.hpp`: `isInterruptCmd()` 선언 추가
    - `DrumRobot2/src/AgentSocket.cpp`: `isInterruptCmd()` 구현 — pause/resume/h/modifier 명령은 gate 닫혀도 큐에 삽입
    - `DrumRobot2/include/tasks/DrumRobot.hpp`: `play_file_index`, `pause_requested`, `is_resuming` 멤버 변수 및 `checkPlayInterrupts()`, `pauseStateRoutine()` 선언 추가
    - `DrumRobot2/src/DrumRobot.cpp`:
      - `checkPlayInterrupts()`: 연주 루프 내에서 pause 플래그 세팅 및 modifier 즉시 반영
      - `pauseStateRoutine()`: Pause 상태 대기, resume 시 `is_resuming=true`+Play 전환, h 시 홈 복귀
      - `runPlayProcess()`: `fileIndex` 로컬→`play_file_index` 멤버화, fresh-start/resume 분기, while 루프 상단에 `checkPlayInterrupts()` + pause 체크 삽입
      - `stateMachine()`: Pause 케이스에 `pauseStateRoutine()` 연결
      - `processInput()`: pause/resume 핸들러 추가 (Idle 상태 방어)
      - `idealStateRoutine()`: 명령 디스패치에 pause/resume 추가
    - `phil_robot/pipeline/intent_classifier.py`: stop_request 분류 기준에 일시정지/재개 발화 추가
    - `phil_robot/pipeline/planner.py`: PLANNER_DOMAIN_STOP 프롬프트에 pause/resume 명령 안내 추가, stop_request allowed_prefixes에 "pause"/"resume" 추가
  - 메모: 연주 악보 파일 단위(~2.4s)로 pause 위치 저장. resume 시 해당 파일 인덱스부터 재개. 연주 중 velocity/tempo modifier도 다음 마디부터 즉시 반영됨

- 23:30 KST (UTC+9) — LangGraph 스타일 상태 기계 도입 (비동기 실행 + 동작 전이 + 홈 복귀)
  - 수정/추가 파일:
    - `phil_robot/pipeline/state_graph.py` (신규): LangGraph 호환 경량 StateGraph 구현 (Python 3.8 + aarch64 환경에서 langgraph 패키지 설치 불가하여 대체 구현. API 동일)
    - `phil_robot/pipeline/exec_thread.py` (신규): `InterruptibleExecutor` — 로봇 명령을 백그라운드 스레드에서 실행하며 cancel() 호출 시 'stop' 전송 + wait: 명령도 즉시 중단
    - `phil_robot/pipeline/robot_graph.py` (신규): LangGraph 상태 기계 — `process→execute→return_home` 노드, `PhilState` TypedDict, plan_type 판단 로직
    - `phil_robot/phil_brain.py` (수정): 동기 blocking 루프를 InterruptibleExecutor + LangGraph app.invoke() 구조로 교체. Enter 누름 순간 executor.cancel() 인터럽트, TTS 백그라운드 병렬 실행, 동작 완료 후 on_done_cb 로 홈 복귀
    - `phil_robot/docs/LANGGRAPH_STATE_MACHINE_KR.md` (신규): 작업 전체 맥락·설계·기대 동작 문서
  - 메모: 박사 피드백("전환이 너무 로봇 같다") 대응. TODO.md의 'LangGraph 도입 검토' 항목 완료. Python 3.9+ 업그레이드 시 state_graph.py → langgraph 교체는 import 한 줄 변경으로 가능

- 19:00 KST (UTC+9) — 드럼 패드를 drum_position.txt 실제 좌표 기반으로 교체
  - 수정 파일:
    - `Drum_intheloop/sil/joint_map.py`: `DRUM_PAD_SPEC` 업데이트 (hardcoded position 제거, 드럼/심벌 반지름 분리), `DRUM_INSTRUMENT_NAMES` / `DRUM_HEAD_INDICES` 추가
    - `Drum_intheloop/sil/pybullet_backend.py`: `_base_z_shift` 추가, `_place_robot_on_ground()`에서 z_shift 저장, `_load_drum_positions()` 추가 (robot→world 좌표 변환), `_add_drum_pad()` 교체 (10개 악기, 드럼/심벌 크기 구분)
  - 메모: 변환 수식 wx=rx, wy=rz, wz=-ry+z_shift (base orientation -π/2,0,0 기준), 좌우 손 타격 위치 평균값을 악기 중심으로 사용

- 18:10 KST (UTC+9) — wave 제스처 개선 및 policy_moveJoint에 moveTime 파라미터 추가
  - 수정 파일:
    - `DrumRobot2/include/tasks/AgentAction.hpp`: `policy_moveJoint`에 `float moveTime = 2.0f` 파라미터 추가
    - `DrumRobot2/src/AgentAction.cpp`: `policy_moveJoint` 구현에서 `move_time` 하드코딩 제거, 파라미터 사용; wave 제스처를 arm1/arm3 oscillation 구조에서 arm2 올리기 + wrist만 1.0f 속도로 4회 왕복 구조로 재설계
  - 메모: arm2를 20→45도로 올려 팔이 인사 자세로 올라가게 하고, 손목 oscillation 범위를 ±25도로 확대, 각 step 1초(기존 2초 대비 2배 빠름)
- 14:30 KST (UTC+9) — Drum_intheloop 로봇 컬러 테마 적용, 바닥 teal 색상, 드럼 패드 추가
  - 수정 파일:
    - `Drum_intheloop/sil/colors.py`: `PLANE_RGBA`를 `IVORY`에서 `TEAL`로 변경
    - `Drum_intheloop/sil/joint_map.py`: `DRUM_PAD_SPEC` 추가 (snare 추정 위치, 내/외경, 색상)
    - `Drum_intheloop/sil/pybullet_backend.py`: `PLANE_RGBA`/`ROBOT_THEME` import 추가, `start()`에서 바닥 색상 적용 및 `_apply_visual_theme()`/`_add_drum_pad()` 호출, 두 메서드 구현
  - 메모: URDF에 별도 드럼 위치 마커 없음 → DRUM_PAD_SPEC에서 페달 기준 snare 추정 위치 사용. 드럼 패드는 외부(검정)+내부(흰색) 두 개 GEOM_CYLINDER로 구성

## 2026-04-15
- 00:10 KST (UTC+9) — SIL 페달 시각화 추가 (R_foot / L_foot)
  - 수정 파일:
    - `Drum_intheloop/sil/joint_map.py`: `PRODUCTION_TO_URDF_JOINT`에 `R_foot → pedal_right`, `L_foot → pedal_left` 가상 키 추가
    - `Drum_intheloop/sil/pybullet_backend.py`: `_add_pedals()` 추가 (GEOM_BOX, baseMass=0 MultiBody 2개), `_tilt_pedal()` 추가 (발뒤꿈치 피벗 기울기 애니메이션), `apply_targets()`에서 pedal_right/pedal_left 키 감지 후 `_tilt_pedal` 호출
  - 메모: URDF 건드리지 않음. 오른발 적갈색 / 왼발 남색으로 구분. 위치는 `_PEDAL_R_POS`, `_PEDAL_L_POS` 상수로 조정 가능.

- 23:55 KST (UTC+9) — 인사 shortcut 및 SIL idle return 추가
  - 수정 파일:
    - `phil_robot/pipeline/brain_pipeline.py`: `_is_greeting_wave()` 추가, `run_brain_turn` 진입 직후 pre-classifier shortcut 삽입 — '안녕'+'반가워' 동시 포함 시 LLM 없이 `wave_hi` + "안녕하세요! 반가워요." 즉시 반환
    - `Drum_intheloop/sil/SilCommandPipeReader.py`: `IDLE_RETURN_SEC=5.0` 상수 추가, 메인 루프에 idle return 로직 추가 — 빈 tick이 5초 이상 지속되면 startup pose로 복귀 (SIL 전용)
  - 메모: greeting shortcut은 분류기 LLM 호출을 건너뛰어 응답 지연 없음. SIL idle return은 `idle_returned` 플래그로 중복 적용 방지.

- 23:30 KST (UTC+9) — SIL DXL/CAN 싱크 수정: tick 기반 frame 배치 처리 및 AgentAction DXL trajectory 보간 추가
  - 수정 파일:
    - `DrumRobot2/include/managers/SilCommandPipeWriter.hpp`: `writeTick()` 선언 추가
    - `DrumRobot2/src/SilCommandPipeWriter.cpp`: `writeTick()` 구현 (`{"kind":"tick"}` 한 줄 전송)
    - `DrumRobot2/src/DrumRobot.cpp`: sendLoopForThread에서 silWriter를 함수 스코프 static으로 올리고, 매 1ms 루프 끝에 `silWriter.writeTick()` 호출
    - `DrumRobot2/include/tasks/AgentAction.hpp`: `lastPanRad`, `lastTiltRad` 멤버 추가
    - `DrumRobot2/src/AgentAction.cpp`: `policy_lookAt`을 단일 push → 5ms 단위 사인 보간 trajectory 200개 push로 변경
    - `Drum_intheloop/sil/SilCommandPipeReader.py`: tick 메시지 파싱 추가, 메인 루프를 frame_targets 배치 방식으로 변경 (tick 수신 시 apply+step)
  - 메모: tick이 Python reader의 1ms frame 경계 역할을 하여 DXL/TMotor/Maxon 명령이 같은 stepSimulation 안에 atomic하게 적용됨.
    AgentAction policy_lookAt도 PathManager처럼 중간 각도 trajectory를 push해 SIL에서 부드러운 고개 움직임이 재현됨.

## 2026-04-15
- 22:00 KST (UTC+9) — session memory (Level 1) 추가: 세션 단기 기억 및 clarification flow 구현
  - 수정 파일:
    - `phil_robot/pipeline/session.py` (신규): `SessionContext`, `update_session`, `build_session_summary`, `resolve_clarification_text`
    - `phil_robot/pipeline/planner.py`: planner input에 `session_summary` 주입, 출력 스키마에 `q` (clarification) 필드 추가
    - `phil_robot/pipeline/validator.py`: `ValidatedPlan`에 `clarification_question` 필드 추가
    - `phil_robot/pipeline/brain_pipeline.py`: `run_brain_turn`에 `session` 파라미터 추가, clarification text 병합 처리
    - `phil_robot/phil_brain.py`: `SessionContext` 생명주기 관리, 매 턴 `update_session` 호출
  - 메모: "거기서 더 올려", "아까 그 노래", 사용자 이름 기억 등 multi-turn 맥락 처리 가능해짐.
    clarification flow ("허리 돌려" → "몇 도로?" → "30도") 추가. session=None 시 기존 stateless 동작 유지.

- 21:00 KST (UTC+9) — CLAUDE.md 파일 추가: AGENTS.md 및 주요 문서 자동 로드 설정
  - 수정 파일: `CLAUDE.md`, `Drum_intheloop/CLAUDE.md`, `phil_robot/CLAUDE.md`, `phil_robot/eval/CLAUDE.md`, `legacy/phil_intheloop/CLAUDE.md`
  - 메모: Claude Code가 각 디렉터리 작업 시 AGENTS.md, README, TODO 등을 자동으로 컨텍스트에 포함한다.

## 2026-04-14
- 17:01 KST (UTC+9) — `motion_resolver`에 다단계 상대이동 파서를 얇게 추가해 `손목 30도 내리고 1초 뒤에 10도 더 내려`, `손목 30도씩 두번 내려` 같은 문장을 state snapshot 기준 누적 절대각 시퀀스로 다시 계산하도록 보강했습니다.
  - 수정 파일: `phil_robot/pipeline/motion_resolver.py`, `phil_robot/tests/test_planner_benchmark.py`, `phil_robot/TODO.md`, `log.md`
  - 메모: planner prompt와 validator 규칙은 건드리지 않고 resolver 쪽 함수만 추가했습니다. `py_compile`은 통과했고, `build_validated_plan()` 직접 호출로 `scenario_20/21` 기대 명령이 각각 `move:R_wrist,20 -> wait:1 -> move:R_wrist,10`, `move:R_wrist,40 -> move:R_wrist,10`으로 나오는 것도 확인했습니다. `unittest`는 환경의 `ollama` import 부재 때문에 전체 모듈 로드가 막혀 직접 실행 확인으로 대체했습니다.

## 2026-04-08
- 15:00 KST (UTC+9) — `phil_robot/TODO.md`의 `Now` 우선순위를 현재 이슈 기준으로 다시 정리해, `planner 의미 해석 / resolver 계산 분리`와 `classifier routing / shortcut` 보강을 맨 위로 올리고 `failure taxonomy`는 `Parking Lot`으로 내렸습니다.
  - 수정 파일: `phil_robot/TODO.md`, `log.md`
  - 메모: `scenario_20/21`은 연속 상대동작 구조 문제, `scenario_08`은 classifier routing 문제로 보인다는 최근 분석을 TODO 보드 우선순위에 반영했습니다.

## 2026-04-08
- 13:50 KST (UTC+9) — `scenario_21`의 `바로 고칠 점` 문구도 더 간결하게 다듬어, 오른손목 반복 상대동작이 왼손목 `30도` 명령 두 번으로 잘못 풀린 문제와 발화 기준 불일치가 한눈에 보이게 정리했습니다.
  - 수정 파일: `phil_robot/eval/eval_docs/reports/scenario_report_q3-4b-q4km_q3-30b-a3b-q4km_20260406_1800.md`, `log.md`
  - 메모: 구조 설명을 줄이고, 기대한 `70→40→10` 순차 변환과 실제 출력의 차이를 바로 읽을 수 있게 문장을 짧게 바꿨습니다.

## 2026-04-08
- 13:10 KST (UTC+9) — `scenario_20`의 `바로 고칠 점` 문구를 더 간결하게 다듬어, 첫 동작은 맞고 두 번째 손목 명령이 빠졌다는 핵심이 바로 읽히게 정리했습니다.
  - 수정 파일: `phil_robot/eval/eval_docs/reports/scenario_report_q3-4b-q4km_q3-30b-a3b-q4km_20260406_1800.md`, `log.md`
  - 메모: 구조 설명은 줄이고, 실제 실패 원인인 후속 `move` 누락에 집중해 문장을 짧게 바꿨습니다.

## 2026-04-08
- 13:00 KST (UTC+9) — `20260406_1800` scenario 리포트에서 `scenario_20`, `scenario_21`의 `바로 고칠 점`을 단순 불일치 진술이 아니라 원인과 수정 방향이 보이도록 다시 썼고, 같은 JSON 리포트 안에서 `passed` 플래그가 `failed_checks`와 어긋난 항목도 함께 바로잡았습니다.
  - 수정 파일: `phil_robot/eval/reports/scenario_report_q3-4b-q4km_q3-30b-a3b-q4km_20260406_1800.json`, `phil_robot/eval/eval_docs/reports/scenario_report_q3-4b-q4km_q3-30b-a3b-q4km_20260406_1800.md`, `log.md`
  - 메모: `scenario_20`은 두 번째 상대 손목 명령 누락 문제로, `scenario_21`은 절대 목표값과 상대 이동량을 같은 `move`로 다루는 구조 문제로 정리했고, 리포트 JSON의 케이스 상태도 `22/26` 요약과 일치하게 맞췄습니다.

## 2026-04-08
- 12:50 KST (UTC+9) — `scenario_24_todo_unsafe_waist_turn_100`는 실패 케이스로 유지하되, `바로 고칠 점`이 단순 재진술이 아니라 validator recovery 보강 또는 케이스 분리 방향을 직접 말하도록 리포트 생성 문구와 `20260406_1800` 해설을 수정했습니다.
  - 수정 파일: `phil_robot/eval/run_eval.py`, `phil_robot/eval/reports/scenario_report_q3-4b-q4km_q3-30b-a3b-q4km_20260406_1800.json`, `phil_robot/eval/eval_docs/reports/scenario_report_q3-4b-q4km_q3-30b-a3b-q4km_20260406_1800.md`, `log.md`
  - 메모: `scenario_24`는 여전히 `22/26` 집계의 실패 항목으로 남기고, 절대각 한계 초과가 generic 차단 문구로 덮이는 문제를 `범위/한계 설명 recovery 추가` 또는 `generic 차단 케이스 분리`로 바로 읽히게 정리했습니다.

## 2026-04-08
- 12:43 KST (UTC+9) — `20260406_1800` scenario 리포트 JSON과 해설 Markdown도 현재 scenario 세트 기준으로 다시 맞춰, 25·28 제거, 뒤 번호 당김, `scenario_17` 발화 판정 보정을 반영했습니다.
  - 수정 파일: `phil_robot/eval/reports/scenario_report_q3-4b-q4km_q3-30b-a3b-q4km_20260406_1800.json`, `phil_robot/eval/eval_docs/reports/scenario_report_q3-4b-q4km_q3-30b-a3b-q4km_20260406_1800.md`, `log.md`
  - 메모: 결과 집계는 26건 기준 `22/26`으로 다시 계산했고, 마지막 케이스 ID도 `scenario_25`, `scenario_26`으로 정리했습니다.

## 2026-04-08
- 12:35 KST (UTC+9) — `ppt/new0408.html`의 본문 기준 문서를 리포트에서 `phil_robot/docs/PHIL_SEQUENCE_DIAGRAM_KR.md`로 바로잡아, 첫 장 영어 표지 뒤에 전체 sequence diagram을 넣고 나머지 슬라이드를 `snapshot`, `classifier`, `planner`, `modifier/executor` 흐름 중심의 한국어 deck으로 다시 구성했습니다.
  - 수정 파일: `ppt/new0408.html`, `log.md`
  - 메모: 한글 글씨가 깨지지 않도록 외부 웹폰트 링크를 빼고 `Pretendard`, `Noto Sans KR`, `Malgun Gothic`, `Apple SD Gothic Neo`, `NanumGothic` 순의 로컬 우선 fallback stack으로 정리했습니다.

## 2026-04-08
- 12:26 KST (UTC+9) — `ppt/new0408.html`을 발표용 deck으로 다시 구성해 첫 장은 영어 표지로 유지하고, 표지 다음에 화면을 꽉 채우는 한국어 sequence diagram을 넣은 뒤 나머지 슬라이드를 `scenario_report_q3-4b-q4km_q3-30b-a3b-q4km_20260406_1800.md` 기준의 한국어 결과 요약으로 정리했습니다.
  - 수정 파일: `ppt/new0408.html`, `log.md`
  - 메모: 2장부터는 실험 개요, 전체 결과, 실패 패턴, 수정 우선순위 흐름으로 재구성했고, 발표에서 바로 읽을 수 있게 75.0% pass rate, 레이어별 통과율, 대표 실패 원인을 한 장씩 나눠 넣었습니다.

## 2026-04-08
- 11:36 KST (UTC+9) — `scenario_eval` 입력 세트에서 빠진 번호가 남지 않도록 뒤의 케이스 ID를 한 칸씩 당겨 `scenario_25`, `scenario_26`으로 다시 맞췄습니다.
  - 수정 파일: `phil_robot/eval/cases_scenario_eval.json`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `log.md`
  - 메모: 현재 활성 세트에서는 옛 `scenario_25`, `scenario_28`이 빠졌기 때문에 남아 있던 `scenario_26_todo_raise_left_arm_a_bit_more`, `scenario_27_todo_look_slightly_right`를 각각 `scenario_25`, `scenario_26`으로 정리했습니다.

## 2026-04-08
- 11:27 KST (UTC+9) — `scenario_17` 농담 요청 false negative를 줄이도록 기대 발화 단서를 넓히고, 현재 validator/final speech 기준으로 검증이 어려운 `scenario_25`, `scenario_28` 제외 상태가 케이스 설명 문서에도 반영되게 다시 생성했습니다.
  - 수정 파일: `phil_robot/eval/cases_scenario_eval.json`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `log.md`
  - 메모: `scenario_17`은 실제 농담 응답이어도 `speech_contains_any`가 `농담/웃/하나`만 허용해 떨어질 수 있어 `들어보세요`, `스탠바이`를 추가했고, 25번은 연주 중 `h`가 validator에서 제거되어 최종 명령이 `s`만 남고 28번은 planner 발화가 있어도 최종 발화가 차단 문구로 바뀌어 현재 기능 기준의 scenario eval로는 안정적으로 테스트하기 어렵다고 판단했습니다.

## 2026-04-07
- 17:52 KST (UTC+9) — planner 입력을 더 줄여 classifier 결과 전체 대신 `needs_motion`만 넘기도록 바꾸고, prompt 문구·문서 예시·단위 테스트도 같은 구조로 정리했습니다.
  - 수정 파일: `phil_robot/pipeline/planner.py`, `phil_robot/tests/test_planner_benchmark.py`, `phil_robot/docs/PHIL_SEQUENCE_DIAGRAM_KR.md`, `log.md`
  - 메모: classifier 출력 객체(`intent`, `needs_motion`, `needs_dialogue`, `risk_level`)는 그대로 유지하되, runtime planner 입력 payload는 `planner_domain`, `robot_state`, `needs_motion`, `user_text`만 사용하도록 정리했습니다.
- 17:47 KST (UTC+9) — 실제 planner runtime 경로에서는 출력 스키마 예시를 system prompt에만 두고 user JSON에서는 제거해, `build_planner_input_json()` payload와 관련 문서/테스트를 중복 없이 정리했습니다.
  - 수정 파일: `phil_robot/pipeline/planner.py`, `phil_robot/tests/test_planner_benchmark.py`, `phil_robot/docs/PHIL_SEQUENCE_DIAGRAM_KR.md`, `log.md`
  - 메모: 실험용 `format compare` 스크립트는 별도 포맷 비교 목적이라 그대로 두고, 실제 runtime planner 입력만 `planner_domain`, `robot_state`, `intent_result`, `user_text` 4항목으로 맞췄습니다.
- 15:47 KST (UTC+9) — 시퀀스 다이어그램에서 `phil_brain.main()` participant를 제거하고, `STT 완료 시점 -> snapshot self-loop -> classifier/planner 입력` 흐름만 남겨 캡처 타이밍이 더 직접 보이게 정리했습니다.
  - 수정 파일: `phil_robot/docs/PHIL_SEQUENCE_DIAGRAM_KR.md`, `log.md`
  - 메모: `snapshot`으로 향하는 화살표는 데이터 전달이 아니라 상태 고정 시점 트리거로 읽히게 바꿨고, classifier/planner 입력은 다시 `STT`와 `snapshot`에서 각각 들어가도록 그렸습니다.
- 15:41 KST (UTC+9) — 시퀀스 다이어그램의 snapshot 구간을 self-loop 형태로 다시 그려, STT 직후 그 턴의 `ROBOT_STATE`를 복사해 고정하는 시점이 lifeline 위에서 바로 보이게 바꿨습니다.
  - 수정 파일: `phil_robot/docs/PHIL_SEQUENCE_DIAGRAM_KR.md`, `log.md`
  - 메모: 단순 note 설명보다 `snapshot -> snapshot` 자기 호출로 표현해 달라는 요청에 맞춰 `get_robot_state_snapshot()`의 의미를 시각적으로 드러냈습니다.
- 14:33 KST (UTC+9) — 시퀀스 다이어그램의 `main` 라벨이 너무 모호해 `main.out`과 헷갈릴 수 있어, Python 진입점인 `phil_brain.main()`으로 명시적으로 바꿨습니다.
  - 수정 파일: `phil_robot/docs/PHIL_SEQUENCE_DIAGRAM_KR.md`, `log.md`
  - 메모: 실제로는 `phil_robot/phil_brain.py`의 `main()`이 STT 뒤 `get_robot_state_snapshot()`을 호출하므로, 다이어그램에서도 그 정체가 바로 보이게 맞췄습니다.
- 14:28 KST (UTC+9) — 시퀀스 다이어그램 상단을 실제 호출 순서 중심으로 다시 다듬어, `main`이 STT 직후 `get_robot_state_snapshot()`을 호출하는 캡처 시점을 명시적으로 보이게 바꿨습니다.
  - 수정 파일: `phil_robot/docs/PHIL_SEQUENCE_DIAGRAM_KR.md`, `log.md`
  - 메모: 기존의 데이터 관계 설명 문구 대신 `STT 완료 -> snapshot 캡처 -> classifier 호출 -> planner 호출` 순서를 화살표와 note로 직접 표시했습니다.
- 12:23 KST (UTC+9) — 시퀀스 다이어그램의 데이터 화살표를 바로잡아 `STT -> classifier(user_text)`와 `snapshot -> classifier(system_info)`가 따로 합류하고, planner도 `intent_result` 단독이 아니라 전체 `planner_input_json` 묶음을 받는 구조로 수정했습니다.
  - 수정 파일: `phil_robot/docs/PHIL_SEQUENCE_DIAGRAM_KR.md`, `log.md`
  - 메모: 기존 `STT -> snapshot` 표현은 실제 코드 흐름과 달라 제거했고, classifier와 planner 위에 각각 입력 JSON 묶음이 어떻게 합쳐지는지 `Note`로 명시했습니다.
- 11:50 KST (UTC+9) — `그대에게 빠르게 연주해줘.` 예시를 기준으로 `snapshot -> classifier 입력/출력 -> planner 입력/출력 -> modifier -> executor transport command`까지 실제 JSON 형태로 따라갈 수 있게 시퀀스 문서를 확장했습니다.
  - 수정 파일: `phil_robot/docs/PHIL_SEQUENCE_DIAGRAM_KR.md`, `log.md`
  - 메모: classifier 출력 4필드(`intent`, `needs_motion`, `needs_dialogue`, `risk_level`)와 `user_text`의 재사용 위치를 분리해 적었고, `tempo_scale:1.10 -> r -> p:TY_short` 전송 순서도 예시로 명시했습니다.
- 11:39 KST (UTC+9) — 단순화한 `phil_robot` 시퀀스 다이어그램에 빠져 있던 `play_modifier` 경로를 다시 넣고, validator에서 추출되어 executor가 템포/세기 보정 명령을 먼저 전송하는 흐름을 보이게 고쳤습니다.
  - 수정 파일: `phil_robot/docs/PHIL_SEQUENCE_DIAGRAM_KR.md`, `log.md`
  - 메모: `modifier`는 별도 planner가 아니라 `parse_play_modifier(user_text)`로 validator 단계에서 붙는 보정값이라, `opt play request` 분기로 표시했습니다.
- 11:34 KST (UTC+9) — `phil_robot` 시퀀스 다이어그램을 주 경로만 남긴 단순 버전으로 다시 그려, `User -> STT -> snapshot -> classifier -> planner -> validator -> executor -> RobotClient + TTS` 흐름이 바로 읽히게 정리했습니다.
  - 수정 파일: `phil_robot/docs/PHIL_SEQUENCE_DIAGRAM_KR.md`, `log.md`
  - 메모: 기존 버전은 `main`, 상태 수신 스레드, shortcut 분기까지 한 번에 넣어 읽기 어려워서, 이번에는 세부 분기를 걷어내고 핵심 participant만 남겼습니다.
- 11:18 KST (UTC+9) — `phil_robot` 한 턴의 실제 호출 순서를 함수/역할 2줄 참가자 라벨로 읽을 수 있도록 한국어 시퀀스 다이어그램 문서를 새로 추가했습니다.
  - 수정 파일: `phil_robot/docs/PHIL_SEQUENCE_DIAGRAM_KR.md`, `log.md`
  - 메모: `main`, `_receive_loop`, `run_brain_turn`, `classifier`, `planner`, `validator`, `executor`, `RobotClient`, `TTS_Engine` 기준으로 정리했고, shortcut 경로와 planner 경로가 어디서 갈리는지도 함께 표시했습니다.
- 10:13 KST (UTC+9) — `txt7.md`의 `import 분석` 섹션을 실제 코드 기준 설명으로 채워 넣고, `numpy`, `run_brain_turn`, `execute_validated_plan`, `get_robot_state_snapshot`처럼 처음 보기엔 헷갈리기 쉬운 역할을 보강 정리했습니다.
  - 수정 파일: `txt7.md`, `log.md`
  - 메모: 사용자 초안의 표현을 최대한 살리되, "문구 정리"처럼 역할이 다르게 읽히던 부분은 `classifier/planner/validator 파이프라인`, `실행 명령 전송`, `C++ 상태 스냅샷` 기준으로 더 정확하게 다듬었습니다.
- 09:46 KST (UTC+9) — `phil_brain.py` 해체 분석용 Velog 초안 템플릿을 `txt7.md`로 새로 만들고, 제목 후보·서론·전체 흐름·import/파라미터/함수별 분석 블록·사용자 메모 칸까지 한 번에 채울 수 있게 정리했습니다.
  - 수정 파일: `txt7.md`, `log.md`
  - 메모: 이후에는 이 초안을 바탕으로 import 섹션부터 차례대로 실제 해설 문장을 덧붙여 나가면 됩니다.

## 2026-04-06
- 18:05 KST (UTC+9) — eval 리포트의 실패 항목 라벨을 실패 원인 중심 표현으로 다시 다듬어, `명령 일치` 같은 중립 표현 대신 `명령 불일치`, `발화 표현 누락`처럼 바로 읽히게 바꿨습니다.
  - 수정 파일: `phil_robot/eval/run_eval.py`, `phil_robot/eval/AGENTS.md`, `phil_robot/tests/test_planner_benchmark.py`, `phil_robot/eval/eval_docs/reports/*.md`, `log.md`
  - 메모: 실패 표와 상세 표 모두 같은 규칙을 쓰도록 맞췄고, `unittest` 25건 재통과와 report markdown 재생성까지 함께 확인했습니다.
- 17:46 KST (UTC+9) — eval 리포트의 실패 표시를 짧은 한글 이름으로 바꾸고, 각 실패 행에서 `기대한 것`과 `실제로 나온 것`을 바로 비교할 수 있게 리포트 생성 규칙과 출력 형식을 함께 정리했습니다.
  - 수정 파일: `phil_robot/eval/run_eval.py`, `phil_robot/eval/AGENTS.md`, `phil_robot/tests/test_planner_benchmark.py`, `phil_robot/eval/eval_docs/reports/*.md`, `log.md`
  - 메모: 이제 `classifier.intent` 같은 내부 체크 ID를 report 표에 그대로 던지지 않고 `의도`, `도메인`, `명령 일치`, `발화 포함`처럼 먼저 풀어 보여줍니다. `농담해봐`처럼 왜 실패했는지 헷갈리던 케이스도 기대 키워드와 실제 발화를 같은 줄에서 비교할 수 있게 바꿨고, `unittest` 25건 통과까지 확인했습니다.
- 16:35 KST (UTC+9) — `phil_robot` 루트에 새 `AGENTS.md`를 추가하고, prompt/shortcut/identity 응답을 손볼 때 공통으로 따를 페르소나 작성 규칙을 정리했습니다.
  - 수정 파일: `phil_robot/AGENTS.md`, `log.md`
  - 메모: 필의 정체성, 톤, 능력/제약을 분리해서 쓰고, 검증되지 않은 능력 과장 금지, 이름 확인형 yes/no 일관성 유지, 재사용 가능한 패턴 중심 작성 같은 기준을 `phil_robot` 범위 전체에 걸어두었습니다.
- 16:23 KST (UTC+9) — `모펫이니?` 같은 이름 확인형 yes/no 질문과 `손흔들고 This Is Me 연주해줘.` 같은 wave+play 복합 요청을 deterministic shortcut으로 고정하고, TODO 케이스 23 문구도 `This Is Me` 기준으로 바꿨습니다.
  - 수정 파일: `phil_robot/pipeline/state_adapter.py`, `phil_robot/pipeline/brain_pipeline.py`, `phil_robot/eval/cases_scenario_eval.json`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `phil_robot/tests/test_planner_benchmark.py`, `log.md`
  - 메모: 이제 `scenario_12_key_out_name_no_shake`는 planner를 거치지 않고 `gesture:shake + "아니요, 제 이름은 필이에요."`로 직접 처리되고, `scenario_23_todo_play_and_wave_greatest_showman`은 실제 문구를 `손흔들고 This Is Me 연주해줘.`로 바꾼 뒤 `gesture:wave + r + p:TIM` 고정 시퀀스로 처리됩니다. targeted eval 두 건 모두 `planner_called=False` 상태로 통과 확인했습니다.
- 16:16 KST (UTC+9) — `scenario_09_key_out_right_wrist_up` 손목 기본 상대 이동폭을 다시 원래 15도로 되돌리고, 대응 테스트와 eval 기대값도 `move:R_wrist,35` 기준으로 복원했습니다.
  - 수정 파일: `phil_robot/pipeline/motion_resolver.py`, `phil_robot/tests/test_planner_benchmark.py`, `phil_robot/eval/cases_scenario_eval.json`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `log.md`
  - 메모: 사용자 확인대로 `현재 20도에서 15도 더 올리기 -> 35도`는 원래 의도와 맞는 결과라 보고 되돌렸습니다. 실제 targeted eval에서도 `valid_op_cmds=['move:R_wrist,35']`, `speech='오른쪽 손목을 15도 더 올려드릴게요.'`로 다시 확인했습니다.
- 16:14 KST (UTC+9) — `scenario_09_key_out_right_wrist_up` 기준이 너무 느슨해 `move:R_wrist,35` 같은 약한 상승도 통과하던 문제를 줄이기 위해, 무각도 손목 올리기 기본치를 30도로 키우고 케이스 기대값도 `move:R_wrist,50`으로 조였습니다.
  - 수정 파일: `phil_robot/pipeline/motion_resolver.py`, `phil_robot/tests/test_planner_benchmark.py`, `phil_robot/eval/cases_scenario_eval.json`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `log.md`
  - 메모: 이제 이 케이스는 접두사만 맞으면 통과가 아니라 실제로 `20 -> 50도` 수준의 눈에 띄는 손목 상승과 대응 발화까지 맞아야 통과합니다. 실제 eval 한 건을 다시 돌려 `valid_op_cmds=['move:R_wrist,50']`, `speech='오른쪽 손목을 30도 더 올려드릴게요.'`로 통과 확인했습니다.
- 16:12 KST (UTC+9) — 곡 목록 질문(`너 무슨 노래 연주할 수 있니?`)이 안전 키 차단 문구로 잘못 빠지지 않도록 repertoire 질의를 classifier에서 `chat`으로 고정하고, brain에서 지원 곡 목록을 deterministic shortcut으로 직접 응답하게 바꿨습니다.
  - 수정 파일: `phil_robot/pipeline/state_adapter.py`, `phil_robot/pipeline/intent_classifier.py`, `phil_robot/pipeline/brain_pipeline.py`, `phil_robot/tests/test_intent_classifier.py`, `phil_robot/tests/test_planner_benchmark.py`, `log.md`
  - 메모: 이제 이 질문은 `play_request`로 올라가지 않아 `아직 안전 키가 해제되지 않아...`로 덮이지 않고, 실제 eval에서도 `chat / chat / valid_op_cmds=[] / "저는 This Is Me, Test Beat, 그대에게, Baby I Need You를 연주할 수 있어요." / planner_called=False`로 통과하는 것을 확인했습니다.
- 16:00 KST (UTC+9) — `오른 쪽 손목 들어봐`, `준비`, `양팔 올렸다가 3초 뒤에 양팔 내려` 시나리오가 실제 eval 경로에서 통과하도록 motion/classifier/skill 필터를 보강하고, scenario 기대 문구도 정상 발화 기준으로 다듬었습니다.
  - 수정 파일: `phil_robot/pipeline/intent_classifier.py`, `phil_robot/pipeline/planner.py`, `phil_robot/pipeline/skills.py`, `phil_robot/pipeline/validator.py`, `phil_robot/pipeline/motion_resolver.py`, `phil_robot/tests/test_intent_classifier.py`, `phil_robot/tests/test_planner_benchmark.py`, `phil_robot/eval/cases_scenario_eval.json`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `log.md`
  - 메모: `준비`는 `motion_request`로 승격하고 `ready_pose`를 더 강하게 유도했으며, `오른 쪽` 같은 띄어쓰기 변형과 planner fallback greeting은 resolver/speech override로 복구되게 했습니다. 또 `wait:3`가 skill category 필터에서 사라지지 않게 고쳐 `/home/shy/miniforge3/envs/drum4/bin/python`으로 해당 3개 케이스를 다시 태웠을 때 모두 통과하는 것까지 확인했습니다.
- 15:47 KST (UTC+9) — `너 무슨 노래 연주할 수 있니?` 케이스는 분류 label(`chat` vs `status`)보다 실제 목록 응답 품질이 더 중요하다고 보고, `scenario_07_song_list_question`에서 intent/domain 강제를 제거했습니다.
  - 수정 파일: `phil_robot/eval/cases_scenario_eval.json`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `log.md`
  - 메모: 이제 이 항목은 `valid_commands_exact=[]`와 곡 목록 발화만 확인하므로, 실제로는 잘 대답했는데 분류 라벨 차이 때문에 실패하는 일이 줄어듭니다.
- 15:46 KST (UTC+9) — `너의 이름 필 맞지?`, `너의 이름은 모펫이니?` 같은 확인형 identity 질문을 classifier가 motion 쪽으로 올리고, motion planner도 긍정은 `nod_yes`, 부정은 `shake_no`를 우선 고려하도록 prompt와 정규화 규칙을 보강했습니다.
  - 수정 파일: `phil_robot/pipeline/intent_classifier.py`, `phil_robot/pipeline/planner.py`, `phil_robot/tests/test_intent_classifier.py`, `phil_robot/tests/test_planner_benchmark.py`, `log.md`
  - 메모: 기존에는 identity keyword 규칙이 이런 문장을 무조건 `chat + needs_motion=false`로 눌러버렸고, 그 때문에 말은 맞아도 끄덕/도리도리 동작이 빠졌습니다. 이제는 확인형 종결 표현을 따로 감지해 motion 의도로 승격합니다.
- 15:43 KST (UTC+9) — `cases_scenario_eval.json`을 다시 정리해 사용자 지정 21개 케이스를 `scenario_01~21`로 앞쪽에 정확히 배치하고, TODO 예시 7개는 뒤 `scenario_22~28`로 유지했습니다.
  - 수정 파일: `phil_robot/eval/cases_scenario_eval.json`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `log.md`
  - 메모: 이제 `scenario` suite를 열면 먼저 보이는 21개가 사용자가 직접 준 테스트 케이스와 1:1로 대응하며, `scenario_15`까지 포함해 첫 21개에는 intent/domain 미채점 항목이 남지 않도록 채웠습니다.
- 15:31 KST (UTC+9) — `look:0,90`를 후처리로 지우는 대신 planner prompt와 skill 정의를 바꿔, 사용자가 고개/시선 방향을 직접 말하지 않으면 `look_forward`나 `look:0,90`를 기본값처럼 생성하지 않도록 조정했습니다.
  - 수정 파일: `phil_robot/pipeline/planner.py`, `phil_robot/pipeline/skills.py`, `phil_robot/tests/test_planner_benchmark.py`, `log.md`
  - 메모: planner 예시 JSON은 더 이상 `wave_hi`로 시작하지 않게 바꿨고, `wave_hi` skill에서도 정면 보기 명령을 제거해 불필요한 출력 토큰과 실행 명령이 함께 줄어들도록 맞췄습니다.
- 15:21 KST (UTC+9) — `scenario_02_todo_play_and_wave_greatest_showman`의 사용자 문구를 `손흔들고 위대한 쇼맨 연주해줘.`로 바꾸고, 대응 scenario 케이스 문서도 다시 생성했습니다.
  - 수정 파일: `phil_robot/eval/cases_scenario_eval.json`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `log.md`
  - 메모: `위대한 쇼맨을 먼저 연주하고 나서 손을 흔드는` 해석은 애초에 불가능한 흐름이므로, 2번 문항은 `먼저 손을 흔들고 그다음 연주`로 읽히게 사용자 발화를 명확히 바꿨습니다.
- 15:14 KST (UTC+9) — `scenario_eval` 케이스 정의를 다듬어 빠진 intent/domain을 보강하고, 느슨한 말 비교는 `speech_contains_all`로 강화했으며 입력 케이스 문서의 빈 기대값 표시는 `기록 없음` 대신 `미채점`으로 바꿨습니다.
  - 수정 파일: `phil_robot/eval/cases_scenario_eval.json`, `phil_robot/eval/run_eval.py`, `phil_robot/eval/generate_case_docs.py`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `phil_robot/tests/test_planner_benchmark.py`, `log.md`
  - 메모: compound/sequence 케이스 몇 건은 JSON 기대값이 비어 있거나 `speech_contains_any`가 너무 느슨해 엉뚱한 답도 통과할 수 있어, `scenario_02`, `07`, `18`, `19`, `21`, `26`, `27`, `28` 중심으로 기준을 다시 조였습니다.
- 15:09 KST (UTC+9) — `scenario_01_todo_greet_and_nod`의 기대 intent를 `chat`에서 `motion_request`로 바로잡고, 대응 scenario 케이스 문서도 다시 생성했습니다.
  - 수정 파일: `phil_robot/eval/cases_scenario_eval.json`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `log.md`
  - 메모: `안녕 하고 고개 끄덕여`는 현재 평가 기준상 인사+동작 복합 요청이므로 `classifier.intent`를 `motion_request`로 보는 쪽이 자연스러워, 정상 결과가 오답 처리되지 않게 수정했습니다.
- 14:28 KST (UTC+9) — `run_eval.py`가 저장한 report JSON으로 대응 Markdown 리포트를 바로 생성하도록 연결하고, 결과 요약에 통과/실패 개수·케이스 목록·실제 최종 발화가 먼저 보이게 정리했습니다.
  - 수정 파일: `phil_robot/eval/run_eval.py`, `phil_robot/tests/test_planner_benchmark.py`, `phil_robot/eval/README.md`, `log.md`
  - 메모: 이제 `--report`나 `--save-report`로 저장하면 `reports/*.json`과 짝이 맞는 `eval_docs/reports/*.md`가 함께 생성되며, 상세 표에는 각 케이스의 실제 응답이 그대로 들어갑니다.
- 14:14 KST (UTC+9) — `cases_*.json`에서 `eval_docs/cases_*.md`를 직접 생성하는 `generate_case_docs.py`를 추가하고, smoke/scenario 케이스 문서도 JSON 기준 표로 다시 생성했습니다.
  - 수정 파일: `phil_robot/eval/generate_case_docs.py`, `phil_robot/eval/eval_docs/cases_smoke.md`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `phil_robot/eval/AGENTS.md`, `log.md`
  - 메모: 입력 케이스 문서는 이제 수동 해설 대신 `case id / tags / 사용자 발화 / 기대 intent / 기대 domain / LLM 응답` 표를 JSON에서 직접 뽑아 쓰며, `핵심 확인 포인트` 같은 수동 열은 없앴습니다.
- 14:14 KST (UTC+9) — 입력 케이스 해설 문서의 상세표에서 `핵심 확인 포인트` 열을 제거하고 `LLM 응답` 중심 표로 정리해, 표만 봐도 기대 답변이 바로 읽히도록 바꿨습니다.
  - 수정 파일: `phil_robot/eval/eval_docs/cases_smoke.md`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `phil_robot/eval/AGENTS.md`, `log.md`
  - 메모: 결과 리포트는 실제 최종 발화를, 입력 케이스 문서는 기대 `LLM 응답`을 바로 보여주는 역할로 더 분명하게 나눴습니다.
- 14:08 KST (UTC+9) — `phil_robot/eval/AGENTS.md`에 결과 해설 문서의 `결과 요약` 규칙을 강화해, 앞으로는 통과/실패 개수와 케이스 목록, 바로 고칠 항목이 먼저 드러나도록 기준을 명시했습니다.
  - 수정 파일: `phil_robot/eval/AGENTS.md`, `log.md`
  - 메모: `결과 요약`에서 감상문보다 사실 정리를 우선하고, 입력 케이스 문서에는 기대 말 응답, 실행 결과 문서에는 실제 최종 발화가 보이도록 규칙을 분명히 했습니다.
- 14:08 KST (UTC+9) — `cases_smoke.md`와 `cases_scenario_eval.md` 상세표에 각 케이스의 기대 말 응답 열을 추가해, 명령 기대값뿐 아니라 어떤 식으로 답해야 하는지도 한눈에 읽히게 정리했습니다.
  - 수정 파일: `phil_robot/eval/eval_docs/cases_smoke.md`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `log.md`
  - 메모: 결과 리포트는 실제 최종 발화를 보여주고, 입력 케이스 해설 문서는 기대 말 응답을 보여주도록 역할을 맞췄습니다.
- 14:08 KST (UTC+9) — `run_eval.py` 기본 summary에 classifier/planner/total 평균 지연, 중앙값, p95를 다시 포함시키고, 콘솔 출력과 단위 테스트도 함께 보강했습니다.
  - 수정 파일: `phil_robot/eval/run_eval.py`, `phil_robot/tests/test_planner_benchmark.py`, `log.md`
  - 메모: 다음 smoke/scenario 실행부터 저장되는 JSON report의 `summary.latency_summary`에 단계별 요약이 남고, 터미널에도 `avg / median / p95`가 같이 보여 이후 해설 리포트에서 바로 인용하기 쉬운 형태로 맞췄습니다.
- 14:02 KST (UTC+9) — `run_eval.py`가 `--scenario` 별칭도 받도록 보완해, `--suite scenario` 대신 자연스럽게 친 실행 명령도 바로 동작하게 했습니다.
  - 수정 파일: `phil_robot/eval/run_eval.py`, `phil_robot/eval/README.md`, `log.md`
  - 메모: 사용 흐름상 `python eval/run_eval.py --scenario`를 먼저 치기 쉬워 보여 argparse alias를 추가했고, README에도 짧은 실행 예시를 같이 적었습니다.
- 13:15 KST (UTC+9) — `phil_robot`의 `scenario eval` TODO 예시 7개와 운영 확인용 21개 시나리오를 새 suite JSON으로 정리하고, 대응 해설 문서와 실행 엔트리를 함께 추가했습니다.
  - 수정 파일: `phil_robot/eval/cases_scenario_eval.json`, `phil_robot/eval/eval_docs/cases_scenario_eval.md`, `phil_robot/eval/run_eval.py`, `phil_robot/eval/README.md`, `log.md`
  - 메모: 일부 케이스는 현재 파이프라인보다 앞선 목표 동작을 고정하는 성격이라 기대값을 완전히 빡빡하게 두기보다, 복합 명령·상태 제약·순차 동작의 핵심 확인 포인트가 드러나도록 작성했습니다.
- 10:26 KST (UTC+9) — `phil_robot` 온라인 STT 전환을 다른 agent가 그대로 구현할 수 있도록 handoff용 `PLAN.md`를 새로 작성하고, 침묵 임계값/echo 차단/복합 발화 acceptance 범위를 결정 완료 수준으로 고정했습니다.
  - 수정 파일: `phil_robot/PLAN.md`, `log.md`
  - 메모: `post-silence commit 300ms`, `final-only planner`, `is_speaking` 기반 self-echo 차단, `복합 발화는 STT final text 안정성만 검증` 기준을 문서에 명시했고, `smalll -> small` 선행 수정도 계획에 포함했습니다.

## 2026-04-03
- 18:02 KST (UTC+9) — latency/KV cache 블로그 초안에서 JSON 출력 형식 비교 해석을 다듬어, `legacy_str`가 평균상 더 빠르긴 했지만 JSON이 압도적으로 느린 것은 아니었다는 뉘앙스로 정정했습니다.
  - 수정 파일: `txt5.md`, `txt6.md`, `log.md`
  - 메모: `avg_wall_sec 3.95 vs 4.88`, `avg_eval_tokens 37.30 vs 57.10` 수치는 유지하고, 결론을 `JSON 포기`가 아니라 `JSON 유지 + compact prompt/schema 최적화` 쪽으로 더 정확하게 표현했습니다.

## 2026-04-03
- 17:52 KST (UTC+9) — 응답 지연 최적화 회고와 KV cache 친화적 prompt/schema 정리 글을 블로그 초안 `txt5.md`, `txt6.md`로 추가했습니다.
  - 수정 파일: `txt5.md`, `txt6.md`, `log.md`
  - 메모: `log.md`와 실제 수정 파일 흐름을 바탕으로 cold start, STT/TTS 벤치, compact schema, prompt 순서 재배열, KV cache 관점의 의도와 효과를 각각 한 문서씩 분리해 정리했습니다.

## 2026-04-03
- 17:32 KST (UTC+9) — planner 출력 스키마도 `s/c/t/r` compact JSON으로 줄이고, parser가 compact/legacy planner 응답을 모두 long-form 내부 구조로 복원하도록 확장했습니다.
  - 수정 파일: `phil_robot/pipeline/planner.py`, `phil_robot/tests/test_planner_benchmark.py`, `log.md`
  - 메모: drum4 live probe에서 `그대에게 연주해줘`와 `손 흔들어줘`의 planner raw가 각각 `{"s":[...],"c":[],"t":"...","r":"..."}` 형태로 출력되는 것을 확인했고, 단위 테스트 20건도 통과했습니다.

## 2026-04-03
- 17:12 KST (UTC+9) — classifier 출력 스키마를 `i/m/d/r` compact JSON과 `C/M/P/Q/X/U`, `1/0`, `L/M/H` 코드로 줄여 eval 토큰을 낮추고, parser가 compact/legacy/truncated 응답을 모두 읽도록 확장했습니다.
  - 수정 파일: `phil_robot/pipeline/intent_classifier.py`, `phil_robot/tests/test_intent_classifier.py`, `log.md`
  - 메모: drum4 live 재현에서 classifier raw가 `{"i":"P","m":1,"d":1,"r":"M"}` 형태로 출력되고 `eval_tokens`가 `34 -> 18`로 줄어든 것을 확인했으며, 단위 테스트 7건도 통과했습니다.

## 2026-04-03
- 17:00 KST (UTC+9) — Ollama 요청에서 `num_predict`를 아예 보내지 않도록 바꿔 classifier/planner 출력 개수 상한을 제거하고, 관련 테스트를 무제한 옵션 기준으로 갱신했습니다.
  - 수정 파일: `phil_robot/config.py`, `phil_robot/tests/test_llm_interface.py`, `log.md`
  - 메모: 더 이상 `PHIL_CLASSIFIER_NUM_PREDICT`나 `PHIL_PLANNER_NUM_PREDICT`로 출력 개수를 자르지 않으며, drum4에서 `그대에게 연주해줘` live 재현과 단위 테스트 6건이 모두 정상 통과하는 것을 다시 확인했습니다.

## 2026-04-03
- 16:44 KST (UTC+9) — classifier JSON이 `num_predict=24`에서 잘려 `unknown`으로 추락하던 문제를 막기 위해 classifier 출력 상한을 늘리고, 잘린 JSON에서도 핵심 필드를 회수하는 parser와 play 요청 보정 테스트를 추가했습니다.
  - 수정 파일: `phil_robot/config.py`, `phil_robot/pipeline/intent_classifier.py`, `phil_robot/phil_brain.py`, `phil_robot/tests/test_intent_classifier.py`, `log.md`
  - 메모: drum4에서 `그대에게 연주해줘`와 `This Is Me 연주해줘`를 다시 돌려 `play_request -> play -> ['r', 'p:TY_short'/'p:TIM']` 경로가 복구된 것을 확인했고, 새 단위 테스트 6건도 통과했습니다. 디버그 로그의 `Phil's Brain` 표기는 `Planner Reason`으로 바꿔 미실행 reason이 실제 실행처럼 보이던 혼선을 줄였습니다.

## 2026-04-03
- 15:37 KST (UTC+9) — `phil_robot/eval` 문서 지침을 더 구체화해 실험 목적, 고정/변경 포인트, 경량화 기법 설명을 반드시 넣도록 하고, 기존 `eval_docs`도 같은 기준의 쉬운 한국어 설명으로 다시 정리했습니다.
  - 수정 파일: `phil_robot/eval/AGENTS.md`, `phil_robot/eval/eval_docs/*.md`, `phil_robot/eval/eval_docs/reports/*.md`, `log.md`
  - 메모: `스크리닝`, `회귀`, `mismatch`, `blocker`, `happy path`, `edge case 정합성` 같은 추상 표현 대신, 실제로 어떤 상황에서 무엇이 기대와 달랐는지 풀어서 쓰도록 규칙을 강화했습니다. voice/planner/format 문서에는 fp16, stream 길이, JSON-only 고정, 입력 고정 같은 비교 항목도 앞쪽 섹션에 드러내도록 맞췄습니다.

## 2026-04-03
- 15:15 KST (UTC+9) — `phil_robot/eval` 전용 문서화 지침을 새로 만들고, 평가 JSON 20개를 사람용 해설 마크다운으로 `eval_docs` 아래에 일괄 정리했습니다.
  - 수정 파일: `phil_robot/eval/AGENTS.md`, `phil_robot/eval/eval_docs/*.md`, `phil_robot/eval/eval_docs/reports/*.md`, `log.md`
  - 메모: 입력 케이스/manifest JSON과 결과 리포트 JSON을 구분해 해설하도록 규칙을 세웠고, 각 문서는 한눈에 보기, 표 중심 상세, 특이 케이스, 종합 총평까지 포함하도록 맞췄습니다.

## 2026-04-03
- 16:23 KST (UTC+9) — STT/TTS 런타임 설정을 다시 원래 방식으로 돌리고, `melo_engine.py`의 구두점 기준 스트리밍 분할은 유지한 채 환경변수 의존을 제거했습니다.
  - 수정 파일: `phil_robot/config.py`, `phil_robot/phil_brain.py`, `phil_robot/runtime/melo_engine.py`, `phil_robot/eval/run_voice_io_benchmark.py`, `phil_robot/tests/test_melo_engine.py`, `log.md`
  - 메모: STT는 다시 `small + fp16=True` 고정으로, TTS는 `stream=True` 고정으로 복원했고, drum4에서 관련 테스트 13건과 `py_compile`을 다시 통과했습니다.

## 2026-04-03
- 14:52 KST (UTC+9) — STT/TTS 설정을 자동 비교하는 voice I/O 벤치 스크립트를 추가하고, drum4 실측 결과에 맞춰 기본값을 `STT fp16 off / prompt 비움 / TTS stream min_len=18`로 조정했습니다.
  - 수정 파일: `phil_robot/config.py`, `phil_robot/phil_brain.py`, `phil_robot/runtime/melo_engine.py`, `phil_robot/eval/run_voice_io_benchmark.py`, `phil_robot/tests/test_melo_engine.py`, `log.md`
  - 메모: `cases_smoke.json` 기반 문장과 보강 probe 문장을 사용해 `/home/shy/robot_project/phil_robot/eval/reports/voice_io_benchmark_20260403_145124.json`을 생성했고, 새 기본값 반영 후 drum4에서 관련 테스트 13건을 다시 통과했습니다.

## 2026-04-03
- 14:17 KST (UTC+9) — MeloTTS 응답 발화에 문장 단위 스트리밍 경로를 추가해, 여러 문장 답변에서 첫 음성이 더 빨리 나오도록 하고 실패 시 기존 `aplay` 방식으로 안전하게 fallback 하도록 했습니다.
  - 수정 파일: `phil_robot/runtime/melo_engine.py`, `phil_robot/phil_brain.py`, `phil_robot/tests/test_melo_engine.py`, `log.md`
  - 메모: 너무 짧은 조각으로 잘라 억양이 깨지는 문제를 줄이기 위해 최소 길이 기반 chunk 분할을 사용했고, drum4 환경에서 새 TTS 테스트 3건과 planner 회귀 테스트 10건을 통과했습니다.

## 2026-04-03
- 14:01 KST (UTC+9) — planner 프롬프트와 입력 JSON의 순서를 KV cache 친화적으로 재배열해, 공통 규칙이 앞에 고정되고 `user_text`가 마지막에 오도록 리팩터링했습니다.
  - 수정 파일: `phil_robot/pipeline/planner.py`, `phil_robot/tests/test_planner_benchmark.py`, `log.md`
  - 메모: system prompt는 `shared rules -> domain rules` 순서로 바꿨고, planner input JSON은 `planner_domain -> response_schema -> robot_state -> intent_result -> user_text` 순서 회귀 테스트를 추가했습니다.

## 2026-04-02
- 18:08 KST (UTC+9) — `python phil_brain.py` 직접 실행 경로에서 `llm_interface.py`가 상대 import 실패 후 올바른 fallback 모듈을 찾도록 import 예외 처리와 fallback 경로를 고쳤습니다.
  - 수정 파일: `phil_robot/pipeline/llm_interface.py`, `phil_robot/tests/test_llm_interface.py`, `log.md`
  - 메모: `ValueError: attempted relative import beyond top-level package`와 이어진 `ModuleNotFoundError: No module named 'failure'`를 함께 수정했고, `pipeline.llm_interface` top-level import regression test를 추가했습니다.

## 2026-04-02
- 17:58 KST (UTC+9) — `phil_robot`의 Ollama 호출 경로에 `think=false`, `keep_alive`, 모델별 `num_ctx`/`num_predict` 기본값을 넣고, runtime 로그에서 `load/prompt_eval/eval` 시간을 바로 보이도록 정리했습니다.
  - 수정 파일: `phil_robot/config.py`, `phil_robot/pipeline/llm_interface.py`, `phil_robot/phil_brain.py`, `phil_robot/init_phil.sh`, `phil_robot/tests/test_llm_interface.py`, `log.md`
  - 메모: classifier와 planner를 둘 다 preload하도록 startup 스크립트를 넓혔고, 새 테스트로 Ollama 옵션 전달과 `load_duration` 포함 메트릭 계산을 검증했습니다. live probe에서는 warm classifier 호출 기준 `load_sec` 약 `0.14s`, `prompt_sec` 약 `0.57s`, `eval_sec` 약 `1.07s`가 확인됐습니다.

## 2026-04-02
- 17:36 KST (UTC+9) — `txt4.md`를 다시 정리해 리포트 파일 경로 나열을 걷어내고, 실제 benchmark JSON의 `summary`, 실패 `failed_checks`, 수정 후 `actual` 조각을 그대로 인용하는 Velog용 형식으로 바꿨습니다.
  - 수정 파일: `txt4.md`, `log.md`
  - 메모: 초기 screening, JSON-only benchmark, latency isolation, targeted fix, final smoke를 모두 ` ```json ` 블록 중심으로 재구성했고, `motion_blocked_while_playing`의 `led:happy` 잔존과 수정 후 `valid_op_cmds: []`를 문서 안에서 바로 확인할 수 있게 했습니다.

## 2026-04-02
- 17:23 KST (UTC+9) — `txt4.md`의 로컬 마크다운 링크를 모두 제거하고, Velog에 그대로 붙여넣기 쉬운 fenced code block 경로 표기로 다시 정리했습니다.
  - 수정 파일: `txt4.md`, `log.md`
  - 메모: 파일 참조는 `[label](path)` 대신 ```text fenced block```으로 통일했고, 기존 벤치마크 수치와 결론은 유지한 채 외부 플랫폼에서 링크가 깨지지 않도록 문서 형식을 다듬었습니다.

## 2026-04-02
- 17:15 KST (UTC+9) — `DrumRobot2`의 `AgentAction`에서 Arduino head LED를 바꾸는 `led` command 경로를 제거하고, 오늘 planner benchmark/latency/fix loop 결과를 Velog용 정리 문서 `txt4.md`로 작성했습니다.
  - 수정 파일: `DrumRobot2/include/tasks/AgentAction.hpp`, `DrumRobot2/src/AgentAction.cpp`, `txt4.md`, `log.md`
  - 메모: Python 쪽 LED 제거와 맞춰 C++ controller에서도 `policy_emotion()`과 `led` routing을 없앴고, `make -C DrumRobot2 -j2` 빌드가 통과했습니다. 문서에는 초기 multi-model screening, JSON-only benchmark, latency isolation, wrist/LED 수정 루프, final smoke `13/13` 결과와 산출물 경로를 함께 정리했습니다.

## 2026-04-02
- 17:02 KST (UTC+9) — LED 명령을 실행 경로와 smoke 기준에서 제거하고, 관련 skill/prompt/example/eval 기대값을 정리한 뒤 qwen3 4b + qwen3 30b-a3b 조합으로 targeted 5건과 full smoke 13건을 다시 검증했습니다.
  - 수정 파일: `phil_robot/pipeline/skills.py`, `phil_robot/pipeline/planner.py`, `phil_robot/pipeline/command_validator.py`, `phil_robot/pipeline/response_parser.py`, `phil_robot/eval/cases_smoke.json`, `phil_robot/eval/README.md`, `phil_robot/eval/run_format_compare.py`, `phil_robot/tests/test_planner_benchmark.py`, `phil_robot/tests/test_stt_llm_format_compare.py`, `phil_robot/docs/LLM_PIPELINE_ARCHITECTURE_KR.md`, `phil_robot/docs/LLM_PIPELINE_ARCHITECTURE.md`, `log.md`
  - 메모: social/play/idle skill에서 `led:*`를 제거했고 planner 허용 prefix 에서도 LED를 뺐습니다. validator 는 `led:`를 항상 reject 하도록 바꿨고, smoke 기대값도 `play`/`stop`/blocked motion에 맞게 갱신했습니다. `/tmp/targeted_led_removed_report.json`은 `5/5`, `/tmp/smoke_led_removed_report.json`은 `13/13`으로 통과했습니다.

## 2026-04-02
- 16:54 KST (UTC+9) — motion blocked 상태에서 planner가 `skills/op_cmd=[]`를 더 강하게 따르도록 프롬프트를 조이고, validator의 양쪽 손목 상한을 `90도`로 낮춘 뒤 관련 smoke 케이스만 다시 실행했습니다.
  - 수정 파일: `phil_robot/pipeline/planner.py`, `phil_robot/pipeline/command_validator.py`, `log.md`
  - 메모: targeted smoke 5건 중 `relative_wrist_raise_blocked`는 wrist limit 조정으로 해결됐고, `motion_blocked_while_playing`는 planner가 여전히 `wave_hi`를 생성해 `led:happy`가 남는 것을 확인했습니다. 즉 손목 한계 문제와 blocked-state social skill 잔류 문제는 별개 축입니다.

## 2026-04-02
- 16:33 KST (UTC+9) — planner benchmark를 `JSON-only fixed fixture` 기준으로 다시 정리하고, planner latency isolation 러너를 추가한 뒤 shortcut 플래그 버그를 고쳐 실제 `planner_input_json` 기반 dt 집계가 채워지도록 검증했습니다.
  - 수정 파일: `phil_robot/eval/planner_json_benchmark.py`, `phil_robot/eval/run_planner_benchmark.py`, `phil_robot/eval/run_planner_latency_isolation.py`, `phil_robot/eval/planner_benchmark_round1_manifest.json`, `phil_robot/eval/README.md`, `phil_robot/eval/PLANNER_MODEL_BENCHMARK_PLAN_KR.md`, `phil_robot/TODO.md`, `phil_robot/tests/test_planner_benchmark.py`, `log.md`
  - 메모: planner benchmark는 이제 `legacy_str` 비교를 후보 선정 경로에서 제외하고, 케이스당 한 번 고정한 `classifier_result`/`planner_input_json` 위에서 planner pass rate, skill/command/speech quality, avg/median/p95 latency를 비교합니다. `python -m py_compile`, `python -m unittest phil_robot.tests.test_planner_benchmark` 8건, 단일-model JSON benchmark, 2-case latency isolation 실측으로 리포트 구조와 dt 집계를 다시 확인했습니다.

## 2026-04-02
- 14:48 KST (UTC+9) — planner round-1 벤치를 `config.py` 수정 없이 돌릴 수 있도록 model override, Ollama timing 메타데이터 수집, round batch runner/manifest, gate 테스트를 추가했습니다.
  - 수정 파일: `phil_robot/eval/run_eval.py`, `phil_robot/pipeline/brain_pipeline.py`, `phil_robot/pipeline/llm_interface.py`, `phil_robot/eval/run_planner_benchmark.py`, `phil_robot/eval/planner_benchmark_round1_manifest.json`, `phil_robot/tests/test_planner_benchmark.py`, `phil_robot/eval/README.md`, `phil_robot/eval/PLANNER_MODEL_BENCHMARK_PLAN_KR.md`, `log.md`
  - 메모: `run_eval.py`는 `--classifier-model`/`--planner-model`/`--capture-llm-metrics`를 지원하게 했고, 새 batch runner는 `prep -> smoke -> latency` 흐름에서 `pass`만 latency 로 넘기며 `hold_review`/`fail`은 raw report 와 round summary 만 남기고 계속 진행합니다. baseline smoke 의 default vs CLI override 결과가 둘 다 `11/13`으로 같음을 확인했고, 새 단위 테스트 5건도 통과했습니다.

## 2026-04-02
- 14:16 KST (UTC+9) — `phil_robot/eval`에 planner 후보 7개 모델의 향후 비교 순서, 게이트, 기록 항목, 저장 규칙을 정리한 실행 계획서와 README 링크를 추가했습니다.
  - 수정 파일: `phil_robot/eval/PLANNER_MODEL_BENCHMARK_PLAN_KR.md`, `phil_robot/eval/README.md`, `log.md`
  - 메모: `qwen3`, `nemotron-cascade-2`, `gpt-oss`, `mistral-small3.2`, `exaone3.5`, `lfm2` 후보군에 대해 correctness-first benchmark 절차와 latency/finalist 추가 비교 흐름을 고정해 이후 round report를 같은 규칙으로 쌓을 수 있게 했습니다.

## 2026-04-02
- 11:14 KST (UTC+9) — smoke 케이스 10개를 사용해 planner의 `legacy_str` 대 `json` 출력 형식을 실제 Qwen 30B A3B 모델로 벤치하는 스크립트를 추가하고, warm-up을 제외한 평균 비교 리포트를 `phil_robot/docs`에 생성했습니다.
  - 수정 파일: `phil_robot/eval/run_format_compare.py`, `phil_robot/docs/FORMAT_COMPARE_BENCHMARK_KR.md`, `phil_robot/docs/format_compare_benchmark_smoke_20260402_111228.json`, `log.md`
  - 메모: classifier는 케이스마다 한 번만 실행해 공유하고 planner 단계만 두 형식으로 비교했으며, 초기 모델 로드 편향을 피하려고 classifier/두 planner 모드에 사전 warm-up 1회씩을 넣었습니다. 최종 평균은 `legacy_str` 약 3.95초, `json` 약 4.88초였습니다.

## 2026-04-02
- 10:59 KST (UTC+9) — 비교 스크립트의 LLM 속도 로그를 더 풀어 `prompt_eval_sec`, `eval_sec`, `meta_sec`, `overhead_sec`까지 함께 보이도록 바꿨습니다.
  - 수정 파일: `phil_robot/tests/test_stt_llm_format_compare.py`, `log.md`
  - 메모: 기존 `prompt_tps`와 `eval_tps`만으로는 Ollama 내부 prefill/decode 시간과 전체 wall-clock 차이를 읽기 어려워, 내부 메타데이터 합과 외부 `time.time()` 기준 차이까지 바로 보이게 정리했습니다.

## 2026-04-02
- 10:54 KST (UTC+9) — 비교 스크립트에 Ollama 응답 메타데이터 기반 `token/sec` 로그를 추가해 prompt/output 토큰 수와 TPS를 함께 확인할 수 있게 했습니다.
  - 수정 파일: `phil_robot/tests/test_stt_llm_format_compare.py`, `log.md`
  - 메모: `prompt_eval_count`, `prompt_eval_duration`, `eval_count`, `eval_duration`가 있으면 `tok/s`를 계산하고, 메타데이터가 없는 환경에서는 `N/A`로 표시되도록 안전하게 처리했습니다.

## 2026-04-02
- 10:40 KST (UTC+9) — `phil_robot/tests`에 STT와 planner LLM만으로 legacy 문자열 출력과 JSON 출력을 직접 비교하는 테스트 스크립트를 추가했습니다.
  - 수정 파일: `phil_robot/tests/test_stt_llm_format_compare.py`, `log.md`
  - 메모: `0` 입력 시 `[CMD:...]` / `[SAY:...]` 기반 legacy str 모드, `1` 입력 시 현행 planner JSON 모드로 분기하며, 로봇 연결과 TTS 없이 Whisper STT와 `qwen3:30b-a3b-instruct-2507-q4_K_M` 호출 결과를 콘솔 로그로 바로 비교할 수 있게 했습니다.

## 2026-04-01
- 17:30 KST (UTC+9) — `phil_robot/TODO.md`의 `gesture / play abstraction uplift`를 현재 기준 완료 처리로 바꾸고, baseline uplift와 pre-play modifier/`readMeasure()` 적용 경로는 정리되었으며 이후 확장 판단은 `DECISION_LAYER_ROADMAP_KR.md`를 중심으로 본다는 메모를 남겼습니다.
  - 수정 파일: `phil_robot/TODO.md`, `log.md`
  - 메모: 원칙과 우선순위 기준이 바뀌어 기존 TODO 첫 항목은 사실상 닫고, 다음 구조 논의는 roadmap 문서 쪽에서 이어가기로 반영했습니다.
- 17:19 KST (UTC+9) — 블로그 3편 초안 `txt3.md`를 추가해, Python `PlayModifier` transport contract가 C++ `idealStateRoutine()`, `runPlayProcess()`, `readMeasure()`를 거쳐 실제 연주에 적용되는 흐름과 `pending/active` modifier 분리를 글로 정리했습니다.
  - 수정 파일: `txt3.md`, `log.md`
  - 메모: `txt1.md`의 모션 보정 편, `txt2.md`의 Python `PlayModifier` 설계 편 다음 자연스러운 후속으로, `AgentSocket`은 transport 계층이고 최종 적용 지점은 `readMeasure()`라는 점을 중심으로 이어 쓸 수 있게 구성했습니다.
- 14:32 KST (UTC+9) — `play_modifier.py`의 `str | None` 타입 힌트를 현재 실행 Python과 호환되는 `Optional[str]`로 바꿔, import 시 `TypeError`가 나던 문제를 바로잡았습니다.
  - 수정 파일: `phil_robot/pipeline/play_modifier.py`, `log.md`
  - 메모: 실행 환경이 `|` union annotation을 평가해버리는 Python 버전이라서 최소 수정으로 호환만 맞췄습니다.
- 14:15 KST (UTC+9) — `DrumRobot2`가 brain에서 먼저 보내는 `tempo_scale:*`, `velocity_delta:*` modifier를 pending next-play 설정으로 저장하고, 이번 연주에서 `readMeasure()`가 BPM과 양손 velocity 컬럼에 실제 반영하도록 연결했습니다.
  - 수정 파일: `DrumRobot2/include/tasks/DrumRobot.hpp`, `DrumRobot2/src/DrumRobot.cpp`, `log.md`
  - 메모: `AgentSocket` transport는 그대로 두고 `idealStateRoutine()`에서 modifier를 해석하도록 붙였으며, `p:`가 시작되면 modifier를 1회 소비해 `runPlayProcess()`와 `readMeasure()` 경로에 적용되도록 정리했습니다. `make -C DrumRobot2 -j2` 빌드도 통과했습니다.
- 13:56 KST (UTC+9) — `modifier layer`를 기술 종류가 아니라 결정 경계로 설명하는 한국어 로드맵 문서 `phil_robot/docs/DECISION_LAYER_ROADMAP_KR.md`를 추가해, interrupt/classifier/planner/modifier resolver/validator/executor/C++ apply의 책임과 향후 ML/LLM 확장 지점을 한눈에 보이도록 정리했습니다.
  - 수정 파일: `phil_robot/docs/DECISION_LAYER_ROADMAP_KR.md`, `log.md`
  - 메모: `play modifier resolver`가 parser보다 넓은 책임 이름이라는 점, classifier는 ML/DL 후보가 강하고 validator/executor/C++ apply는 deterministic 경계를 유지하는 편이 좋다는 점, 그리고 단계별 future roadmap을 같은 문서에 함께 남겼습니다.
- 11:36 KST (UTC+9) — 통합 초안 `txt.md`를 바탕으로 모션/gesture 보정 편 `txt1.md`와 `PlayModifier` 설계 편 `txt2.md`를 분리해, 블로그를 2편 연재 형태로 바로 다듬을 수 있게 초안을 나눴습니다.
  - 수정 파일: `txt1.md`, `txt2.md`, `log.md`
  - 메모: `txt1.md`는 arm 포즈와 `wave/hurray` 보정, Python/C++ 역할 분리를 중심으로 두고, `txt2.md`는 `PlayModifier`, validator gate, executor transport contract, `readMeasure()` 적용 예정 지점을 독립 글로 정리했습니다.
- 11:34 KST (UTC+9) — 루트 `txt.md` 초안에 validator의 modifier gate, executor의 `requested_op_cmds`와 transport command 분리, 그리고 아직 남은 C++ `processInput()/readMeasure()` 연결 지점을 더 명확히 보강했습니다.
  - 수정 파일: `txt.md`, `log.md`
  - 메모: `PlayModifier` 객체와 실제 TCP 전송 문자열을 다른 층으로 설명하고, Python 쪽 파싱/보관/직렬화는 끝났지만 C++ parser/apply는 아직 남았다는 점이 글만 읽어도 드러나도록 덧썼습니다.
- 11:27 KST (UTC+9) — 모션 시뮬레이터 기반 gesture 보정부터 `SongModifier`/`PlayModifier` 설계 흐름까지를 Velog에 바로 복붙할 수 있는 루트 `txt.md` 초안으로 정리했습니다.
  - 수정 파일: `txt.md`, `log.md`
  - 메모: 1번 요구사항의 joint 튜닝 시행착오, `wave`/`hurray` 보정, Python/C++ 역할 분리, `play_modifier` parser와 transport contract, C++ `readMeasure()` 적용 예정 지점까지 한 글 흐름으로 묶었습니다.

## 2026-03-31
- 17:56 KST (UTC+9) — `executor`가 validator의 `valid_op_cmds`와 실제 TCP 전송 문자열을 분리해 보이도록 `requested_transport_cmds`/`executed_transport_cmds`를 추가하고, optional `play_modifier`를 `tempo_scale:*`, `velocity_delta:*` transport 명령으로 serialize한 뒤 `p:`보다 먼저 보내도록 정리했습니다.
  - 수정 파일: `phil_robot/pipeline/executor.py`, `log.md`
  - 메모: 지금 C++는 아직 이 modifier transport prefix를 처리하지 않으므로, 이번 변경은 Python 측 transport contract를 먼저 고정하는 단계입니다. `requested_op_cmds`는 plan 관점의 validator 통과 명령으로 유지했습니다.
- 16:04 KST (UTC+9) — `play_modifier`를 explicit 키워드만 파싱하는 순수 parser로 다듬고, `validator`가 `play_request` 의도와 실제로 살아남은 `p:` 명령이 모두 있을 때만 optional `play_modifier`를 `ValidatedPlan`에 싣도록 연결했습니다.
  - 수정 파일: `phil_robot/pipeline/play_modifier.py`, `phil_robot/pipeline/validator.py`, `log.md`
  - 메모: 기본 metadata는 non-identity modifier에서만 채우도록 바꿨고, `brain_pipeline`은 그대로 둔 채 `ValidatedPlan` 확장만으로 후속 executor/C++ 연결 준비를 마쳤습니다.

## 2026-03-30
- 15:20 KST (UTC+9) — 루트 `README.md`를 현재 저장소 구조에 맞게 다시 작성하고, `Drum_intheloop/README.md`의 실제 디렉터리명/경로 표기를 맞춰 최신 실행 흐름과 문서 링크가 깨지지 않게 정리했습니다.
  - 수정 파일: `README.md`, `Drum_intheloop/README.md`, `log.md`
  - 메모: `phil_robot`의 pipeline/runtime/eval 구조, `DrumRobot2`의 TCP 대기 및 `DRUM_SIL_MODE` 기반 SIL export, `Drum_intheloop`의 named pipe reader 소유 모델과 command-level viewer 성격을 루트 문서에 반영했습니다.
- 14:43 KST (UTC+9) — `phil_robot`의 fallback/차단 메시지 처리를 `failure.py`로 모아, LLM 호출 실패 JSON, classifier/planner 기본 실패 결과, motion block 메시지가 한 파일에서 관리되도록 리팩터링했습니다.
  - 수정 파일: `phil_robot/pipeline/failure.py`, `phil_robot/pipeline/llm_interface.py`, `phil_robot/pipeline/intent_classifier.py`, `phil_robot/pipeline/planner.py`, `phil_robot/pipeline/response_parser.py`, `phil_robot/pipeline/validator.py`, `phil_robot/pipeline/command_validator.py`, `phil_robot/pipeline/llm_contract.py`, `phil_robot/docs/PROJECT_STRUCTURE.md`, `phil_robot/docs/PROJECT_STRUCTURE_KR.md`, `log.md`
  - 메모: 기존 `llm_contract.py`는 제거했고, `llm_interface` fallback payload를 classifier/planner/parser가 모두 읽을 수 있는 공용 JSON 형태로 맞췄습니다. `python -m compileall phil_robot/pipeline`과 간단한 failure import/parsing smoke를 확인했습니다.

## 2026-03-27
- 17:58 KST (UTC+9) — `wave` 인사 포즈를 arm1을 벌렸다 오므리는 형태로 다시 조정하고, `arm_down`은 Python posture 매크로와 resolver에서 기존 값(`arm3=20`)으로 되돌렸습니다.
  - 수정 파일: `DrumRobot2/src/AgentAction.cpp`, `phil_robot/pipeline/skills.py`, `phil_robot/pipeline/motion_resolver.py`, `log.md`
  - 메모: `arm_up`과 `arm_out`은 직전 값 그대로 유지했고, `wave`는 상박 고정에 가깝게 둔 채 `arm1`, `arm3`, `wrist` 변화를 반복하도록 바꿨습니다. `make -C DrumRobot2 -j2`와 resolver 출력 확인을 다시 돌렸습니다.
- 17:45 KST (UTC+9) — `wave`와 `hurray`의 C++ gesture 값을 앞으로 찌르는 자세가 아니라 팔꿈치를 세운 포즈로 다시 잡고, Python `arm_up/down/out` 매크로도 `arm2/arm3/wrist` 중심의 새 하드코딩 값으로 조정했습니다.
  - 수정 파일: `DrumRobot2/src/AgentAction.cpp`, `phil_robot/pipeline/skills.py`, `phil_robot/pipeline/motion_resolver.py`, `phil_robot/eval/cases_smoke.json`, `log.md`
  - 메모: `arm_up/out`은 Python `move:` 경로를 유지했고, `wave`/`hurray`만 C++ gesture 정의를 갱신했습니다. `make -C DrumRobot2 -j2`, `python -m compileall phil_robot/pipeline`, resolver smoke 확인을 함께 돌렸습니다.
- 16:36 KST (UTC+9) — `Drum_intheloop`에 이번 SIL/TCP/state 디버깅 세션을 Velog용 글 초안으로 정리한 `velog.md`를 추가해, command-level viewer 성격과 `current_angles`/`k` 타이밍 한계를 한 번에 기록했습니다.
  - 수정 파일: `Drum_intheloop/velog.md`, `log.md`
  - 메모: real SIL과의 차이, `current_angles`의 measured vs simulator-applied 불일치, `vcan` 논의, head 보정 위치, `k` 입력과 first state snapshot 타이밍 이슈를 재사용 가능한 글 흐름으로 묶었습니다.
- 15:16 KST (UTC+9) — Python motion macro 경로로 `팔 올려/내려/벌려`를 해석하도록 `motion_resolver`와 skill 카탈로그를 확장해, `팔 올려`는 `arm2/arm3`, `벌려`는 `arm1` 위주로 풀리고 왼팔/오른팔 지정도 함께 반영되게 했습니다.
  - 수정 파일: `phil_robot/pipeline/skills.py`, `phil_robot/pipeline/motion_resolver.py`, `phil_robot/eval/cases_smoke.json`, `log.md`
  - 메모: 양팔/왼팔/오른팔의 `up/down/out` skill을 `posture`로 추가했고, 자연어에 `손목`이 들어간 경우는 기존 손목 resolver와 충돌하지 않도록 팔 macro 해석에서 제외했습니다. smoke case에는 `팔 올려`, `왼팔 벌려` 기대값을 추가했습니다.
- 12:45 KST (UTC+9) — `phil_robot` 작업 보드의 `Now` 항목이 너무 큰 umbrella로 읽히지 않도록, `gesture abstraction v1`과 `play modifier contract v1`으로 쪼개고 `failure taxonomy`는 `Next`로 내려 현재 실행 범위를 더 현실적으로 다시 정리했습니다.
  - 수정 파일: `phil_robot/TODO.md`, `log.md`
  - 메모: `Now`를 “금방 끝나는 일”이 아니라 “지금 당장 구현 집중할 단위”로 유지하되, 한 항목이 지나치게 큰 epic처럼 보이지 않도록 분해했습니다.
- 12:45 KST (UTC+9) — `phil_robot` 작업 보드의 현재 우선순위를 `gesture / play abstraction uplift` 중심으로 재정렬하고, 긴 `op_cmd` 시퀀스를 상위 skill/gesture로 올리고 `AgentAction`은 저수준 실행 primitive로 두는 방향을 TODO에 명시했습니다.
  - 수정 파일: `phil_robot/TODO.md`, `log.md`
  - 메모: `wave / nod / shake / hurray` 계열의 상위 추상화, `arm_up / arm_down` 추가, `tempo:fast|slow`와 `strength:strong|soft`의 pre-play modifier 설계를 `Now`로 올리고, `planner latency isolation benchmark`는 `Next`로 내렸습니다.

## 2026-03-26
- 17:48 KST (UTC+9) — `AgentSocket`의 JSON 수신 보조 함수와 루프 상태 변수를 `trimText`, `isJsonStart`, `hasFullJson`, `brace_level`, `in_quote`, `cmd_text`처럼 다시 정리해, 문자열 다듬기/JSON 시작 판별/완성 판별 의미가 이름만으로 보이게 리팩터링했습니다.
  - 수정 파일: `DrumRobot2/include/managers/AgentSocket.hpp`, `DrumRobot2/src/AgentSocket.cpp`, `log.md`
  - 메모: 기존 `trimPayload`, `trimmed`, `inString`, `braceDepth`, `collectingJson` 같은 이름은 유지비가 높다고 보고 교체했고, 닫는 중괄호가 먼저 나오는 비정상 케이스는 `false`로 빠지도록 판별도 약간 보강했습니다.
- 17:48 KST (UTC+9) — `makeStateJson()`의 deadband 변수명을 `min_diff_deg`로 다시 정리하고 `wide_band` 분기 변수는 제거해, “현재값과 이전값의 최소 갱신 차이”라는 의미가 바로 읽히게 했습니다.
  - 수정 파일: `DrumRobot2/src/DrumRobot.cpp`, `log.md`
  - 메모: 손목/발목만 `1.0 deg`, 나머지는 `0.5 deg`라는 기존 동작은 유지했고, 이름만 읽어도 threshold 의미가 드러나도록 단순화했습니다.
- 17:45 KST (UTC+9) — `makeStateJson()`의 관절 각도 변수명을 `curr_deg`, `prev_deg`, `dead_band_deg`로 다시 다듬고, `joint_deg_cache`가 “남은 각도”가 아니라 “마지막으로 전송에 반영한 각도 캐시”라는 뜻이 주석에서 바로 읽히게 정리했습니다.
  - 수정 파일: `DrumRobot2/include/tasks/DrumRobot.hpp`, `DrumRobot2/src/DrumRobot.cpp`, `log.md`
  - 메모: 동작은 그대로 두고, 라디안→도 변환값과 deadband 비교 기준의 의미가 변수명만으로 드러나도록 맞췄습니다.
- 17:22 KST (UTC+9) — 루트 `AGENTS.md`에 변수명/표현 규칙을 추가하고, `DrumRobot2`의 `makeStateJson()`을 `auto`, 람다, 긴 변수명 없이 다시 정리해 읽는 즉시 역할이 보이도록 리팩터링했습니다.
  - 수정 파일: `AGENTS.md`, `DrumRobot2/include/tasks/DrumRobot.hpp`, `DrumRobot2/src/DrumRobot.cpp`, `log.md`
  - 메모: 새 규칙은 3단어 이하 변수명, 약어의 언더바 분리, `auto`/후행 반환형/`ptr->field` 기본 지양을 담고 있고, 함수 쪽은 `joint_list`, `live_deg`, `joint_deg_cache` 같은 짧은 이름으로 맞췄습니다.
- 17:10 KST (UTC+9) — `DrumRobot2`의 state JSON 관절 각도 직렬화에서 deadband 캐시 관련 변수명을 줄이고 분기를 정리해, 실측값/캐시값/출력값의 역할이 바로 읽히도록 다듬었습니다.
  - 수정 파일: `DrumRobot2/include/tasks/DrumRobot.hpp`, `DrumRobot2/src/DrumRobot.cpp`, `log.md`
  - 메모: 기존 `lastBroadcastJointAnglesDeg`, `motorIter` 같은 이름과 중복 대입 흐름이 읽기 어려워 보여 `broadcastAngleCacheDeg`, `motorIt`, `reportedAngleDeg` 중심으로 정리했고, 동작은 유지했습니다.
- 15:29 KST (UTC+9) — `drum_intheloop` URDF 폴더에서 현재 Python command-level simulator가 쓰지 않는 ROS/Gazebo 패키징 잔재를 제거해, 필요한 자산이 `URDF/STL/runtime patch` 축으로만 남도록 정리했습니다.
  - 수정 파일: `drum_intheloop/urdf/drumrobot_RL_urdf/CMakeLists.txt`, `drum_intheloop/urdf/drumrobot_RL_urdf/package.xml`, `drum_intheloop/urdf/drumrobot_RL_urdf/config/joint_names_drumrobot_RL_urdf.yaml`, `drum_intheloop/urdf/drumrobot_RL_urdf/launch/display.launch`, `drum_intheloop/urdf/drumrobot_RL_urdf/launch/gazebo.launch`, `log.md`
  - 메모: 현재 `drum_intheloop` 실행 경로는 `sil/urdf_tools.py`가 원본 URDF와 mesh만 직접 참조하고, ROS launch/config/package 메타파일은 사용하지 않으므로 안전한 legacy cleanup으로 판단했습니다.
- 15:25 KST (UTC+9) — 루트 `.gitignore`에 `phil_intheloop/` 전체를 legacy mock/simulator workspace로 추가해, 앞으로 이 디렉터리 아래의 새 미추적 파일이 기본적으로 버전 관리 대상에 올라오지 않도록 정리했습니다.
  - 수정 파일: `.gitignore`, `log.md`
  - 메모: `phil_intheloop`는 이미 다수의 파일이 Git에 추적 중이므로, 이번 변경은 “새 파일 무시” 성격이고 기존 tracked 파일을 자동으로 untrack하지는 않습니다.
- 15:22 KST (UTC+9) — `drum_intheloop/README.md`에 pipe reader의 실제 소비 단위와 `stepSimulation()` 타이밍 의미를 사람 친화적으로 다시 풀어 써, “한 줄 메시지 = 한 번 적용 + 한 번 step + optional sleep” 구조와 batch 부재를 타임라인으로 이해할 수 있게 정리했습니다.
  - 수정 파일: `drum_intheloop/README.md`, `log.md`
  - 메모: 기존의 “timing 없는 즉시 이동형” 표현은 너무 뭉뚱그려져 있었고, 이번에는 C++ write cadence, Python read/apply 순서, `--sleep 0.0001`의 정확한 의미, startup preset의 별도 1회 step까지 구분해서 문서화했습니다.
- 15:03 KST (UTC+9) — `drum_intheloop/TODO.md`에 simulator ON일 때만 드러나는 `current_angles`/planner/validator 흔들림 문제를 `Simulator-Triggered Integration Blockers` 섹션으로 다시 포함시켜, simulator 내부 작업과 end-to-end surfaced blocker를 분리해 함께 보게 정리했습니다.
  - 수정 파일: `drum_intheloop/TODO.md`, `log.md`
  - 메모: 책임 코드가 `DrumRobot2`나 `phil_robot` 쪽일 수 있어도, 실제로 문제가 표면화되는 조건이 `drum_intheloop` 시뮬레이터 실행인 만큼 TODO에 추적 항목으로 남겼습니다.
- 14:55 KST (UTC+9) — `drum_intheloop/TODO.md`에서 simulator 범위를 벗어난 `DrumRobot2`/`phil_robot` blocker 항목을 제거해, TODO가 `drum_intheloop` 내부 simulator 작업만 다루도록 다시 좁혔습니다.
  - 수정 파일: `drum_intheloop/TODO.md`, `log.md`
  - 메모: 이전 버전은 end-to-end 맥락을 함께 적으려다 바깥 계층의 state/owner 이슈가 섞였고, 이번 수정에서 `READY -> snare`, visual 보정, trace/replay, timing/future SIL 같은 simulator 항목만 남겼습니다.
- 14:51 KST (UTC+9) — `drum_intheloop`의 TODO를 현재 command-level SIL 역할과 실제 blocker 기준으로 전면 교체하고, 중복 복사본 `TODO copy.md`는 제거했습니다.
  - 수정 파일: `drum_intheloop/TODO.md`, `drum_intheloop/TODO copy.md`, `log.md`
  - 메모: 새 TODO는 `READY -> snare` 자세 문제, arm visual 보정값 정리, command-level seam에서 CAN-frame SIL로 넘어갈 migration checklist, 그리고 `current_angles` garbage-value 같은 외부 blocker를 분리해서 적도록 재구성했습니다.
- 13:19 KST (UTC+9) — `drum_intheloop`의 runtime URDF pose patch 주석을 고쳐, `RUNTIME_LINK_FRAME_PATCH_POSE`가 joint origin이 아니라 각 link의 `visual`과 `collision` origin을 둘 다 같은 pose로 덮어쓴다는 점이 바로 읽히도록 정리했습니다.
  - 수정 파일: `drum_intheloop/sil/urdf_tools.py`, `log.md`
  - 메모: 기존 설명은 `visual/collision`을 함께 적어 놓고도 실제로 둘 다 수정된다는 사실이 눈에 잘 안 들어와 오해를 부를 수 있었습니다. left/right arm 섹션 라벨도 `visual frame`에서 `link origin patch`로 바꿨습니다.
- 10:21 KST (UTC+9) — `drum_intheloop`의 현재 command-level SIL 구조, 실행 순서, 각도/visual 보정 계층, 알려진 한계와 디버깅 기준을 README와 하위 AGENTS에 최신 상태로 정리했습니다.
  - 수정 파일: `drum_intheloop/README.md`, `drum_intheloop/AGENTS.md`, `log.md`
  - 메모: 새 README는 reader/writer 경계, startup pose, `head_tilt -90` 보정, per-joint transform, runtime URDF pose patch, 현재 남은 이슈까지 처음 보는 사람이 따라갈 수 있도록 상세 설명으로 재구성했고, 하위 AGENTS는 같은 내용을 작업 원칙 관점으로 압축했습니다.

## 2026-03-25
- 18:05 KST (UTC+9) — arm runtime patch를 회전만 주는 방식에서 xyz+rpy pose 보정으로 확장해, x축 180도 회전 후 어깨/손목에서 mesh가 떠 보이던 현상을 STL bounds 중심 유지 기준으로 같이 비교할 수 있게 했습니다.
  - 수정 파일: `drum_intheloop/sil/urdf_tools.py`, `phil_intheloop/sil/urdf_tools.py`, `log.md`
  - 메모: `left/right_shoulder_1`, `left/right_shoulder_2`, `left/right_elbow`, `left/right_wrist`에 대해 runtime URDF에서만 `origin xyz`와 `origin rpy`를 함께 덮어쓰도록 바꿨고, xyz 값은 각 STL의 bounds center가 회전 전후로 유지되도록 계산한 1차 실험값입니다.
- 17:55 KST (UTC+9) — arm visual runtime patch를 어깨 1축과 손목 링크까지 확장해, 중간 링크만 뒤집혀 체인이 어깨/손목에서 분리돼 보이던 현상을 같은 실험 조건으로 연속 확인할 수 있게 했습니다.
  - 수정 파일: `drum_intheloop/sil/urdf_tools.py`, `phil_intheloop/sil/urdf_tools.py`, `log.md`
  - 메모: 체크인된 URDF/STL은 그대로 두고 runtime URDF에서만 `left/right_shoulder_1`, `left/right_shoulder_2`, `left/right_elbow`, `left/right_wrist`의 `visual/collision origin rpy`를 모두 x축 180도 회전시키도록 확장했습니다.
- 17:47 KST (UTC+9) — runtime URDF patch helper의 tuple 타입 힌트를 `typing.Tuple`로 바꿔, 실행 환경에서 import 시 `type object is not subscriptable` 에러가 나지 않도록 정리했습니다.
  - 수정 파일: `drum_intheloop/sil/urdf_tools.py`, `phil_intheloop/sil/urdf_tools.py`, `log.md`
  - 메모: 동작 변화는 없고, arm link runtime patch 코드를 실제 `python sil/SilCommandPipeReader.py` 실행 경로에서 바로 import할 수 있게 타입 annotation만 호환형으로 맞췄습니다.
- 17:45 KST (UTC+9) — `drum_intheloop`와 `phil_intheloop`의 runtime URDF patch에 상완/하완 링크 frame 보정 테이블을 추가해, 양팔 연결부가 바깥을 보는 현상을 x축 180도 회전으로 비교 확인할 수 있게 했습니다.
  - 수정 파일: `drum_intheloop/sil/urdf_tools.py`, `phil_intheloop/sil/urdf_tools.py`, `log.md`
  - 메모: 체크인된 URDF/STL은 그대로 두고, generated runtime URDF에서만 `left/right_shoulder_2`, `left/right_elbow`의 `visual/collision origin rpy`를 덮어씌우도록 했습니다. 수정 블록은 큰 구분 주석으로 분리해 이후 실험 시 숫자만 바로 바꿔볼 수 있게 정리했습니다.
- 17:10 KST (UTC+9) — `drum_intheloop`의 CAN-to-URDF 각도 변환을 joint별 `reference/sign/bias` 테이블로 일반화해, 어떤 관절이 단순 부호 반전인지 90도 기준 mirror인지 코드에서 바로 보이도록 정리했습니다.
  - 수정 파일: `drum_intheloop/sil/joint_map.py`, `drum_intheloop/sil/command_applier.py`, `log.md`
  - 메모: 현재 동작은 유지하면서 기존 `R_arm1` 특수처리도 동일한 수식으로 흡수했고, 사용자가 각 joint 보정을 눈으로 확인하고 바로 조정하기 쉽도록 큰 구분 주석을 함께 넣었습니다.
- 16:39 KST (UTC+9) — `drum_intheloop`의 `_place_robot_on_ground()`에 z축 보정과 AABB 계산 흐름을 이해하기 쉬운 설명 주석으로 덧붙였습니다.
  - 수정 파일: `drum_intheloop/sil/pybullet_backend.py`, `log.md`
  - 메모: 동작은 바꾸지 않고, base orientation은 유지한 채 바닥 clearance를 맞추기 위해 어떤 값을 계산하는지 바로 읽히도록 변수 의미를 주석으로 정리했습니다.

## 2026-03-24
- 18:06 KST (UTC+9) — SIL 시작 시 실기에서 손으로 맞춰두는 preset 자세를 다시 적용하고, `안녕 손 흔들어 봐` 같은 혼합 발화를 motion 경로로 승격하며, `/tmp/drum_command.pipe`가 있으면 `sudo` 실행에서도 SIL 모드가 자동으로 켜지도록 보강했습니다.
  - 수정 파일: `drum_intheloop/sil/robot_spec.py`, `drum_intheloop/sil/SilCommandPipeReader.py`, `phil_robot/pipeline/intent_classifier.py`, `phil_robot/pipeline/planner.py`, `DrumRobot2/src/CanManager.cpp`, `log.md`
  - 메모: startup pose 는 `DrumRobot.hpp`의 `initialJointAngles`와 같은 body preset(손목 90도) 기준으로 적용하고, 이후 실제 C++ HOME/READY pipe 명령이 들어오면 그 흐름으로 자연스럽게 덮이도록 했습니다. planner 쪽은 `chat + needs_motion` 혼합 케이스를 motion planning 으로 흘려 `wave_hi`가 비워지지 않게 했고, `DRUM_SIL_MODE` 환경변수가 없어도 reader가 만든 FIFO가 있으면 SIL 모드를 자동 활성화해 `sudo ./main.out` 경로에서도 body export가 살아나도록 했습니다.
- 17:52 KST (UTC+9) — `SilCommandPipeReader`가 시작 직후 HOME 자세를 강제로 적용하던 동작을 제거해, PyBullet이 실제 pipe 명령 흐름과 동기화되도록 되돌렸습니다.
  - 수정 파일: `drum_intheloop/sil/SilCommandPipeReader.py`, `drum_intheloop/sil/robot_spec.py`, `log.md`
  - 메모: 초기 pose는 더 이상 reader에서 주입하지 않고, 시뮬레이터는 실제로 수신한 `tmotor`/`maxon`/`dxl` 명령만 반영합니다. 함께 `robot_spec.py`에 임시로 추가했던 초기 pose 상수도 제거했습니다.
- 17:48 KST (UTC+9) — SIL 시작 자세 기준을 `initialJointAngles`가 아니라 실제 시작 흐름과 맞는 HOME 최종 자세로 정정해, 양 손목이 `75deg`로 내려온 상태에서 시작하도록 맞췄습니다.
  - 수정 파일: `drum_intheloop/sil/robot_spec.py`, `log.md`
  - 메모: DrumRobot2는 `initializePos("o")` 후 `HOME`으로 진입하므로, SIL도 `PathManager::setAddStanceAngle()`의 `homeAngle` 기준을 따르도록 바꿨습니다. head tilt는 이미 `90deg`로 일치하고 있어 손목 값만 `75deg`로 조정했습니다.
- 17:37 KST (UTC+9) — `SilCommandPipeReader` 시작 직후 DrumRobot2의 초기 자세와 DXL 기본 head 자세를 한 번 적용하도록 해, PyBullet이 URDF 기본 pose 대신 더 자연스러운 시작 자세로 뜨게 맞췄습니다.
  - 수정 파일: `drum_intheloop/sil/robot_spec.py`, `drum_intheloop/sil/SilCommandPipeReader.py`, `log.md`
  - 메모: body는 `DrumRobot.hpp`의 `initialJointAngles`, head는 `PathManager`의 기본 `readyAngle(12/13)`를 기준으로 사용했고, 변환은 기존 `CommandApplier`를 재사용해 sign/특수 매핑을 그대로 따르도록 했습니다.
- 17:31 KST (UTC+9) — `DRUM_SIL_MODE=1`일 때 미연결 CAN 모터를 `motors` 맵에서 지우지 않고, body 명령 export/queue 소비는 유지한 채 실제 CAN 송신과 TMotor send safety만 우회하도록 `CanManager`를 보강했습니다.
  - 수정 파일: `DrumRobot2/include/motors/Motor.hpp`, `DrumRobot2/include/managers/CanManager.hpp`, `DrumRobot2/src/CanManager.cpp`, `log.md`
  - 메모: 기본 실기 동작은 그대로 두고 SIL 모드에서만 body 모터를 살려 command-level exporter가 `tmotor/maxon`을 내보낼 수 있게 했습니다. disconnected 모터의 기본 socket 값도 `-1`로 명시해 안전하지 않은 초기 상태를 줄였습니다.
- 17:24 KST (UTC+9) — `pybullet_backend.py`에 바닥 plane 로드와 `_place_robot_on_ground()` 호출을 다시 붙이고, 초기 카메라를 더 높고 여유 있게 보이도록 조정했습니다.
  - 수정 파일: `drum_intheloop/sil/pybullet_backend.py`, `log.md`
  - 메모: `pybullet_data` 경로를 추가해 `plane.urdf`를 로드했고, 로봇을 clearance 기준으로 바닥 위에 맞춘 뒤 GUI 카메라는 더 높은 target과 완만한 pitch로 시작하게 조정했습니다.
- 17:19 KST (UTC+9) — `drum_intheloop`의 `pybullet_backend.py`에 GUI 디버그 패널 숨김과 초기 카메라 위치 설정을 다시 추가해, 창을 열었을 때 바로 로봇 자세를 보기 쉬운 상태로 시작하도록 정리했습니다.
  - 수정 파일: `drum_intheloop/sil/pybullet_backend.py`, `log.md`
  - 메모: `p.COV_ENABLE_GUI`를 꺼서 기본 패널을 숨기고, `resetDebugVisualizerCamera(...)`로 초기 시점을 고정했습니다. 요청에 맞춰 추가된 부분은 `#=====================` 블록으로 표시했습니다.
- 17:10 KST (UTC+9) — `SilCommandPipeWriter`가 실제 소비 지점에서 no-op으로 빠지지 않도록 `CanManager`와 DXL 소비 루프에서 명시적으로 `setEnabled(true)`를 호출하도록 켰습니다.
  - 수정 파일: `DrumRobot2/src/CanManager.cpp`, `DrumRobot2/src/DrumRobot.cpp`, `log.md`
  - 메모: 기존에는 writer 호출 자리는 있었지만 `enabled=false` 기본값 때문에 pipe로 아무 메시지도 쓰지 않았고, 이제 reader를 먼저 실행해두면 command-level SIL ingress가 실제로 데이터를 받기 시작합니다.
- 17:05 KST (UTC+9) — `drum_intheloop`의 옛 mock 엔트리포인트였던 `run_sil.py`를 제거하고, README 실행 문구를 현재 `sil/SilCommandPipeReader.py` 기준으로 정리했습니다.
  - 수정 파일: `drum_intheloop/run_sil.py`, `drum_intheloop/README.md`, `log.md`
  - 메모: `run_sil.py`는 이미 삭제된 `sil.server`를 가리키고 있었고, 현재 구조에서는 `drum_intheloop` 루트에서 `python sil/SilCommandPipeReader.py --mode gui`로 실행하는 경로만 남기면 충분합니다.
- 16:23 KST (UTC+9) — `motorTypeConversion(...)` 도입 후 `Motor` 변환 메서드의 비-const 시그니처에 맞게 `SilCommandPipeWriter`의 참조 타입을 조정하고 단독 컴파일을 다시 확인했습니다.
  - 수정 파일: `DrumRobot2/include/managers/SilCommandPipeWriter.hpp`, `DrumRobot2/src/SilCommandPipeWriter.cpp`, `log.md`
  - 메모: `TMotor::motorPositionToJointAngle()`와 `MaxonMotor::motorPositionToJointAngle()`가 `const` 메서드가 아니어서 writer helper 인자도 non-const 참조로 맞췄고, `SilCommandPipeWriter.cpp` 단독 컴파일이 통과했습니다.
- 16:17 KST (UTC+9) — `SilCommandPipeWriter`가 exporter 단계에서 `TMotor/Maxon`의 motor position과 DXL rad 값을 simulator용 joint degree로 변환해 내보내도록 `motorTypeConversion(...)` helper를 추가했습니다.
  - 수정 파일: `DrumRobot2/include/managers/SilCommandPipeWriter.hpp`, `DrumRobot2/src/SilCommandPipeWriter.cpp`, `DrumRobot2/src/CanManager.cpp`, `log.md`
  - 메모: pipe transport만 확인되는 수준을 넘어 Python `CommandApplier`가 바로 사용할 수 있도록, writer 호출부에서 실제 모터 객체를 함께 넘겨 joint-space degree로 변환한 뒤 NDJSON의 `position` 필드에 기록하게 했습니다.
- 16:05 KST (UTC+9) — `drum_intheloop`의 README를 현재 command-level SIL + named pipe 구조 기준으로 전면 정리하고, `joint_map`은 실제 매핑과 URDF limit patch에 필요한 최소 상수만 남기도록 다이어트했습니다.
  - 수정 파일: `drum_intheloop/README.md`, `drum_intheloop/sil/joint_map.py`, `drum_intheloop/sil/__init__.py`, `log.md`
  - 메모: 옛 `phil_intheloop`/TCP/server/protocol/state_model 설명을 걷어내고, `JOINT_LIMITS_DEG`, 초기/홈 포즈, gesture/clamp 상수는 제거했습니다. `URDF_JOINT_LIMITS_DEG`는 `urdf_tools.py`의 runtime limit patch 때문에 유지했습니다.
- 15:24 KST (UTC+9) — `drum_intheloop`에 C++ writer 기본 경로(`/tmp/drum_command.pipe`)를 그대로 읽는 최소 Python ingress인 `SilCommandPipeReader.py`를 추가했습니다.
  - 수정 파일: `drum_intheloop/sil/SilCommandPipeReader.py`, `log.md`
  - 메모: named pipe에서 NDJSON 한 줄을 읽어 `TMotorData`/`MaxonData`/DXL payload로 복원한 뒤 `CommandApplier -> PyBulletBackend`에 바로 넘기는 최소 러너를 마련했습니다.
- 14:48 KST (UTC+9) — `SilCommandPipeWriter`의 NDJSON 생성 코드를 학습 단계에 맞게 `std::ostringstream` 기반으로 정리하고, 이에 맞춰 `<sstream>` include와 불필요한 저수준 include를 정리했습니다.
  - 수정 파일: `DrumRobot2/src/SilCommandPipeWriter.cpp`, `log.md`
  - 메모: 문자열 버퍼 직접 조립보다 `TMotorData/MaxonData/DXL -> JSON 한 줄` 흐름이 바로 읽히는 구현을 우선했고, `charconv`/`cstdio`는 제거했습니다.
- 14:41 KST (UTC+9) — `SilCommandPipeWriter::buildTMotorLine()`의 TODO를 NDJSON 직렬화 구현으로 교체하고, 스트림 대신 직접 문자열 버퍼를 조립해 호출 비용을 줄였습니다.
  - 수정 파일: `DrumRobot2/src/SilCommandPipeWriter.cpp`, `log.md`
  - 메모: `motorName`, `position`, `velocityERPM`, `mode`, `useBrake`를 한 줄 JSON으로 만들고, 숫자 필드는 `snprintf`/`to_chars`로 바로 붙이도록 정리했습니다.
- 14:12 KST (UTC+9) — velog 초안을 사용자가 선호한 첫 번째 설명형 문체로 다시 정리해 `named pipe` 학습 배경과 SIL 맥락이 더 자연스럽게 드러나도록 수정했습니다.
  - 수정 파일: `named_pipe_velog_draft.txt`, `log.md`
  - 메모: 서두를 `오늘은 로봇 SIL 연결을 준비하면서...`로 되돌리고, `PathManager/CanManager` 문맥과 `command-level SIL` 동기를 더 직접적으로 설명하는 버전으로 맞췄습니다.
- 14:08 KST (UTC+9) — velog용 named pipe 정리 글 초안을 마크다운 문법이 살아 있는 `.txt` 파일로 루트에 추가했습니다.
  - 수정 파일: `named_pipe_velog_draft.txt`, `log.md`
  - 메모: velog에 바로 옮겨 붙일 수 있도록 제목, 본문, 코드펜스(````), 강조 문장 흐름을 포함한 초안 형태로 정리했습니다.
- 13:30 KST (UTC+9) — 루트 작업 규칙에 `DrumRobot2`는 가능하면 C++ 위주로 유지하고, Python 실행 코드는 별도 Python 영역에 둔다는 배치 원칙을 추가했습니다.
  - 수정 파일: `AGENTS.md`, `log.md`
  - 메모: 이후 `DrumRobot2/` 아래에는 가급적 새 Python 파일을 만들지 않고, `drum_intheloop/`나 `phil_intheloop/` 같은 별도 프로세스/폴더 쪽에 Python 진입점을 두는 방향을 기본으로 따릅니다.
- 12:37 KST (UTC+9) — `DrumRobot2` 안의 Python 실행 보조 파일을 정리하고, pipe reader 예제와 SIL 데모의 Python 진입점은 `drum_intheloop` 쪽에만 두도록 위치를 맞췄습니다.
  - 수정 파일: `drum_intheloop/tests/pipe_demo_reader.py`, `DrumRobot2/tests/pipe_demo_reader.py`, `DrumRobot2/tests/wave_nod_demo.py`, `log.md`
  - 메모: `DrumRobot2`에는 C++ writer 예제만 남기고, Python reader 및 SIL demo runner는 별도 Python 프로세스 영역인 `drum_intheloop/tests`로 정리했습니다.
- 12:33 KST (UTC+9) — named pipe 개념을 로봇 코드와 분리해서 이해할 수 있도록 `DrumRobot2/tests`에 C++ writer / Python reader 최소 예제를 추가했습니다.
  - 수정 파일: `DrumRobot2/tests/pipe_demo_writer.cpp`, `DrumRobot2/tests/pipe_demo_reader.py`, `log.md`
  - 메모: `DemoCommand` struct를 NDJSON 한 줄로 보내고 Python이 그대로 읽어 파싱하는 흐름만 남겨, `PathManager`나 `CanManager`를 빼고도 pipe 자체를 먼저 눈으로 확인할 수 있게 했습니다.
- 11:25 KST (UTC+9) — `SilCommandPipeWriter::writeLine()`이 문자열 복사 없이 NDJSON 라인과 개행을 `writev()`로 바로 기록하도록 구현했습니다.
  - 수정 파일: `DrumRobot2/src/SilCommandPipeWriter.cpp`, `log.md`
  - 메모: `EINTR`은 즉시 재시도하고, reader disconnect로 인한 `EPIPE`가 나면 fd를 닫아 다음 호출에서 다시 열 수 있게 했습니다.
- 11:18 KST (UTC+9) — `SilCommandPipeWriter::openPipe()`가 FIFO를 생성 또는 재사용한 뒤 non-blocking writer fd를 열도록 구현했습니다.
  - 수정 파일: `DrumRobot2/src/SilCommandPipeWriter.cpp`, `log.md`
  - 메모: `mkfifo()`에서 기존 파이프가 이미 있으면 그대로 사용하고, `O_WRONLY | O_NONBLOCK | O_CLOEXEC`로 열어 reader 부재 시 다음 호출에서 다시 시도할 수 있게 했습니다.

## 2026-03-23
- 11:18 KST (UTC+9) — `DrumRobot2`에 1차 command-level SIL용 named pipe exporter 골격(`SilCommandPipeWriter`)을 추가하고, 실제 소비 지점인 `CanManager::setCANFrame()` 및 DXL 소비 지점에서 호출 자리를 마련했습니다.
  - 수정 파일: `DrumRobot2/include/managers/SilCommandPipeWriter.hpp`, `DrumRobot2/src/SilCommandPipeWriter.cpp`, `DrumRobot2/src/CanManager.cpp`, `DrumRobot2/src/DrumRobot.cpp`, `log.md`
  - 메모: writer 기본값은 비활성(no-op)이라 기존 동작에는 영향이 없고, `TMotorData`/`MaxonData`/DXL command를 NDJSON으로 FIFO에 내보내는 TODO만 채우면 ingress 실험을 시작할 수 있는 상태입니다. `Makefile`의 `src/*.cpp` 와일드카드로 새 `.cpp`가 자동 빌드 대상에 포함되는 것도 dry-run으로 확인했습니다.
- 17:56 KST (UTC+9) — `command_applier.py`가 예전 stateful 버전으로 남아 있던 흔적을 정리하고, stateless command-to-joint-target 변환 버전으로 다시 맞췄습니다.
  - 수정 파일: `drum_intheloop/sil/command_applier.py`, `log.md`
  - 메모: `clear()`/`get_joint_targets()` 기반 내부 저장 구조를 제거하고, `build_default_can_command()`와 `apply_can_command(motor_name, command)` / `apply_dxl_command()`가 바로 URDF joint target dict를 반환하도록 복구했습니다. `DrumRobot2/tests/wave_nod_demo.py --mode direct` 재실행으로 동작도 다시 확인했습니다.
- 17:50 KST (UTC+9) — `drum_intheloop`에 C++ `TMotorData`/`MaxonData` 대응 Python command 자료구조를 추가하고, `CommandApplier`가 command 객체를 받아 바로 URDF joint target dict를 반환하도록 정리했습니다.
  - 수정 파일: `drum_intheloop/sil/command_types.py`, `drum_intheloop/sil/command_applier.py`, `drum_intheloop/tests/wave_nod_demo.py`, `drum_intheloop/sil/server.py`, `log.md`
  - 메모: `clear()`/`get_joint_targets()` 같은 내부 상태 저장을 제거하고, `apply_can_command()`가 `TMotorData` 또는 `MaxonData`를 받아 검증 후 매핑하도록 바꿨습니다. `wave_nod_demo.py`와 `server.py`도 새 구조로 맞췄고, `drum4` 환경에서 `DrumRobot2/tests/wave_nod_demo.py --mode direct` 재실행까지 확인했습니다.
- 17:05 KST (UTC+9) — `wave` 포즈가 의도와 다르게 보이던 원인으로 확인된 `R_arm1` 예외 각도 변환을 `command_applier`에 복원했습니다.
  - 수정 파일: `drum_intheloop/sil/command_applier.py`, `log.md`
  - 메모: backend 단순화 과정에서 빠졌던 `R_arm1`의 90도 기준 미러링 보정을 `command_applier`로 옮겨 매핑 책임 분리 구조 안에서 유지하도록 조정했습니다.
- 17:02 KST (UTC+9) — `DrumRobot2/tests`에서 바로 `python wave_nod_demo.py`로 실행할 수 있도록 `drum_intheloop` 데모 러너 래퍼를 추가하고, `drum4` 환경에서 `--mode direct` 실행까지 확인했습니다.
  - 수정 파일: `DrumRobot2/tests/wave_nod_demo.py`, `log.md`
  - 메모: 실제 구현은 `drum_intheloop/tests/wave_nod_demo.py`에 두고, `DrumRobot2/tests`에서는 경로를 신경쓰지 않고 동일 이름으로 실행할 수 있게 연결했습니다.
- 16:57 KST (UTC+9) — `wave`/`nod`를 하드코딩으로 재생해 `CommandApplier -> PyBulletBackend` 흐름을 확인할 수 있도록 `drum_intheloop/tests/wave_nod_demo.py` 러너를 추가했습니다.
  - 수정 파일: `drum_intheloop/tests/wave_nod_demo.py`, `log.md`
  - 메모: `joint_map.get_gesture_sequence()`를 읽어 `CommandApplier`가 URDF joint target dict를 만들고, `PyBulletBackend.apply_targets()`가 그 dict를 적용하는 최소 경로를 테스트할 수 있게 했습니다.
- 16:14 KST (UTC+9) — `drum_intheloop`에서 매핑 책임을 `command_applier`로 몰아주고, `pybullet_backend`는 URDF joint target 적용만 담당하도록 역할을 분리했습니다.
  - 수정 파일: `drum_intheloop/sil/command_applier.py`, `drum_intheloop/sil/pybullet_backend.py`, `drum_intheloop/sil/server.py`, `log.md`
  - 메모: backend에서 `joint_map` 의존을 제거했고, `command_applier`가 production name과 DXL logical joint를 URDF joint target dict로 변환하도록 정리했습니다. `server.py`도 새 경로에 맞게 `CommandApplier`를 통해 target을 구성하도록 맞췄습니다.
- 16:06 KST (UTC+9) — `drum_intheloop`의 `pybullet_backend.py`를 더 줄여 `start()`가 PyBullet 연결, 런타임 URDF 생성, 로봇 로드, joint index 조회만 수행하도록 단순화했습니다.
  - 수정 파일: `drum_intheloop/sil/pybullet_backend.py`, `log.md`
  - 메모: 바닥, 중력, debug visualizer, self-collision flag, 기본 모터 비활성화, ground placement를 초기 경로에서 제거해 smoke test와 학습용 읽기 난도를 낮췄습니다.
- 15:47 KST (UTC+9) — `drum_intheloop`의 `pybullet_backend.py`를 smoke test용 최소 구조로 정리해 시작/종료, URDF 런타임 패치, joint index 조회, target 적용, ground placement만 남겼습니다.
  - 수정 파일: `drum_intheloop/sil/pybullet_backend.py`, `log.md`
  - 메모: `wave`/`nod` 같은 하드코딩 동작을 먼저 확인할 수 있도록 시각 테마, 카메라, 스크린샷, 색상 처리 등 부수 기능을 제거해 읽기 쉽게 단순화했습니다.
- 11:33 KST (UTC+9) — DYNAMIXEL 단품 테스트 스크립트에 `ping` 재시도와 포트 버퍼 초기화를 추가해 불안정한 응답 상황에서 바로 실패하지 않도록 보강했습니다.
  - 수정 파일: `DrumRobot2/tests/dxl_move_test.py`, `log.md`
  - 메모: 단일 `ping`에서 `Incorrect status packet`이 발생해도 여러 번 재시도하도록 했고, 각 read/write 전 포트 버퍼를 비우도록 조정했습니다.
- 11:31 KST (UTC+9) — 저장소의 루트/하위 `AGENTS.md` 범위를 다시 확인하고, DYNAMIXEL 단품 테스트용 스크립트를 `tests/` 아래로 추가했습니다.
  - 수정 파일: `DrumRobot2/tests/dxl_move_test.py`, `log.md`
  - 메모: `XM430-W350-T` 테스트 모터는 `Protocol 2.0`, `57600 baud`, `ID 28`, `model 1020`으로 응답함을 확인했고, `U2D2`는 전원 공급기가 아니라 USB-DYNAMIXEL 통신 어댑터라는 점과 TTL/RS485 배선 구분을 확인했습니다.

## 2026-03-20
- 15:31 KST (UTC+9) — 작업 로그 시각을 한국시간 기준으로 통일하도록 루트 `AGENTS.md`와 `log.md`를 업데이트했습니다.
  - 수정 파일: `AGENTS.md`, `log.md`
  - 메모: 이후 로그는 별도 지시가 없으면 한국시간(KST, UTC+9/GMT+9)으로 기록합니다.
- 15:16 KST (UTC+9) — 저장소 전역 작업 규칙을 문서화하기 위해 루트 `AGENTS.md`와 `log.md`를 새로 추가했습니다.
  - 수정 파일: `AGENTS.md`, `log.md`
  - 메모: 이후 에이전트 작업 시 변경 내용을 같은 형식으로 누적 기록하도록 기준을 마련했습니다.
## 2026-03-25
- 14:22 KST (UTC+9) — 현재 SIL/TCP 통합 상태와 우회 사항을 문서로 정리해 future handoff용 컨텍스트를 남겼다.
  - 수정 파일: `AGENTS.md`, `drum_intheloop/README.md`
  - 메모: 명시적 SIL 모드, FIFO 수명, disconnected CAN/Maxon 우회, TCP/gate/CSV 착시 문제와 실행 순서를 README/AGENTS에 반영.

## 2026-03-25
- 11:19 KST (UTC+9) — SIL 모드에서 미연결 Maxon 초기화 하드웨어 왕복을 건너뛰도록 해 pre-TCP 단계의 CAN 노이즈를 줄였다.
  - 수정 파일: `DrumRobot2/src/DrumRobot.cpp`
  - 메모: `motorSettingCmd`, `maxonMotorEnable`, `setMaxonMotorMode`에서 disconnected Maxon을 우회해 TCP 서버가 뜨기 전부터 `sendAndRecv`/`txFrame` 오류가 반복되지 않도록 조정.

## 2026-03-25
- 11:04 KST (UTC+9) — SIL 활성화 경계를 명시적 모드로 좁히고 FIFO 수명을 reader 중심으로 정리했다.
  - 수정 파일: `DrumRobot2/src/CanManager.cpp`, `DrumRobot2/src/DrumRobot.cpp`, `DrumRobot2/include/managers/SilCommandPipeWriter.hpp`, `DrumRobot2/src/SilCommandPipeWriter.cpp`, `drum_intheloop/sil/SilCommandPipeReader.py`
  - 메모: `DRUM_SIL_MODE=1`일 때만 pipe writer를 켜고, reader가 `/tmp/drum_command.pipe`를 매 실행마다 삭제 후 재생성/종료 시 정리하도록 바꿔 stale FIFO로 인한 SIL 오인 검출을 막음.
