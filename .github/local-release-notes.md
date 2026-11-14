Experimental local two-player split-screen for GTA San Andreas 1.0 US on Windows 10 or later.

Preview 5 makes the host the shared audio and cinematic view for local play, and repairs input when restoring the game.

- Radio, cutscene soundtracks and shared mission dialogue are audible only from Player 1. Player 2 keeps effects; native playback clocks continue silently.
- During host cutscenes and scripted mission camera scenes, the host fills the fullscreen container. Native camera resizing and a local aspect-ratio correction replace stretching a half-screen frame. The selected split returns when the scene ends, is skipped or is left.
- Player 2 keeps running underneath the host view. Device assignments stay unchanged.
- Taskbar, Alt+Tab and F10 restoration retry handoff to the keyboard player, clear stale key/mouse state and reacquire the DirectInput mouse. Queued activation messages cannot overwrite the current foreground state.
- Includes Preview 4's save-path/alignment correction and detailed native storage diagnostics.

Download **SA-COOP.zip**. Close both games and `coop-local.exe`, extract the **complete archive** over the same GTA folder,
then launch `coop-local.exe`. F10 opens session controls; Alt+F4 asks to end both games. Save first. No GTA game files are included.

**Actual GTA gameplay, audible output and native render resizing still need in-game verification.** The prior bike/world-load
failure is not claimed fixed by dummy-process tests. If loading still fails, open F10 → Saves / Logs and collect both player
logs plus `player2/world/partner_world.b`.

This archive comes unchanged from the [Windows build](https://github.com/timskap/sa-coop-local/actions/runs/43609770551)
of commit `e102eb2de6fdcdbb055fd0a40a14b7b0ca8cea38`. Full compilation, FSD/i18n checks, portable input/save/audio regressions,
launcher smoke and native session integration passed. Session tests exercise both keyboard seats,
repeated cinematic window resize/restore, deliberately dropped focus requests, actual DirectInput unacquire/reacquire and
process cleanup. They use dummy games, not GTA. The small CI display uses horizontal fullscreen and verifies rejection of
an undersized vertical layout; portable tests cover both split calculations. `SHA256SUMS.txt` contains the archive checksum.

[Setup and testing notes](https://github.com/timskap/sa-coop-local/blob/e102eb2de6fdcdbb055fd0a40a14b7b0ca8cea38/docs/LOCAL_COOP.md)
