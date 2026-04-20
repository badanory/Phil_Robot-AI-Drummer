# Phil Robot TODO

이 파일은 `phil_robot`의 실행 가능한 작업 보드다.
아이디어를 전부 같은 레벨에 두지 않고, `Now / Next / Later / Parking Lot`으로 강제 분리한다.

운영 규칙:
- `Now`에는 최대 3개만 둔다.
- 새 아이디어가 떠오르면 바로 구현하지 말고 먼저 `Parking Lot`에 적는다.
- `Now`가 비워질 때만 `Next`에서 끌어올린다.
- 체크 완료 시 체크박스를 `x`로 바꾸고, 필요하면 한 줄 메모를 덧붙인다.
- 실험/측정은 가능하면 결과 파일 경로를 같이 남긴다.

## Now

- [ ] `planner 의미 해석 / resolver 계산 분리`
  - 목표: planner는 `absolute / delta / sequence` 의미만 정하고, resolver는 state snapshot 기준 수치 계산만 맡는다.
  - 이유:
    - `scenario_20`, `scenario_21`처럼 연속 상대동작에서 `move`를 다시 해석하다가 명령이 꼬이는 문제를 줄인다.
    - planner가 최종 각도까지 추측하기보다 관절, 방향, 단계 수, 대기 시간만 구조화해 넘기게 한다.
  - 구현 방향:
    - planner 출력에 `joint`, `mode`, `value`, `wait` 같은 중간 표현을 둔다.
    - resolver는 이를 누적 절대각 시퀀스로 변환한다.
    - validator는 최종 절대각 명령만 검증한다.
  - 진행 메모:
    - `2026-04-14` 기준으로 `motion_resolver.py`에 `N초 뒤에 N도 더`와 `N도씩 두번` 텍스트를 직접 읽는 다단계 상대이동 파서를 먼저 추가했다.
    - 이번 단계에서는 planner prompt와 validator 규칙은 그대로 두고, resolver에서 누적 절대각 시퀀스만 다시 계산한다.

- [ ] `classifier routing / shortcut` 보강
  - 목표: 짧은 social-motion 발화가 `chat`으로 빠지지 않게 classifier routing을 먼저 안정화한다.
  - 우선 대상:
    - `만세`
    - `손 흔들어`
    - `고개 끄덕여`
    - `고개 저어봐`
  - 메모:
    - `scenario_08`은 failure taxonomy보다 classifier가 `만세`를 `chat`으로 본 문제가 더 직접적인 원인으로 보인다.
    - 저모호도 motion/social command는 prefilter 또는 shortcut으로 먼저 잡는 쪽이 낫다.

- [ ] `scenario eval` 확장
  - 목표: smoke(기본 확인)에서 벗어나 실제 운용에 가까운 복합 시나리오를 평가한다.
  - 범위:
    - 정상 명령
    - 애매한 명령
    - 위험 명령
    - 연속 대화 명령
  - 예시:
    - `안녕 하고 고개 끄덕여`
    - `위대한 쇼맨 틀고 손 흔들어`
    - `허리 100도 돌려`
    - `지금 연주 중이면 멈추고 홈으로 가`
    - `왼팔 조금만 더 올려`
    - `아까보다 살짝 오른쪽 봐`
    - `연주하다가 끝나면 인사해`

## Next

- [x] `planner latency isolation benchmark` 추가
  - 목표: classifier 영향 없이 planner 자체의 latency와 output variability를 따로 측정한다.
  - 조건:
    - 동일 `classifier_result`
    - 동일 `planner_input_json`
    - warm/cold 조건 분리 가능하면 기록
  - 기록 항목:
    - planner input JSON 길이
    - planner response 길이
    - avg / median / p95 latency
  - 메모:
    - `eval/run_planner_latency_isolation.py`를 추가해 JSON production planner path만 대상으로 같은 fixture 위에서 planner만 반복 측정하도록 구성했다.

