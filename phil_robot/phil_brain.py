# phil_robot/phil_brain.py

import os
import time

import numpy as np
import psutil
import sounddevice as sd
import whisper

from config import PLANNER_MODEL, CLASSIFIER_MODEL
from pipeline.brain_pipeline import run_brain_turn
from pipeline.exec_thread import InterruptibleExecutor
from pipeline.robot_graph import build_phil_graph
from pipeline.session import SessionContext, update_session
from runtime.melo_engine import TTS_Engine
from runtime.phil_client import RobotClient, get_robot_state_snapshot

# ==========================================
# Config
# ==========================================
SAMPLE_RATE = 16000
RECORD_SECONDS = 3
HOST = "127.0.0.1"
PORT = 9999


def get_mem_usage():
    """현재 프로세스의 메모리 사용량을 MB 단위로 반환"""
    process = psutil.Process(os.getpid())
    return process.memory_info().rss / (1024 * 1024)


def record_audio():
    """마이크로 소리를 듣고 1차원 float 배열로 반환"""
    print(f"\n🎤 듣는 중... ({RECORD_SECONDS}초)")
    try:
        audio = sd.rec(
            int(RECORD_SECONDS * SAMPLE_RATE),
            samplerate=SAMPLE_RATE,
            channels=1,
            dtype="float32",
        )
        sd.wait()
        return audio.flatten()
    except Exception as exc:
        print(f"❌ 마이크 녹음 실패: {exc}")
        return None


def warm_up_stt_model(stt_model):
    """초기 추론 지연을 줄이기 위해 무음 샘플로 모델을 예열"""
    print("🔥 모델 예열 중... (잠시만 기다려주세요)")
    try:
        dummy_audio = np.zeros(16000, dtype=np.float32)
        stt_model.transcribe(dummy_audio, fp16=False, language="ko")
    except Exception:
        pass


def load_runtime():
    """
    대화 루프에 필요한 런타임 객체를 준비한다.
    phil_brain.py 는 객체 생성과 메인 루프 orchestration에만 집중한다.
    """
    bot = RobotClient(host=HOST, port=PORT)
    if not bot.connect():
        print("연결 실패")
        return None, None, None

    base_mem = get_mem_usage()
    print(f"[{time.strftime('%H:%M:%S')}] 초기 메모리: {base_mem:.2f} MB")

    tts = TTS_Engine()
    tts_mem = get_mem_usage()
    print(f"[{time.strftime('%H:%M:%S')}] TTS 로드 후: {tts_mem:.2f} MB (증가량: {tts_mem - base_mem:.2f} MB)")

    print("[STT] Whisper 모델 로딩 중...")
    stt_model = whisper.load_model("small", device="cuda")

    stt_mem = get_mem_usage()
    print(f"[{time.strftime('%H:%M:%S')}] STT 로드 후: {stt_mem:.2f} MB (증가량: {stt_mem - tts_mem:.2f} MB)")
    print(f"\n✅ 모델 로딩 완료! 총 점유: {stt_mem:.2f} MB")

    warm_up_stt_model(stt_model)
    print("✅[STT] 준비 완료!")
    return bot, tts, stt_model


def transcribe_user_speech(stt_model, audio_data):
    """STT 단계만 별도 함수로 분리해 메인 루프 가독성을 높인다."""
    stt_start_time = time.time()
    print("텍스트 변환 중...")
    result = stt_model.transcribe(audio_data, fp16=False, language="ko")
    user_text = result["text"].strip()
    print(f"🗣️ User: {user_text}")
    print(f"⏱️ STT 처리 시간: {time.time() - stt_start_time:.2f}초")
    return user_text


def format_metric(value):
    if not isinstance(value, (int, float)):
        return "N/A"
    return f"{value:.2f}s"


def print_llm_metrics(label, metric_obj):
    if not metric_obj:
        return

    prompt_tok = metric_obj.get("prompt_tokens")
    eval_tok = metric_obj.get("eval_tokens")
    wall = metric_obj.get("wall_sec")

    if metric_obj.get("backend") == "openai":
        total_tok = metric_obj.get("total_tokens")
        tps = metric_obj.get("eval_tps")
        tps_str = f"{tps:.1f}t/s" if isinstance(tps, float) else "N/A"
        print(
            f"[{label} LLM/API] wall={format_metric(wall)}, "
            f"prompt_tok={prompt_tok if isinstance(prompt_tok, int) else 'N/A'}, "
            f"eval_tok={eval_tok if isinstance(eval_tok, int) else 'N/A'}, "
            f"total_tok={total_tok if isinstance(total_tok, int) else 'N/A'}, "
            f"~{tps_str}"
        )
    else:
        print(
            f"[{label} LLM] wall={format_metric(wall)}, "
            f"load={format_metric(metric_obj.get('load_sec'))}, "
            f"prompt_eval={format_metric(metric_obj.get('prompt_sec'))}, "
            f"eval={format_metric(metric_obj.get('eval_sec'))}, "
            f"prompt_tok={prompt_tok if isinstance(prompt_tok, int) else 'N/A'}, "
            f"eval_tok={eval_tok if isinstance(eval_tok, int) else 'N/A'}, "
            f"overhead={format_metric(metric_obj.get('overhead_sec'))}"
        )


