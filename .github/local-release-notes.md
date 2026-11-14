Experimental local two-player split-screen for GTA San Andreas 1.0 US on Windows 10 or later.

Preview 4 embeds both games in **one borderless fullscreen window** with one taskbar/Alt+Tab entry.

- Both top/bottom and left/right layouts fill the primary display.
- F10 opens session controls; Return to game / F10 restores fullscreen and the keyboard player's focus.
- Alt+F4 asks to close both games. Alt+Tab and minimizing release gameplay input.
- DirectInput uses an invisible top-level owner in each game process, because the visible game windows are now children.
- Save loading now leaves native filename generation intact and uses a scoped short prefix instead of the old filename hook.
- Valid snapshots receive checksum-preserving four-byte alignment when needed. Original input saves remain unchanged.
- Native storage error 7 is no longer the only diagnostic: the original error and file-open result are shown and logged.

Download **SA-COOP.zip**. Close both games and `coop-local.exe`, then extract the complete archive over the same GTA folder
and launch `coop-local.exe` again. GTA game files are not included. Save before ending a session.

**Gameplay and the reported bike/world-load failure still require confirmation in GTA.** The user's Preview 3 failure was
storage error 7; that generic code alone does not establish the underlying cause. This preview corrects the filename
integration and adds diagnostics; it does not disable checksum or script compatibility checks. If loading still fails,
open F10 → Saves / Logs and capture both player logs plus `player2/world/partner_world.b`.

Automated checks cover input mappings, save aliases/alignment, native launcher controls and real Win32 dummy game/server
processes, including embedded layouts, focus and shutdown. They do not run GTA or test physical controllers.