- [ ] `state_adapter` 강화
  - raw state를 planner-friendly feature로 가공한다.
  - 후보 feature:
    - `currently_busy`
    - `can_accept_motion_command`
    - `can_accept_play_command`
    - `safety_lock`
    - `recoverable_error`
    - `recent_action_summary`

- [ ] `task planner + dialogue planner` 분리
  - 현재 domain planner가 동시에 다루는 `행동 계획`과 `말하기 계획`을 분리한다.
  - 목표 흐름:
    - `intent_classifier`
    - `task_planner`
    - `dialogue_planner`
    - `validator`
    - `executor`

- [ ] `approval / clarification checkpoint` 1급 시민화
  - 목표: 위험하거나 모호한 명령은 일반 실패가 아니라 명시적 승인/되묻기 단계로 승격한다.
  - 예:
    - `허리 돌려` -> `몇 도로 움직일까요?`
    - `종료해` -> 필요 시 확인 후 실행
  - 구현 방향:
    - planner 또는 validator가 `clarification` / `confirmation` 상태를 반환
    - 다음 턴에서 pending action을 이어서 처리

- [ ] `session memory` 초안
  - 목표: 바로 직전 문맥을 기억해 `거기서 더`, `아까처럼`, `그 노래` 같은 지시를 안정적으로 처리한다.
  - 범위:
    - 직전 intent
    - 직전 planner result
    - 직전 confirmed target
    - 직전 clarification state

- [x] `planner model benchmark` 설계
  - 목표: 여러 planner 모델을 같은 조건에서 비교해 planner 후보를 고른다.
  - 비교 조건:
    - 동일 `cases`
    - 동일 `classifier_result` 또는 동일 `planner_input_json`
    - 동일 validator / executor 규칙
  - 비교 항목:
    - planner pass rate
    - skill selection quality
    - valid command quality
    - speech quality
    - avg / median / p95 planner latency
  - 메모:
    - `eval/run_planner_benchmark.py`를 JSON-only fixed fixture 방식으로 바꿔, classifier 를 케이스당 한 번만 실행하고 모든 planner 모델을 같은 `planner_input_json` 위에서 비교하도록 정리했다.

- [ ] `classifier prefilter` 도입
  - 목표: 자주 나오는 저모호도 발화를 rule-based shortcut으로 먼저 처리해 classifier latency를 줄인다.
  - 대상 예:
    - 인사
    - 이름/정체 질문
    - 관절 각도 질문
    - 단순 상태 질의

- [ ] `clarification flow` 설계
  - 목표: `허리 돌려`, `팔 올려` 같은 모호한 요청에 대해 `몇 도로 움직일까요?`처럼 되묻는다.
  - 원칙:
    - 저위험 상대이동은 기본 step 허용 가능
    - 중/고위험 모호 명령은 clarification 우선

- [ ] `skill abstraction` 강화
  - 목표: LLM이 저수준 `move:<motor>,<angle>`를 직접 만드는 비율을 더 낮춘다.
  - 방향:
    - `greet_user`
    - `wave_hi`
    - `nod_yes`
    - `shake_no`
    - `celebrate`
    - `arm_up`
    - `arm_down`
    - `start_play_song(TIM)`
    - `explain_joint_limit`
    - `look_at_user`
    - `return_home`
  - 원칙:
    - 이미 `AgentAction` 기본 제어 함수로 안정적으로 수행 가능한 긴 시퀀스부터 먼저 위 계층으로 올린다

## Later

- [ ] `planner model search`
  - 조건: planner 구조를 먼저 안정화한 뒤 진행
  - 주의: classifier나 planner 구조가 흔들리는 동안의 모델 비교는 해석이 어렵다.

- [ ] `URDF 기반 SIL` 구축
  - 목표: 실제 로봇을 덜 건드리고 scenario test를 반복 가능한 환경에서 돌린다.
  - 기대 효과:
    - 연속 시나리오 테스트
    - 복구 정책 검증
    - 저수준 command 안전성 확인

- [ ] `recovery / resume flow` 설계
  - 목표: 중간 실패 후 안전 상태로 돌아가거나, 가능한 경우 작업을 재개한다.
  - 예:
    - joint limit 차단 후 대안 제시
    - 실행 중 실패 후 `home` 복귀
    - recoverable error 해소 후 pending task 재개

