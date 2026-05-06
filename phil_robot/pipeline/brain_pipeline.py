import time
from dataclasses import dataclass, field
from typing import Dict, List

try:
    # 패키지 문맥으로 import될 때 사용한다.
    # 예: from phil_robot.pipeline.brain_pipeline import run_brain_turn
    from .intent_classifier import (
        CLASSIFIER_SYSTEM_PROMPT,           # classifier system prompt
        build_classifier_input_json,        # classifier 입력 JSON 생성
        is_ambiguous_follow_up,             # history 없이 해석 어려운 초단문 follow-up 감지
        normalize_intent_result,            # classifier 결과 후처리/정규화
        parse_intent_response,              # classifier JSON 응답 파싱
    )
    
    # JSON 형식 LLM 호출 공통 래퍼
    from .llm_interface import call_json_llm
    from .planner import (
        build_planner_input_json,           # planner 입력 JSON 생성
        enforce_intent_constraints,         # planner 결과를 intent/domain 기준으로 한 번 더 정리
        get_planner_system_prompt,          # domain별 planner system prompt 생성
        parse_plan_response,                # planner JSON 응답 파싱
        select_planner_domain,              # classifier intent -> planner domain 변환
    )
    
    # raw state 정리 + 관절 각도 질문 shortcut 처리
    from .state_adapter import (
        adapt_robot_state,
        detect_identity_confirmation_query,
        build_joint_angle_answer,
        build_repertoire_answer,
        detect_joint_angle_query,
        detect_repertoire_query,
        detect_wave_play_request,
    )

    # planner 결과를 최종 실행 가능 계획으로 검증/정리
    from .validator import ValidatedPlan, build_validated_plan

    # 세션 단기 기억
    from .session import SessionContext, build_session_summary, resolve_clarification_text

except (ImportError, ValueError):
    # phil_robot 폴더 안에서 직접 실행할 때 사용하는 fallback import다.
    # 예: cd phil_robot && python phil_brain.py
    from pipeline.intent_classifier import (
        CLASSIFIER_SYSTEM_PROMPT,           # classifier system prompt
        build_classifier_input_json,        # classifier 입력 JSON 생성
        is_ambiguous_follow_up,             # history 없이 해석 어려운 초단문 follow-up 감지
        normalize_intent_result,            # classifier 결과 후처리/정규화
        parse_intent_response,              # classifier JSON 응답 파싱
    )
    
    # JSON 형식 LLM 호출 공통 래퍼
    from pipeline.llm_interface import call_json_llm
    from pipeline.planner import (
        build_planner_input_json,           # planner 입력 JSON 생성
        enforce_intent_constraints,         # planner 결과를 intent/domain 기준으로 한 번 더 정리
        get_planner_system_prompt,          # domain별 planner system prompt 생성
        parse_plan_response,                # planner JSON 응답 파싱
        select_planner_domain,              # classifier intent -> planner domain 변환
    )
    
    # raw state 정리 + 관절 각도 질문 shortcut 처리
    from pipeline.state_adapter import (
        adapt_robot_state,
        detect_identity_confirmation_query,
        build_joint_angle_answer,
        build_repertoire_answer,
        detect_joint_angle_query,
        detect_repertoire_query,
        detect_wave_play_request,
    )

    # planner 결과를 최종 실행 가능 계획으로 검증/정리
    from pipeline.validator import ValidatedPlan, build_validated_plan

    # 세션 단기 기억
    from pipeline.session import SessionContext, build_session_summary, resolve_clarification_text

try:
    # 패키지 문맥 import일 때 공용 모델 설정을 가져온다.
    from ..config import CLASSIFIER_MODEL, PLANNER_MODEL

except (ImportError, ValueError):
    # 직접 실행 시에는 phil_robot 루트의 config 모듈에서 가져온다.
    from config import CLASSIFIER_MODEL, PLANNER_MODEL


@dataclass
class BrainTurnResult:
    classifier_input_json: str
    classifier_result: Dict
    planner_domain: str
    planner_input_json: str
    classifier_raw_response_text: str
    planner_raw_response_text: str
    planner_result: Dict
    adapted_state: Dict
    validated_plan: ValidatedPlan = field(default_factory=ValidatedPlan)
    classifier_duration_sec: float = 0.0
    planner_duration_sec: float = 0.0
    llm_duration_sec: float = 0.0
    classifier_metrics: Dict = field(default_factory=dict)
    planner_metrics: Dict = field(default_factory=dict)


