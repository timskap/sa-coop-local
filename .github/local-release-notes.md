Experimental local two-player split-screen for GTA San Andreas 1.0 US.

- FIFA-style Player 1 / Player 2 device selection and readiness.
- Keyboard/mouse + XInput controller (either player), or two different XInput controllers.
- Top/bottom or left/right split, using two game processes connected over localhost.
- Automatic host/partner startup and separate player saves, configs and logs.

Download **SA-COOP.zip**, install it into your GTA SA 1.0 US folder using the included installer instructions,
then launch **coop-local.exe** next to `gta_sa.exe`. GTA game files are not included.

This is the unchanged archive from the [successful Windows build](https://github.com/timskap/sa-coop-local/actions/runs/43599158082)
of commit `31500475339adae0c216024ebff09036dcd3d9ed`. Compilation, regression checks and the native launcher UI smoke test passed.
The download's SHA-256 is provided in `SHA256SUMS.txt`.

**Actual GTA gameplay and physical controllers have not yet been tested.** The opening prologue is played by Player 1 alone;
Player 2 waits. Full controller navigation of custom SA:COOP overlays is not implemented. Save in-game before ending the
session: closing either game closes both. This preview is not marked as a stable release.

[Setup, controls and testing notes](https://github.com/timskap/sa-coop-local/blob/31500475339adae0c216024ebff09036dcd3d9ed/docs/LOCAL_COOP.md)
