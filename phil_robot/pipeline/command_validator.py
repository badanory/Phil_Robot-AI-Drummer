from dataclasses import dataclass, field
from typing import List

JOINT_LIMITS = {
    #"waist": (-70.0, 70.0),
    #"R_arm1": (20.0, 120.0),
    #"L_arm1": (50.0, 150.0),
    #"R_arm2": (-40.0, 60.0),
    #"R_arm3": (30.0, 120.0),
    #"L_arm2": (-40.0, 60.0),
    #"L_arm3": (30.0, 120.0),
    #"R_wrist": (-10.0, 90.0),
    #"L_wrist": (15.0, 90.0),
    #"R_foot": (-90.0, 200.0),
    #"L_foot": (-90.0, 200.0),
    "waist": (-90.0, 90.0),
    "R_arm1": (0.0, 150.0),
    "L_arm1": (30.0, 180.0),
    "R_arm2": (-60.0, 90.0),
    "R_arm3": (0.0, 140.1),
    "L_arm2": (-60.0, 90.0),
    "L_arm3": (0.0, 140.1),
    "R_wrist": (-108.0, 90.0),
    "L_wrist": (-108.0, 90.0),
    "R_foot": (-90.0, 200.0),
    "L_foot": (-90.0, 200.0),
}

PLAY_CODES = {"TIM", "TY_short", "BI", "test_one"}
GESTURES = {"hi", "nod", "shake", "wave", "hurray", "happy"}
SIMPLE_COMMANDS = {"r", "h", "s", "t", "u"}
# 연주 중 게이트 우회 인터럽트 명령 — state/motion 조건 무관하게 항상 통과
INTERRUPT_COMMANDS = {"pause", "resume"}
MOTION_KEYWORDS = [
    "손",
    "팔",
    "허리",
    "손목",
    "발",
    "움직",
    "들어",
    "들어줘",
    "돌려",
    "봐",
    "고개",
    "인사",
    "흔들",
    "만세",
    "연주",
    "쳐",
]


@dataclass
class ValidationResult:
    valid_commands: List[str] = field(default_factory=list)
    rejected_commands: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)


def validate_commands(commands, robot_state):
    """
    LLM이 만든 명령 문자열을 실행 전에 검증한다.
    parser는 형식 해석, validator는 실행 가능 여부 판단에 집중한다.
    """
    result = ValidationResult()

    for command in commands:
        normalized = normalize_command(command)
        if not normalized:
            continue

        is_valid, reason = validate_command(normalized, robot_state)
        if is_valid:
            result.valid_commands.append(normalized)
        else:
            result.rejected_commands.append(normalized)
            result.warnings.append(reason)

    result.warnings.extend(validate_sequence_rules(result.valid_commands))
    return result


def normalize_command(command):
    """
    모델이 과거 포맷을 섞어 내보내도 실행 계층에서는 최대한 표준 명령으로 정규화한다.
    현재 가장 흔한 케이스는 move:motor,L_wrist,90 -> move:L_wrist,90 이다.
    """
    normalized = (command or "").strip()
    if normalized.startswith("move:motor,"):
        move_args = normalized[len("move:motor,"):]
        return f"move:{move_args}"
    return normalized


def validate_command(command, robot_state):
    if command in INTERRUPT_COMMANDS:
        return True, ""  # pause/resume은 C++ gate bypass 명령이므로 항상 통과
    if command in SIMPLE_COMMANDS:
        return validate_simple_command(command, robot_state)
    if command.startswith("p:"):
        return validate_play_command(command, robot_state)
    if command.startswith("look:"):
        return validate_look_command(command, robot_state)
    if command.startswith("gesture:"):
        return validate_gesture_command(command, robot_state)
    if command.startswith("led:"):
        return validate_led_command(command)
    if command.startswith("move:"):
        return validate_move_command(command, robot_state)
    if command.startswith("wait:"):
        return validate_wait_command(command)
    return False, f"알 수 없는 명령 형식 차단: {command}"


def validate_simple_command(command, robot_state):
    if command == "s":
        return True, ""
    return validate_motion_allowed(command, robot_state)


def validate_play_command(command, robot_state):
    _, _, song_code = command.partition(":")
    if song_code not in PLAY_CODES:
        return False, f"알 수 없는 곡 코드 차단: {command}"

    allowed, reason = validate_motion_allowed(command, robot_state)
    if not allowed:
        return False, reason

    if robot_state.get("state", 0) != 0:
        return False, f"현재 state={robot_state.get('state')} 에서는 연주 명령 차단: {command}"

    return True, ""


