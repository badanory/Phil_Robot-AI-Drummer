import json
import re
from typing import Dict


FALLBACK_MESSAGE = "죄송해요. 응답을 정리하다가 잠시 헷갈렸어요. 다시 말씀해 주세요."

DEFAULT_CLASSIFIER_FAILURE = {
    "intent": "unknown",
    "needs_motion": False,
    "needs_dialogue": True,
    "risk_level": "medium",
}

DEFAULT_PLANNER_FAILURE = {
    "skills": [],
    "op_cmd": [],
    "speech": FALLBACK_MESSAGE,
    "reason": "",
}


def sanitize_message(message) -> str:
    if not isinstance(message, str):
        return FALLBACK_MESSAGE

    clean_msg = re.sub(r"\([^)]*\)", "", message)
    sanitized = re.sub(r"\s+", " ", clean_msg).strip()
    return sanitized or FALLBACK_MESSAGE


def build_classifier_failure_result() -> Dict:
    return dict(DEFAULT_CLASSIFIER_FAILURE)


def build_planner_failure_result(reason: str = "") -> Dict:
    result = dict(DEFAULT_PLANNER_FAILURE)
    if isinstance(reason, str):
        result["reason"] = reason.strip()
    return result


def build_llm_call_failure_json(reason: str) -> str:
    detail = reason if isinstance(reason, str) else str(reason)
    return json.dumps(
        {
            "intent": DEFAULT_CLASSIFIER_FAILURE["intent"],
            "needs_motion": DEFAULT_CLASSIFIER_FAILURE["needs_motion"],
            "needs_dialogue": DEFAULT_CLASSIFIER_FAILURE["needs_dialogue"],
            "risk_level": DEFAULT_CLASSIFIER_FAILURE["risk_level"],
            "skills": [],
            "op_cmd": [],
            "commands": [],
            "speech": FALLBACK_MESSAGE,
            "message": FALLBACK_MESSAGE,
            "reason": detail,
            "thinking": detail,
        },
        ensure_ascii=False,
    )


def build_motion_block_message(robot_state) -> str:
    if not robot_state.get("is_lock_key_removed", False):
        return "아직 안전 키가 해제되지 않아 움직일 수 없습니다. 안전 키를 먼저 확인해 주세요."

    current_state = robot_state.get("state", 0)
    if current_state == 2:
        return "지금은 연주 중이라 다른 동작을 할 수 없습니다."
    if current_state == 4:
        return "지금은 일시정지 상태라 다른 동작을 할 수 없습니다. '다시 시작'이라고 말씀하시면 연주를 재개합니다."
    if current_state == 6:
        error_detail = robot_state.get("error_detail", "원인을 아직 확인 중입니다.")
        return f"지금은 에러 상태라 동작할 수 없습니다. 원인은 {error_detail} 입니다."

    if not robot_state.get("is_fixed", True):
        return "지금은 자세를 이동 중이라 다른 동작을 할 수 없습니다."

    return "지금은 해당 동작을 수행할 수 없습니다."