def _is_greeting_wave(user_text: str) -> bool:
    """'안녕'과 '반가워'가 동시에 포함된 인사 발화를 감지한다."""
    return "안녕" in user_text and "반가워" in user_text


_STOP_KEYWORDS = {"정지", "스톱", "잠깐", "그만", "멈춰", "멈춰봐"}
_PAUSE_KEYWORDS = {"일시정지", "pause"}
_RESUME_KEYWORDS = {"다시", "계속", "이어서", "재개", "resume"}


def _detect_play_interrupt(user_text: str, adapted_state: dict):
    """
    연주 중(state==2) pause, 또는 일시정지 중(state==4) resume 발화를 감지한다.
    LLM 없이 키워드 매칭으로 직접 처리해 지연을 줄인다.

    반환값: "stop_sequence" | "pause" | "resume" | None
    """
    current_state = adapted_state.get("state", 0)
    text = user_text.strip()
    
    if current_state == 2:
        if any(kw in text for kw in _STOP_KEYWORDS):
            return "stop_sequence"
        if any(kw in text for kw in _PAUSE_KEYWORDS):
            return "pause"
    if current_state == 4 and any(kw in text for kw in _RESUME_KEYWORDS):
        return "resume"
        
    return None


def run_brain_turn(
    user_text,
    raw_robot_state,
    classifier_model_name=CLASSIFIER_MODEL,
    planner_model_name=PLANNER_MODEL,
    capture_metrics: bool = False,
    session=None,
):
    """
    한 턴의 LLM 처리 파이프라인.
    1차 classifier 와 2차 planner 를 분리해, 각 역할을 독립적으로 다룰 수 있게 한다.

    session: SessionContext | None
        세션 단기 기억. None 이면 기존 stateless 동작과 동일하다.
        - clarification 대기 상태라면, user_text 와 이전 발화를 합쳐서 처리한다.
        - planner input 에 최근 대화 히스토리와 마지막 동작 상태를 포함한다.
    """
    adapted_state = adapt_robot_state(raw_robot_state)

    # ===========================================================
    # [이동 필요] 이 shortcut은 phil_brain.py 의 orchestration 진입점으로 옮겨야 한다.
    #
    # brain_pipeline.py 의 책임은 LLM 두 단계(classifier → planner) 조율이다.
    # 아래 블록은 classifier 호출 자체를 건너뛰는 순수 문자열 매칭 prefilter로,
    # LLM 파이프라인과 성격이 다르다.
    #
    # 올바른 위치: phil_brain.py 에서 run_brain_turn() 호출 전에 체크.
    # 장기적으로는 TODO.md 의 'classifier prefilter' 항목에 따라
    # pipeline/prefilter.py 로 분리하는 것이 맞다.
    # ===========================================================
    # Shortcut path:
    # 연주 중 pause/resume 발화는 LLM latency 없이 즉시 처리한다.
    # C++ gate 는 이미 pause/resume 을 interrupt 명령으로 허용하므로
    # 여기서 빠르게 내보내는 것이 사용자 체감에 직접적 영향을 준다.
    play_interrupt = _detect_play_interrupt(user_text, adapted_state)
    if play_interrupt is not None:
        speech_map = {"pause": "잠깐 멈출게요.", "resume": "다시 연주할게요.", "stop_sequence": "연주를 부드럽게 정지할게요."}
        
        if play_interrupt == "stop_sequence":
            op_cmds = [
                "tempo_scale:0.8",
                "velocity_delta:-2",
                "wait:1.0",
                "tempo_scale:0.6",
                "velocity_delta:-5",
                "wait:1.0",
                "tempo_scale:0.4",
                "velocity_delta:-8",
                "wait:1.0",
                "tempo_scale:1.0",
                "velocity_delta:0",
                "stop"
            ]
        else:
            op_cmds = [play_interrupt]

        classifier_result = {
            "intent": "stop_request",
            "needs_motion": False,
            "needs_dialogue": True,
            "risk_level": "low",
        }
        planner_result = {
            "skills": [],
            "op_cmd": op_cmds,
            "speech": speech_map[play_interrupt],
            "reason": f"연주 중 {play_interrupt} 키워드 감지 → 직접 처리",
        }
        validated_plan = build_validated_plan(
            user_text=user_text,
            robot_state=adapted_state,
            classifier_result=classifier_result,
            planner_result=planner_result,
        )
        return BrainTurnResult(
            classifier_input_json="",
            classifier_result=classifier_result,
            planner_domain="stop",
            planner_input_json="",
            classifier_raw_response_text="",
            planner_raw_response_text="",
            planner_result=planner_result,
            adapted_state=adapted_state,
            validated_plan=validated_plan,
        )
    # ===========================================================

    if _is_greeting_wave(user_text):
        classifier_result = {
            "intent": "motion_request",
            "needs_motion": True,
            "needs_dialogue": True,
            "risk_level": "low",
        }
        planner_result = {
            "skills": ["wave_hi"],
            "op_cmd": [],
            "speech": "안녕하세요! 반가워요.",
            "reason": "'안녕'과 '반가워'가 동시에 포함된 인사는 손 흔들기로 직접 처리",
        }
        validated_plan = build_validated_plan(
            user_text=user_text,
            robot_state=adapted_state,
            classifier_result=classifier_result,
            planner_result=planner_result,
        )
        return BrainTurnResult(
            classifier_input_json="",
            classifier_result=classifier_result,
            planner_domain="motion",
            planner_input_json="",
            classifier_raw_response_text="",
            planner_raw_response_text="",
            planner_result=planner_result,
            adapted_state=adapted_state,
            validated_plan=validated_plan,
        )
    # ===========================================================

    # ── clarification 처리 ────────────────────────────────────────────────
    # 이전 턴에서 필이 되물었다면, 원래 발화 + 이번 답변을 합쳐 classifier/planner 에 넘긴다.
    # 예) "허리 돌려" + "30도" → "허리 돌려 30도"
    if session is not None:
        user_text = resolve_clarification_text(session, user_text)

    classifier_input_json = build_classifier_input_json(adapted_state, user_text)
    classifier_start_time = time.time()
    if capture_metrics:
        classifier_raw_response_text, classifier_metrics = call_json_llm(
            model_name=classifier_model_name,
            system_prompt=CLASSIFIER_SYSTEM_PROMPT,
            user_input_json=classifier_input_json,
            capture_metrics=True,
        )
    else:
        classifier_raw_response_text = call_json_llm(
            model_name=classifier_model_name,
            system_prompt=CLASSIFIER_SYSTEM_PROMPT,
            user_input_json=classifier_input_json,
        )
        classifier_metrics = {}
    classifier_duration_sec = time.time() - classifier_start_time
    classifier_result = parse_intent_response(classifier_raw_response_text)
    classifier_result = normalize_intent_result(classifier_result, user_text)
    if detect_repertoire_query(user_text):
        classifier_result["intent"] = "chat"
        classifier_result["needs_motion"] = False
        classifier_result["needs_dialogue"] = True
        classifier_result["risk_level"] = "low"
    wave_play_request = detect_wave_play_request(user_text)
    if wave_play_request is not None:
        classifier_result["intent"] = "play_request"
        classifier_result["needs_motion"] = True
        classifier_result["needs_dialogue"] = True
        classifier_result["risk_level"] = "medium"

    planner_domain = select_planner_domain(classifier_result)
    joint_angle_query = detect_joint_angle_query(user_text)
    identity_query = detect_identity_confirmation_query(user_text)

    # Shortcut path:
    # history 가 없는 상태에서 "왜?" 같은 초단문을 planner 로 넘기면
    # 실제 근거 없는 이유를 만들어낼 수 있어 clarification 으로 바로 응답한다.
    if classifier_result.get("intent") == "unknown" and is_ambiguous_follow_up(user_text):
        planner_result = {
            "skills": [],
            "op_cmd": [],
            "speech": "무엇이 왜 그런지 조금만 더 구체적으로 말씀해 주세요.",
            "reason": "맥락 없는 초단문 후속 질문은 clarification 으로 직접 응답",
        }
        validated_plan = build_validated_plan(
            user_text=user_text,
            robot_state=adapted_state,
            classifier_result=classifier_result,
            planner_result=planner_result,
        )
        return BrainTurnResult(
            classifier_input_json=classifier_input_json,
            classifier_result=classifier_result,
            planner_domain=planner_domain,
            planner_input_json="",
            classifier_raw_response_text=classifier_raw_response_text,
            planner_raw_response_text="",
            planner_result=planner_result,
            adapted_state=adapted_state,
            validated_plan=validated_plan,
            classifier_duration_sec=classifier_duration_sec,
            planner_duration_sec=0.0,
            llm_duration_sec=classifier_duration_sec,
            classifier_metrics=classifier_metrics,
            planner_metrics={},
        )

    # Shortcut path:
    # 지원 곡 목록 질문은 실행 가능 명령이 필요 없고,
    # 안전 키 상태와 무관하게 고정된 repertoire 답변을 주는 편이 더 안정적이다.
    if detect_repertoire_query(user_text):
        planner_result = {
            "skills": [],
            "op_cmd": [],
            "speech": build_repertoire_answer(),
            "reason": "지원 곡 목록 질문은 고정 repertoire 응답으로 직접 처리",
        }
        validated_plan = build_validated_plan(
            user_text=user_text,
            robot_state=adapted_state,
            classifier_result=classifier_result,
            planner_result=planner_result,
        )
        return BrainTurnResult(
            classifier_input_json=classifier_input_json,
            classifier_result=classifier_result,
            planner_domain=planner_domain,
            planner_input_json="",
            classifier_raw_response_text=classifier_raw_response_text,
            planner_raw_response_text="",
            planner_result=planner_result,
            adapted_state=adapted_state,
            validated_plan=validated_plan,
            classifier_duration_sec=classifier_duration_sec,
            planner_duration_sec=0.0,
            llm_duration_sec=classifier_duration_sec,
            classifier_metrics=classifier_metrics,
            planner_metrics={},
        )

    # Shortcut path:
    # 이름 확인형 질문은 필의 정체성이 고정돼 있으므로,
    # yes/no 몸짓과 발화를 deterministic 하게 고정하는 편이 더 안정적이다.
    if identity_query is not None and classifier_result.get("intent") == "motion_request":
        if identity_query["is_robot_name"]:
            planner_result = {
                "skills": ["nod_yes"],
                "op_cmd": [],
                "speech": "네, 제 이름은 필이에요.",
                "reason": "이름 확인 질문은 필의 정체성을 기준으로 직접 응답",
            }
        else:
            planner_result = {
                "skills": ["shake_no"],
                "op_cmd": [],
                "speech": "아니요, 제 이름은 필이에요.",
                "reason": "이름 확인 질문은 필의 정체성을 기준으로 직접 응답",
            }
        validated_plan = build_validated_plan(
            user_text=user_text,
            robot_state=adapted_state,
            classifier_result=classifier_result,
            planner_result=planner_result,
        )
        return BrainTurnResult(
            classifier_input_json=classifier_input_json,
            classifier_result=classifier_result,
            planner_domain=planner_domain,
            planner_input_json="",
            classifier_raw_response_text=classifier_raw_response_text,
            planner_raw_response_text="",
            planner_result=planner_result,
            adapted_state=adapted_state,
            validated_plan=validated_plan,
            classifier_duration_sec=classifier_duration_sec,
            planner_duration_sec=0.0,
            llm_duration_sec=classifier_duration_sec,
            classifier_metrics=classifier_metrics,
            planner_metrics={},
        )

    # Shortcut path:
    # 손 인사 후 특정 곡 재생 요청은 현재 skill 조합이 정해져 있으므로,
    # planner 자유생성 대신 고정 시퀀스로 처리해 wave/play 동시 성공률을 높인다.
    if wave_play_request is not None:
        planner_result = {
            "skills": ["wave_hi", wave_play_request["play_skill"]],
            "op_cmd": [],
            "speech": f"손을 흔들며 인사하고, {wave_play_request['song_label']}를 연주할게요.",
            "reason": "손 인사 후 곡 재생 복합 요청은 고정 skill 시퀀스로 직접 처리",
        }
        validated_plan = build_validated_plan(
            user_text=user_text,
            robot_state=adapted_state,
            classifier_result=classifier_result,
            planner_result=planner_result,
        )
        return BrainTurnResult(
            classifier_input_json=classifier_input_json,
            classifier_result=classifier_result,
            planner_domain=planner_domain,
            planner_input_json="",
            classifier_raw_response_text=classifier_raw_response_text,
            planner_raw_response_text="",
            planner_result=planner_result,
            adapted_state=adapted_state,
            validated_plan=validated_plan,
            classifier_duration_sec=classifier_duration_sec,
            planner_duration_sec=0.0,
            llm_duration_sec=classifier_duration_sec,
            classifier_metrics=classifier_metrics,
            planner_metrics={},
        )

    # Shortcut path:
    # 특정 관절의 현재 각도를 묻는 상태 질의는 planner 를 거치지 않고
    # 현재 상태 스냅샷에서 직접 답해 더 빠르고 안정적으로 처리한다.
    if classifier_result.get("intent") == "status_question" and joint_angle_query:
        deterministic_speech = build_joint_angle_answer(adapted_state, joint_angle_query)
        planner_result = {
            "skills": [],
            "op_cmd": [],
            "speech": deterministic_speech,
            "reason": "현재 관절 각도 조회는 상태 스냅샷에서 직접 응답",
        }
        validated_plan = build_validated_plan(
            user_text=user_text,
            robot_state=adapted_state,
            classifier_result=classifier_result,
            planner_result=planner_result,
        )
        return BrainTurnResult(
            classifier_input_json=classifier_input_json,
            classifier_result=classifier_result,
            planner_domain=planner_domain,
            planner_input_json="",
            classifier_raw_response_text=classifier_raw_response_text,
            planner_raw_response_text="",
            planner_result=planner_result,
            adapted_state=adapted_state,
            validated_plan=validated_plan,
            classifier_duration_sec=classifier_duration_sec,
            planner_duration_sec=0.0,
            llm_duration_sec=classifier_duration_sec,
            classifier_metrics=classifier_metrics,
            planner_metrics={},
        )

    # Planner path:
    # 일반 대화/행동/연주 요청은 planner LLM 에게 위임해
    # skill/command/speech plan 을 만들고 validator 로 넘긴다.
    planner_system_prompt = get_planner_system_prompt(planner_domain)

    # session 이 있으면 최근 대화 히스토리와 마지막 동작 상태를 planner input 에 포함한다.
    session_summary = build_session_summary(session) if session is not None else None
    planner_input_json = build_planner_input_json(
        adapted_state, user_text, classifier_result, planner_domain, session_summary
    )
    planner_start_time = time.time()
    if capture_metrics:
        planner_raw_response_text, planner_metrics = call_json_llm(
            model_name=planner_model_name,
            system_prompt=planner_system_prompt,
            user_input_json=planner_input_json,
            capture_metrics=True,
        )
    else:
        planner_raw_response_text = call_json_llm(
            model_name=planner_model_name,
            system_prompt=planner_system_prompt,
            user_input_json=planner_input_json,
        )
        planner_metrics = {}
    planner_duration_sec = time.time() - planner_start_time

    planner_result = parse_plan_response(planner_raw_response_text)
    planner_result = enforce_intent_constraints(planner_result, classifier_result)
    validated_plan = build_validated_plan(
        user_text=user_text,
        robot_state=adapted_state,
        classifier_result=classifier_result,
        planner_result=planner_result,
    )

    return BrainTurnResult(
        classifier_input_json=classifier_input_json,
        classifier_result=classifier_result,
        planner_domain=planner_domain,
        planner_input_json=planner_input_json,
        classifier_raw_response_text=classifier_raw_response_text,
        planner_raw_response_text=planner_raw_response_text,
        planner_result=planner_result,
        adapted_state=adapted_state,
        validated_plan=validated_plan,
        classifier_duration_sec=classifier_duration_sec,
        planner_duration_sec=planner_duration_sec,
        llm_duration_sec=classifier_duration_sec + planner_duration_sec,
        classifier_metrics=classifier_metrics,
        planner_metrics=planner_metrics,
    )
