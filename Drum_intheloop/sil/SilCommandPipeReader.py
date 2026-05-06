import argparse
import json
import logging
import os
import stat
import sys
import time
from pathlib import Path
from typing import Dict, Iterator, Optional

IDLE_RETURN_SEC = 5.0  # 명령 없이 이 시간이 지나면 SIL에서 startup pose로 복귀

# Ensure the project root is in the Python path for imports
PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from sil.command_applier import CommandApplier
from sil.command_types import MaxonData, TMotorData
from sil.pybullet_backend import PyBulletBackend
from sil.robot_spec import STARTUP_CAN_POSE_DEG, STARTUP_DXL_POSE_DEG
from sil.vcan_state_writer import VcanStateWriter

logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")

# Writer 경로와 동일
DEFAULT_PIPE_PATH = Path("/tmp/drum_command.pipe")

# URDF 경로는 PyBulletBackend의 기본 URDF와 동일하게 설정
DEFAULT_URDF_PATH = (
    PROJECT_ROOT
    / "urdf"
    / "drumrobot_RL_urdf"
    / "urdf"
    / "drumrobot_RL_urdf.urdf"
)

# SilCommandPipeReader 클래스는 named pipe에서 명령 메시지를 읽고 파싱하는 역할을 담당
class SilCommandPipeReader:
    def __init__(self, pipe_path: Path = DEFAULT_PIPE_PATH):
        self.pipe_path = Path(pipe_path)
        self._owns_pipe = False

    # stale FIFO를 지우고 새 named pipe를 준비
    def prepare_pipe(self) -> None:
        if self.pipe_path.exists():
            path_stat = self.pipe_path.stat()
            if not stat.S_ISFIFO(path_stat.st_mode):
                raise RuntimeError(f"Pipe path exists but is not a FIFO: {self.pipe_path}")
            self.pipe_path.unlink()

        os.mkfifo(self.pipe_path)
        self._owns_pipe = True

    def cleanup_pipe(self) -> None:
        if not self._owns_pipe:
            return

        if self.pipe_path.exists():
            path_stat = self.pipe_path.stat()
            if stat.S_ISFIFO(path_stat.st_mode):
                self.pipe_path.unlink()

        self._owns_pipe = False

    # named pipe에서 메시지를 읽고 파싱된 명령을 생성하는 제너레이터
    def read_messages(self) -> Iterator[dict]:
        with self.pipe_path.open("r", encoding="utf-8") as pipe:
            for raw_line in pipe:
                line = raw_line.strip()
                if not line:
                    continue

                yield self.parse_line(line)

    # 메시지를 JSON파싱하고 kind에 따라 dict 구조로 변환
    def parse_line(self, line: str) -> dict:
        payload = json.loads(line)
        kind = payload["kind"]

        if kind == "tick":
            return {"kind": "tick"}

        if kind == "tmotor":
            return {
                "kind": kind,
                "motor": payload["motor"],
                "command": TMotorData(
                    position=float(payload["position"]),
                    velocityERPM=float(payload["velocityERPM"]),
                    mode=int(payload["mode"]),
                    useBrake=int(payload["useBrake"]),
                ),
            }

        if kind == "maxon":
            return {
                "kind": kind,
                "motor": payload["motor"],
                "command": MaxonData(
                    position=float(payload["position"]),
                    mode=int(payload["mode"]),
                    kp=int(payload["kp"]),
                    kd=int(payload["kd"]),
                ),
            }

        if kind == "dxl":
            return {
                "kind": kind,
                "motor": payload["motor"],
                "position": float(payload["position"]),
            }

        raise ValueError(f"Unsupported command kind: {kind}")

# command_applier에 넘겨서 URDF joint dict 만듬
def build_joint_targets(applier: CommandApplier, message: dict) -> Dict[str, float]:
    if message["kind"] in {"tmotor", "maxon"}:
        return applier.apply_can_command(message["motor"], message["command"])

    if message["kind"] == "dxl":
        return applier.apply_dxl_command(message["motor"], message["position"])

    return {}


