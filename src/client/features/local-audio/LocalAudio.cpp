#include <windows.h>
#include <cstdint>
#include "common/lib/local-coop/AudioPolicy.h"
#include "common/lib/log/Log.h"
#include "shared/config/Config.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"

namespace coop {
namespace {
using VolumeFn = void(__fastcall*)(void*, void*, int16_t, uint16_t, float, uint8_t);
using UpdateFn = void(__fastcall*)(void*, void*);
VolumeFn originalVolume = nullptr;
UpdateFn originalRadio = nullptr, originalCutscene = nullptr;
thread_local local::AudioSource source = local::AudioSource::Effects;
struct AudioScope {
    local::AudioSource previous = source;
    explicit AudioScope(local::AudioSource current) { source = current; }
    ~AudioScope() { source = previous; }
};
void __fastcall Volume(void* hardware, void* edx, int16_t channel, uint16_t id, float volume, uint8_t flags) {
    originalVolume(hardware, edx, channel, id,
                   local::LocalAudioVolume(Cfg().localCoop, Cfg().wantsHost, source, volume), flags);
}
void __fastcall Radio(void* manager, void* edx) {
    const AudioScope scope(local::AudioSource::Radio);
    originalRadio(manager, edx);
}
void __fastcall Cutscene(void* manager, void* edx) {
    const AudioScope scope(local::AudioSource::Cutscene);
    originalCutscene(manager, edx);
}
class LocalAudio final : public Module {
public:
    const char* Name() const override { return "LocalAudio"; }
    void OnInstallHooks() override {
        if (!Cfg().localCoop || Cfg().wantsHost) return;
        // Mute the volume writes, not playback: cutscene clocks/status and radio queues
        // must still advance. Other hardware volume calls (effects/loading/ambience) pass through.
        const bool volume = mem::HookFn(0x4D8870, &Volume, &originalVolume, "CAEAudioHardware::SetChannelVolume (local audio)");
        const bool radio = mem::HookFn(0x4EA010, &Radio, &originalRadio, "CAERadioTrackManager::UpdateRadioVolumes (local audio)");
        const bool cutscene = mem::HookFn(0x4DBF10, &Cutscene, &originalCutscene, "CAECutsceneTrackManager::StartTrackPlayback (local audio)");
        if (!volume || !radio || !cutscene) {
            LOGE("Local host-only audio hooks could not be installed");
            TerminateProcess(GetCurrentProcess(), 6);
        }
        LOGI("Local audio: radio and cutscene tracks are audible only in Player 1");
    }
} instance;
struct Register { Register() { core::RegisterModule(&instance); } } registration;
}
}
