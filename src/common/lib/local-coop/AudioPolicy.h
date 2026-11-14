#pragma once

namespace coop::local {
enum class AudioSource { Effects, Radio, Cutscene };
inline float LocalAudioVolume(bool localSession, bool host, AudioSource source, float volume) {
    return localSession && !host && source != AudioSource::Effects ? -100.0f : volume;
}
inline bool ShareMissionSpeech(bool localSession) { return !localSession; }
}