- [ ] `fallback planner` / `recovery planner` 분리 검토
  - 목표: 기본 planner 실패나 실행 실패 시 별도 계획기로 대응한다.
  - fallback planner:
    - planner JSON 실패
    - skill 선택 실패
    - domain 불확실
  - recovery planner:
    - 실행 실패 후 다음 안전 행동 결정
    - 사용자 설명 문구와 복구 행동 분리

- [ ] `perception` 결합 지점 설계
  - 목표: 향후 비전/센서 인식 결과를 planner-friendly feature로 넣을 수 있게 한다.
  - 예:
    - 사용자가 어느 방향에 있는지
    - 현재 타겟이 감지되는지
    - 시선/팔 동작의 실제 성공 여부

- [ ] `TAU-style task benchmark` 설계
  - 그대로 원본을 들여오기보다, 로봇 제어 태스크 중심으로 adaptation 한다.
  - 지표 후보:
    - task completion rate
    - safety violation rate
    - recovery success rate
    - multi-turn consistency

- [ ] `deterministic status shortcut` 확장
  - 현재 일부 관절 각도 질문 외에도 직접 답할 수 있는 상태 질의를 늘린다.
  - 후보:
    - 현재 곡
    - 현재 BPM
    - 직전 행동
    - 에러 상태 / 에러 원인

- [x] `LangGraph-style state graph` 도입 여부 검토
  - 목표: 장기적으로 pause/resume, confirmation, recovery, multi-step routing을 상태 그래프로 관리할지 판단한다.
  - 메모:
    - `2026-04-16` 기준으로 LangGraph 스타일 상태 기계 도입 완료.
    - Python 3.8 + aarch64 제약으로 langgraph 패키지 대신 `pipeline/state_graph.py` 경량 호환 구현 사용. API 동일.
    - `pipeline/robot_graph.py`에 `process→execute→return_home` 노드 구성.
    - `pipeline/exec_thread.py`에 `InterruptibleExecutor` 구현 (cancel 시 'stop' 명령 전송 + wait 즉시 중단).
    - 동작 완료 후 홈 복귀(`h`), Enter 누름 시 이전 동작 즉시 중단, TTS 동시 실행.
    - 상세 설계: `docs/LANGGRAPH_STATE_MACHINE_KR.md`

- [ ] `memory / RAG` 장기 구조 검토
  - 목표: 제어 path와 분리된 knowledge path를 설계한다.
  - memory:
    - session memory
    - long-term memory
  - RAG:
    - 매뉴얼/에러문서/레퍼토리 설명 retrieval
  - 원칙:
    - safety-critical control은 code/validator 중심 유지
    - RAG는 knowledge augmentation 용도로만 사용

- [ ] `multi-agent / supervisor` 필요성 재평가
  - 목표: 현재 단일 pipeline으로 충분한지, 장기적으로 supervisor가 필요한지 판단한다.
  - 후보 구조:
    - intent agent
    - task planner agent
    - dialogue agent
    - supervisor / coordinator
  - 비고:
    - 현재 시점에는 과도할 수 있으므로 실제 병목이 보일 때만 추진

- [ ] `legacy C++ control revival review`
  - 목표: 예전 C++ 제어 기능 중 재활용 가능한 범위와 폐기할 범위를 다시 나눈다.
  - 재검토 대상:
    - `DrumRobot.cpp`의 magenta mode
    - sync 맞추기 기능
    - `TestManager`
  - 판단 기준:
    - blocking behavior가 현재 Python/LLM 파이프라인과 충돌하는지
    - low-level primitive / timing logic으로 재사용 가능한지
    - legacy orchestration 자체는 버리고 test harness 또는 하위 제어기로만 남길지
  - 권장 시점:
    - scenario eval
    - recovery / resume flow
    - URDF 기반 SIL
    이후에 다시 검토

## Parking Lot