def validate_look_command(command, robot_state):
    allowed, reason = validate_motion_allowed(command, robot_state)
    if not allowed:
        return False, reason

    try:
        _, look_args = command.split(":", 1)
        pan_raw, tilt_raw = look_args.split(",", 1)
        pan = float(pan_raw)
        tilt = float(tilt_raw)
    except ValueError:
        return False, f"look 명령 파싱 실패: {command}"

    if not (-90.0 <= pan <= 90.0 and 0.0 <= tilt <= 120.0):
        return False, f"look 범위 초과 차단: {command}"

    return True, ""


def validate_gesture_command(command, robot_state):
    allowed, reason = validate_motion_allowed(command, robot_state)
    if not allowed:
        return False, reason

    _, _, gesture = command.partition(":")
    if gesture not in GESTURES:
        return False, f"gesture 값 차단: {command}"

    return True, ""


def validate_led_command(command):
    return False, f"LED 명령은 현재 사용하지 않음: {command}"


def validate_move_command(command, robot_state):
    allowed, reason = validate_motion_allowed(command, robot_state)
    if not allowed:
        return False, reason

    try:
        _, move_args = command.split(":", 1)
        motor_name, angle_raw = move_args.split(",", 1)
        angle = float(angle_raw)
    except ValueError:
        return False, f"move 명령 파싱 실패: {command}"

    if motor_name not in JOINT_LIMITS:
        return False, f"알 수 없는 모터 차단: {command}"

    min_angle, max_angle = JOINT_LIMITS[motor_name]
    if not (min_angle <= angle <= max_angle):
        return False, f"관절 한계 초과 차단: {command}"

    return True, ""


def validate_wait_command(command):
    try:
        _, _, seconds_raw = command.partition(":")
        seconds = float(seconds_raw)
    except ValueError:
        return False, f"wait 명령 파싱 실패: {command}"

    if seconds < 0.0 or seconds > 10.0:
        return False, f"wait 범위 초과 차단: {command}"

    return True, ""


def validate_motion_allowed(command, robot_state):
    if not robot_state.get("is_lock_key_removed", False):
        return False, f"안전 키 미해제로 동작 명령 차단: {command}"

    current_state = robot_state.get("state", 0)
    if current_state == 2:
        return False, f"연주 중이므로 동작 명령 차단: {command}"
    if current_state == 6:
        return False, f"에러 상태이므로 동작 명령 차단: {command}"

    if not robot_state.get("is_fixed", True):
        return False, f"이동 중이므로 동작 명령 차단: {command}"

    return True, ""


def validate_sequence_rules(commands):
    """
    현재는 강제 차단보다 경고 위주로 둔다.
    planner가 도입되면 이 계층에서 더 적극적으로 시퀀스를 보정할 수 있다.
    """
    warnings = []

    play_commands = [cmd for cmd in commands if cmd.startswith("p:")]
    if play_commands and "r" not in commands:
        warnings.append("play 명령이 준비 자세 없이 생성되었습니다. 현재는 로봇 측 기존 동작에 의존합니다.")

    consecutive_moves = 0
    for command in commands:
        if command.startswith("move:"):
            consecutive_moves += 1
        elif command.startswith("wait:"):
            consecutive_moves = 0
        else:
            consecutive_moves = 0

        if consecutive_moves >= 3:
            warnings.append("move 명령이 연속으로 길게 이어집니다. 후속 planner 단계에서 wait 삽입을 고려하세요.")
            break

    return warnings


def user_text_requests_motion(user_text):
    """아주 가벼운 규칙 기반 감지기로 물리 동작 요청을 추정한다."""
    normalized_text = (user_text or "").strip()
    return any(keyword in normalized_text for keyword in MOTION_KEYWORDS)


def has_actionable_motion_command(commands):
    """
    실제로 로봇 자세/행동을 바꾸는 명령이 하나라도 남아있는지 본다.
    wait 는 보조 명령이므로 단독으로는 동작 성공으로 보지 않는다.
    """
    for command in commands:
        if command.startswith(("move:", "gesture:", "look:", "p:")):
            return True
        if command in {"r", "h"}:
            return True
    return False
