Experimental local two-player split-screen for GTA San Andreas 1.0 US on Windows 10 or later.

Preview 3 addresses Player 2 being disconnected with a misleading version-mismatch error when Player 1 gets off the bike
and the first host world snapshot becomes available.

- Full per-player save paths no longer enter GTA's two 104-byte load-name buffers. Short tokens are resolved to the real
  paths only at the native file-open boundary.
- A previous storage error is cleared before validating or saving the next snapshot.
- Damaged or incomplete snapshots are rejected by checksum before upload/load.
- File-read and native-validation failures report their actual category instead of guessing that versions differ.
- The main.scm compatibility check is retained. No save format or network protocol changes; existing saves stay in place.

Download **SA-COOP.zip**. Close both games and `coop-local.exe`, then extract the complete archive over the same GTA folder
and launch `coop-local.exe` again. GTA game files are not included.

This is the unchanged archive from the [successful Windows build](https://github.com/timskap/sa-coop-local/actions/runs/43603016867) of commit
`e96d8235fa6490771eb00efa46cd5a3ebf15cabe`. Full compilation, FSD/i18n checks, input tests, save-path/checksum/error-recovery
regressions, the native launcher smoke test and process lifecycle tests passed. `SHA256SUMS.txt` contains the download checksum.

**The reported scenario still needs confirmation inside GTA on the user's installation.** Automated tests exercise the
shared file logic and real launcher processes, not the proprietary game engine or physical controllers. If the problem
persists, use **Saves / Logs** and capture `player1/local.log` and `player2/local.log` immediately after the failure.

Custom mod overlays still use keyboard/mouse. Save before ending a session: closing either game closes both.

[Setup and testing notes](https://github.com/timskap/sa-coop-local/blob/e96d8235fa6490771eb00efa46cd5a3ebf15cabe/docs/LOCAL_COOP.md)
