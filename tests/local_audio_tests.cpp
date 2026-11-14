#include <cassert>
#include <iostream>
#include "common/lib/local-coop/AudioPolicy.h"

int main() {
    using namespace coop::local;
    for (bool localSession : {false, true}) for (bool host : {false, true}) {
        for (float volume : {-100.0f, -16.0f, -3.0f, 0.0f, 6.0f}) {
            assert(LocalAudioVolume(localSession, host, AudioSource::Effects, volume) == volume);
            const float expected = localSession && !host ? -100.0f : volume;
            assert(LocalAudioVolume(localSession, host, AudioSource::Radio, volume) == expected);
            assert(LocalAudioVolume(localSession, host, AudioSource::Cutscene, volume) == expected);
        }
    }
    assert(ShareMissionSpeech(false) && !ShareMissionSpeech(true));
    std::cout << "Local audio: host/online levels preserved, partner radio/cutscene muted, effects untouched\n";
}
