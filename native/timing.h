#pragma once
namespace recvx {
// ps2_NaSystem.c Ps2SwapDBuff waits for two 60-Hz VSyncs before
// njUserMain runs again. Host presentation must not drive gameplay.
inline constexpr unsigned game_tick_hz=30;
inline constexpr unsigned audio_rate=48000;
inline constexpr unsigned audio_frames_per_tick=audio_rate/game_tick_hz;
inline constexpr double game_tick_seconds=1.0/game_tick_hz;
}
