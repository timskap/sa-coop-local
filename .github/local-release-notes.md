Experimental local two-player split-screen for GTA San Andreas 1.0 US on Windows 10 or later.

Preview 2 fixes issues found in a follow-up review:
- Controller seats can use F9, chat and invitation shortcuts in the focused window without sending keyboard gameplay input to GTA.
- LB + RB performs pickup/action on foot; X performs the secondary vehicle action/nitro.
- A controller player can participate in cutscene skip votes while the partner window is foreground.
- Client DPI awareness matches the launcher for Windows display scaling above 100%.
- Closing or forcibly terminating the launcher closes both games and their descendant server, including after a crash.
- UNC profile paths are preserved correctly.

Download **SA-COOP.zip**, install it into your GTA SA 1.0 US folder using the included installer instructions,
then launch **coop-local.exe** next to `gta_sa.exe`. GTA game files are not included.
Supports keyboard/mouse + one XInput controller (either player) or two different XInput controllers.
Top/bottom and left/right layouts use two game processes connected over localhost.

This is the unchanged archive from the [successful Windows build](https://github.com/timskap/sa-coop-local/actions/runs/43601577711)
of the tagged commit `def3e1abca08334f1b479961fa63835d83a84d68`. Compilation, input regression checks,
the native launcher UI smoke test, and Session integration tests with dummy game/server processes passed.
Session tests cover failed startup, environment isolation, host-before-partner readiness, restart, focus handoff,
one player exiting, normal shutdown and abrupt launcher termination. The SHA-256 is provided in `SHA256SUMS.txt`.

**Actual GTA gameplay and physical controllers have not yet been tested.** The opening prologue is played by Player 1 alone;
Player 2 waits. Full controller navigation of custom SA:COOP overlays is not implemented: focus the desired player's window
and use keyboard/mouse for its overlays. Save in-game before ending the session: closing either game closes both.
This preview is not marked as a stable release.

[Setup, controls and testing notes](https://github.com/timskap/sa-coop-local/blob/def3e1abca08334f1b479961fa63835d83a84d68/docs/LOCAL_COOP.md)