def build_startup_joint_targets(applier: CommandApplier) -> Dict[str, float]:
    joint_targets_deg: Dict[str, float] = {}

    for motor_name, target_deg in STARTUP_CAN_POSE_DEG.items():
        command = applier.build_default_can_command(motor_name, target_deg)
        joint_targets_deg.update(applier.apply_can_command(motor_name, command))

    for motor_name, target_deg in STARTUP_DXL_POSE_DEG.items():
        joint_targets_deg.update(applier.apply_dxl_command(motor_name, target_deg))

    return joint_targets_deg

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Read command-level SIL messages from a named pipe and apply them to PyBullet."
    )
    parser.add_argument(
        "--pipe",
        type=Path,
        default=DEFAULT_PIPE_PATH,
        help="Named pipe path. Must match the C++ SilCommandPipeWriter path.",
    )
    parser.add_argument(
        "--mode",
        default="gui",
        choices=["gui", "direct"],
        help="PyBullet connection mode.",
    )
    parser.add_argument(
        "--urdf",
        type=Path,
        default=DEFAULT_URDF_PATH,
        help="URDF file to load.",
    )
    parser.add_argument(
        "--sleep",
        type=float,
        default=0.0001,
        help="Optional delay after each applied message.",
    )
    parser.add_argument(
        "--no-vcan",
        action="store_true",
        default=False,
        help="vcan0 피드백을 비활성화한다 (open-loop 디버그용).",
    )
    parser.add_argument(
        "--vcan",
        type=str,
        default="vcan0",
        help="vcan 인터페이스 이름 (기본값: vcan0).",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    reader = SilCommandPipeReader(args.pipe)
    applier = CommandApplier()
    backend = PyBulletBackend(urdf_path=args.urdf, mode=args.mode)

    # close-loop: vcan0 피드백 송신기
    vcan_writer: Optional[VcanStateWriter] = None
    if not args.no_vcan:
        vcan_writer = VcanStateWriter(channel=args.vcan)
        if not vcan_writer.open():
            print(f"[SIL] vcan writer disabled (interface '{args.vcan}' unavailable).")
            vcan_writer = None
        else:
            print(f"[SIL] vcan feedback enabled on {args.vcan}.")

    try:
        reader.prepare_pipe()
        backend.start()
        startup_joint_targets_deg = build_startup_joint_targets(applier)
        if startup_joint_targets_deg:
            print(f"Applying startup preset pose: {startup_joint_targets_deg}")
            backend.apply_targets(startup_joint_targets_deg)
            backend.step()
            # startup pose도 즉시 피드백으로 전송한다.
            if vcan_writer is not None:
                vcan_writer.send_all(backend.read_joint_states())
        print(f"Listening on named pipe: {args.pipe}")

        # tick 단위로 joint target을 모아서 한 번에 apply → step 한다.
        # 같은 1ms 구간의 TMotor/Maxon/DXL 명령이 atomic하게 적용되어
        # DXL과 CAN 모터의 frame 경계가 맞춰진다.
        #
        # close-loop:
        # tick 처리 후 PyBullet joint state를 vcan0 CAN 프레임으로 C++에 돌려준다.
        # C++ CanManager recv loop 가 이 프레임을 읽어 motor.jointAngle 을 갱신한다.
        #
        # Idle return:
        # 실제 관절 명령이 없는 빈 tick이 IDLE_RETURN_SEC 초 이상 이어지면
        # startup pose로 복귀한다. SIL 전용 임시 기능.
        frame_targets: Dict[str, float] = {}

        for message in reader.read_messages():
            if message["kind"] == "tick":
                if frame_targets:
                    backend.apply_targets(frame_targets)
                    frame_targets.clear()

                # 빈 tick이든 아니든 step() 과 vcan_writer.send_all() 은 항상 호출한다.
                # 그래야 시뮬레이터 시간이 흐르고, vcan 피드백도 1ms 주기로 일정하게 돌아간다.
                backend.step()
                if vcan_writer is not None:
                    vcan_writer.send_all(backend.read_joint_states())

                if args.sleep > 0.0:
                    time.sleep(args.sleep)
            else:
                joint_targets_deg = build_joint_targets(applier, message)
                frame_targets.update(joint_targets_deg)

        return 0
    finally:
        if vcan_writer is not None:
            vcan_writer.close()
        backend.close()
        reader.cleanup_pipe()


if __name__ == "__main__":
    raise SystemExit(main())