def debug_brain_result(brain_result):
    """LLM 입력, 내부 thinking, validator 경고를 한 곳에서 출력"""
    print(f"🧭 [Classifier 입력]\n{brain_result.classifier_input_json}")
    print(f"🧭 [Classifier 결과] {brain_result.classifier_result}")
    print(f"🧭 [Planner Domain] {brain_result.planner_domain}")
    print(f"🧐 [Planner 입력]\n{brain_result.planner_input_json}")
    print(f"🗺️ [Planner 결과] {brain_result.planner_result}")
    print(
        f"⏱️ LLM 처리 시간: 총 {brain_result.llm_duration_sec:.2f}초 "
        f"(classifier {brain_result.classifier_duration_sec:.2f}초 + planner {brain_result.planner_duration_sec:.2f}초)"
    )
    print_llm_metrics("Classifier", brain_result.classifier_metrics)
    print_llm_metrics("Planner", brain_result.planner_metrics)

    if brain_result.validated_plan.reason:
        print(f"\n[Planner Reason]\n{brain_result.validated_plan.reason}\n")

    if brain_result.validated_plan.expanded_op_cmds:
        print(f"[Planner Expanded] {brain_result.validated_plan.expanded_op_cmds}")
    if brain_result.validated_plan.resolved_op_cmds:
        print(f"[Planner Resolved] {brain_result.validated_plan.resolved_op_cmds}")
    if brain_result.validated_plan.valid_op_cmds:
        print(f"[Validator Accepted] {brain_result.validated_plan.valid_op_cmds}")
    if brain_result.validated_plan.rejected_op_cmds:
        print(f"[Validator Rejected] {brain_result.validated_plan.rejected_op_cmds}")

    for warning in brain_result.validated_plan.warnings:
        print(f"[Validator] {warning}")


def main():
    bot, tts, stt_model = load_runtime()
    if not all([bot, tts, stt_model]):
        return

    tts.speak("대화 준비가 되었습니다. 엔터 키를 누르고 말씀해 주세요.")

    # 세션 단기 기억
    session = SessionContext()

    # ── InterruptibleExecutor 초기화 ──────────────────────────────────────
    executor = InterruptibleExecutor(bot)

    # ── LangGraph 빌드 ────────────────────────────────────────────────────
    # get_session() 은 클로저로 최신 session 객체를 반환한다.
    # session 은 매 턴 끝에 재할당되므로 직접 참조 대신 getter 를 쓴다.
    def get_session():
        return session

    app = build_phil_graph(
        bot=bot,
        tts=tts,
        stt_model=stt_model,
        executor=executor,
        get_session=get_session,
        get_state_fn=get_robot_state_snapshot,
        run_brain_turn_fn=run_brain_turn,
        classifier_model=CLASSIFIER_MODEL,
        planner_model=PLANNER_MODEL,
    )

    try:
        while True:
            key = input("\n⌨️ [Enter] 듣기 / 'q' 종료 >> ")
            if key.lower() == "q":
                print("에이전트 종료")
                break

            # ── 실행 중이면 인터럽트 ──────────────────────────────────────
            # Enter 를 누른 시점에 executor 가 살아있으면 즉시 중단한다.
            # executor.cancel() 은 로봇에 's' 를 전송하고 스레드를 중단한다.
            if executor.is_running():
                print("⚡ [인터럽트] 이전 동작을 중단합니다.")
                executor.cancel()

            audio_data = record_audio()
            if audio_data is None:
                continue


            user_text = transcribe_user_speech(stt_model, audio_data)
            if not user_text:
                continue

            robot_state = get_robot_state_snapshot()

            # clarification 대기 중인지 안내한다.
            if session.pending_clarification_q:
                print(f"💬 [Clarification 대기 중] 필의 질문: {session.pending_clarification_q}")

            print("🧠 생각 중...")

            # ── LangGraph 실행 ────────────────────────────────────────────
            initial_state = {
                "user_text": user_text,
                "robot_hw_state": robot_state,
                "plan_type": "none",
                "speech": "",
                "commands": [],
                "play_modifier": {},
                "brain_result": {},
                "was_interrupted": False,
            }

            final_state = app.invoke(initial_state)

            # ── TTS 메인 스레드에서 호출 ─────────────────────────────────
            # MeloTTS(내부 PyTorch + C 라이브러리)는 스레드 안전하지 않다.
            # executor 는 백그라운드에서 이미 실행 중이고, TTS 는 메인 스레드에서
            # 호출한다. TCP send 는 거의 즉시 완료되므로 실질적으로 동시에 진행된다.
            speech = final_state.get("speech", "")
            if speech:
                print(f"🤖 Phil: {speech}")
                tts.speak(speech, stream=True)

            # ── 디버그 출력 ───────────────────────────────────────────────
            brain_result_obj = final_state.get("brain_result", {}).get("_obj")
            if brain_result_obj is not None:
                debug_brain_result(brain_result_obj)
                print(f"[Graph] plan_type={final_state.get('plan_type')} | "
                      f"interrupted={final_state.get('was_interrupted')}")

                # ── 세션 갱신 ─────────────────────────────────────────────
                session = update_session(session, user_text, brain_result_obj)

    except KeyboardInterrupt:
        print("\n종료합니다.")
    finally:
        if executor.is_running():
            executor.cancel()
        bot.close()


if __name__ == "__main__":
    main()