- [ ] `DrumRobot2 박자 단위 pause/resume` (파일 단위 재개의 다음 단계)
  - 현재: 파일 처음부터 재연주. 더 정밀한 재개를 원하면 버퍼에 파일 경계 마커를 심어야 함
  - 방향: `TMotorData`에 마커 필드 추가 → send 스레드가 마커 소비 시 임시 파일에 위치 기록 → pause 시 읽기
- [ ] `failure taxonomy` 정의
  - 메모:
    - 지금은 `raw_op_cmds / resolved_op_cmds / valid_op_cmds`만으로도 1차 원인 분리가 가능하다.
    - 당장은 taxonomy 확장보다 `planner 의미 해석 / resolver 계산 분리`와 `classifier routing` 안정화가 우선으로 보인다.
- [ ] `task planner`와 `dialogue planner`를 서로 다른 모델로 분리할지 검토
- [ ] TTS 숫자 읽기 정책 세분화
  - 각도
  - 시간
  - BPM
  - 퍼센트
- [ ] `executor`와 `command_executor`의 장기적 통합/분리 기준 정리
- [ ] import fallback 정리
  - 장기적으로 실행 방식을 하나로 통일할지 검토
- [ ] `ValidatedPlan` / `BrainTurnResult` 시각화용 디버그 출력 정리
- [ ] `unused import` / 구조 정리 리팩토링

## Done

- [x] `DrumRobot2 파일 단위 pause/resume` 구현
  - `runPlayProcess()` 파일별 처리 블록 안에 실행 완료 대기 루프 추가
  - pause 시 `play_file_index`가 현재 파일을 가리킴 → resume 시 해당 파일 처음부터 재연주
  - 박자 단위 재개는 Parking Lot 참조
- [x] `gesture / play abstraction uplift`
  - 목표: 긴 `op_cmd` 시퀀스를 위 계층 skill/gesture로 끌어올리고, `AgentAction`은 저수준 실행 primitive로 고정한다.
  - 1차 범위:
    - 기존 `wave / nod / shake / hurray` 계열을 상위 skill 카탈로그 기준으로 다시 정리
    - `arm_up`, `arm_down` 같은 팔 동작 gesture 추가
    - `tempo:fast|slow`, `strength:strong|soft`를 다음 연주용 pre-play modifier로 설계
  - 원칙:
    - planner는 긴 `move:` 나열보다 skill / symbolic command를 우선 사용
    - calibration과 실제 joint 시퀀스는 C++ `AgentAction` 쪽에 둔다
    - 연주 speed / intensity modifier는 `readMeasure()`에서 적용한다
  - 메모:
    - 현재 기준으로 baseline uplift와 pre-play modifier/readMeasure 적용 경로까지는 사실상 정리 완료로 본다.
    - 이후 확장 기준과 계층 재정리는 `phil_robot/docs/DECISION_LAYER_ROADMAP_KR.md`를 중심으로 따라간다.
- [x] 자유형 문자열 출력에서 `JSON-only` 출력 계약으로 전환
- [x] parser fallback 추가
- [x] classifier / planner 2단계 분리
- [x] domain-specific planner 도입
- [x] `ValidatedPlan` 기반 validator / executor 분리
- [x] `skill-first` planning 도입
- [x] `relative motion resolver` 추가
- [x] 일부 상태 질의에 대한 deterministic direct answer 도입
- [x] `smoke` multi-layer eval 구축
- [x] classifier benchmark 수행 및 `qwen3:4b-instruct-2507-q4_K_M` 채택
- [x] eval report 자동 파일명 규칙 도입
- [x] `phil_robot` 폴더 구조 정리

## Active Notes

- 현재 classifier:
  - `qwen3:4b-instruct-2507-q4_K_M`
- 현재 planner:
  - `qwen3:30b-a3b-instruct-2507-q4_K_M`
- 현재 기본 smoke suite:
  - `eval/cases_smoke.json`
- 최신 규칙 기반 자동 리포트 파일명:
  - `<suite>_report_<classifier약어>_<planner약어>_<YYYYMMDD_HHMM>.json`
