# Phil Robot 최신 함수 명세서 (Function Specs)

> 이 문서는 파편화된 코드베이스를 하나의 맥락으로 이해하기 위해, 현재(최신) 아키텍처 기준으로 각 파일과 핵심 함수가 정확히 무슨 일을 하는지 매핑한 문서입니다.

---

## 1. 최상위 오케스트레이션 (Orchestration)

### `phil_brain.py`
가장 바깥쪽에서 마이크를 열고, LLM을 호출하고, 로봇에게 명령을 내리는 무한 루프를 돌리는 메인 파일입니다.

- **`main()`**
  - 프로그램의 시작점입니다.
  - `InterruptibleExecutor`(백그라운드 실행기)와 `MeloTTS`(음성 합성기)를 초기화합니다.
  - `build_phil_graph()`를 호출해 상태 기계(LangGraph)를 만듭니다.
  - 무한 루프를 돌며 사용자의 음성을 듣고(STT), 상태 기계를 실행(`app.invoke(state)`)합니다.

---

## 2. 상태 기계 및 실행 제어 (State Machine & Executor)

### `pipeline/robot_graph.py`
단일 턴의 흐름(`process` ➔ `execute` ➔ `return_home`)을 정의합니다.

- **`build_phil_graph(...)`**
  - LangGraph 뼈대 위에 세 개의 노드(생각, 실행, 마무리)를 조립하고 컴파일해 반환합니다.
- **`make_process_node(...)`**
  - **역할**: "생각하기" 노드를 만듭니다.
  - 로봇이 이전 동작을 수행 중이라면 즉시 `executor.cancel()`을 호출해 중단시킵니다.
  - `run_brain_turn()`을 호출해 LLM 파이프라인(Classifier ➔ Planner)을 가동하고 그 결과를 바구니(`PhilState`)에 담습니다.
- **`make_execute_node(...)`**
  - **역할**: "행동하기" 노드를 만듭니다.
  - 바구니에 담긴 명령어 리스트(`commands`)를 `InterruptibleExecutor.run()`에 넘겨 백그라운드에서 실행시킵니다.
  - 모션인 경우(`plan_type == "motion"`), 멈췄을 때 홈으로 돌아가라는 콜백(`on_done`)을 예약합니다.
- **`_wait_for_fixed_then_home(bot, get_state_fn)`**
  - **역할**: (백그라운드에서) 로봇이 움직이는지(`is_fixed=False`), 멈췄는지(`is_fixed=True`)를 0.05초 단위로 감시하다가, 멈췄을 때만 `h`(홈 복귀) 명령을 쏩니다.

### `pipeline/exec_thread.py`
사용자 인터럽트(끼어들기)를 처리하기 위해 로봇의 행동을 백그라운드로 밀어넣는 일꾼입니다.

- **`InterruptibleExecutor.run(commands, on_done)`**
  - 명령어들을 받아 별도의 스레드를 띄워 하나씩 전송합니다.
- **`InterruptibleExecutor.cancel()`**
  - 하던 일을 당장 멈추라는 신호(`_stop.set()`)를 보내고, 로봇에게 긴급 정지(`s`) 명령을 발사합니다.
- **`InterruptibleExecutor._interruptible_wait(cmd)`**
  - `wait:2` 같은 대기 명령을 수행할 때 `time.sleep(2)`로 통째로 멈추지 않고, 0.05초씩 쪼개어 자면서 언제든 `cancel`이 들어오면 깰 수 있도록 합니다.

---

## 3. LLM 두뇌 파이프라인 (Brain Pipeline)

### `pipeline/brain_pipeline.py`
"이 누더기(?)의 핵심 원흉이자 가장 중요한 파일"입니다. LLM 호출(분류 ➔ 계획)과 각종 지름길(Shortcut, 하드코딩된 예외 처리)이 섞여 있습니다.

- **`run_brain_turn(...)`**
  - 1턴의 LLM 처리를 통괄합니다. (Classifier ➔ Planner ➔ Validator)
  - **주의(누더기 포인트)**: LLM을 부르기 전에 연주 중 일시정지, "안녕" 인사 처리, 관절 각도 질문 등 뻔한 질문은 LLM을 안 거치고 여기서 직접 정답을 만들어치우는 땜질(Shortcut) 로직이 잔뜩 들어있습니다.
- **`_detect_play_interrupt(user_text, adapted_state)`**
  - 연주 중일 때 사용자가 "멈춰"라고 했는지 LLM 없이 빠르게 문자열 매칭으로만 확인합니다. 지연 시간을 없애기 위한 편법입니다.

### `pipeline/intent_classifier.py`
- **`build_classifier_input_json()`**: 상태와 사용자 말을 가벼운 JSON으로 포장합니다.
- **`parse_intent_response()`**: LLM이 뱉은 답변에서 `intent`(의도)를 파싱합니다.
- **`normalize_intent_result()`**: LLM이 헷갈린 의도를 안전하게 보정해 줍니다.

### `pipeline/planner.py`
- **`select_planner_domain(classifier_result)`**: 의도에 따라 어떤 프롬프트(chat, play, motion 등)를 쓸지 결정합니다.
- **`parse_plan_response()`**: LLM이 작성한 행동 계획(명령어, 대사)을 JSON에서 추출합니다.

---

## 4. 검열 및 최종 승인 (Validator & Resolver)

### `pipeline/validator.py`
LLM이 만든 계획을 로봇이 이해할 수 있는 최종 형태로 번역하고 위험한 명령을 걸러냅니다.

- **`build_validated_plan(...)`**
  - LLM의 원시 계획(`planner_result`)을 받아서 최종 검열 통과증(`ValidatedPlan`)을 발급합니다.
  - 이 안에서 `skills.py`를 불러 "안녕 인사하기" 같은 추상적 스킬을 `[gesture:wave, wait:2]`처럼 풀어냅니다.
  - 이 안에서 `motion_resolver.py`를 불러 "조금만 더 올려" 같은 상대적 요청을 "50도 위치로 이동" 같은 절대 각도로 계산합니다.
  - 마지막으로 `command_validator.py`를 통해 존재하지 않는 모터나 위험한 각도로 이동하는 걸 막아냅니다.
