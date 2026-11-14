Experimental local two-player split-screen for GTA San Andreas 1.0 US on Windows 10 or later.

Preview 7 adds the missing native-menu route for Xbox controller buttons.

| Xbox button | During gameplay | In GTA's native menu |
| --- | --- | --- |
| Menu / Start | Open menu | Escape / back / close pause root |
| View / Back / Select | Change camera view | Back / close pause root |
| A | Existing gameplay action | Confirm |
| B | Existing gameplay action | Back / close pause root |
| D-pad / left stick | Existing gameplay action | Navigate |
| X | Existing gameplay action | Toggle waypoint on map |
| LB / RB | Existing gameplay action | Zoom on map |

The PC frontend checks keyboard Escape, so setting a controller's Start field alone did not open it. This build
bridges the assigned controller into that game's native menu update without sending OS key presses or moving focus
from the keyboard player's window. A separate button history prevents a held Menu from toggling repeatedly, or B
from firing when leaving the menu. Buttons held during focus loss, overlay capture or reconnection must be released
before acting again. As before, the shared world continues running while a native menu is open.

Custom SA:COOP overlays (F9, chat, invitations) remain keyboard/mouse oriented. Preview 6's post-effect resize fix,
host-only radio/dialogue, fullscreen host cutscenes and focus recovery are retained.

Download **SA-COOP.zip**. Close both games and `coop-local.exe`, extract the **complete archive** over the same GTA folder,
then launch `coop-local.exe`. Save before closing. No GTA game files are included.

This archive comes unchanged from [Windows build 43613213633](https://github.com/timskap/sa-coop-local/actions/runs/43613213633)
of commit `56d1d893db48a394e50992dc1d0304c659bbff19`. Full compilation, FSD/i18n checks, portable
input/save/audio/render/menu regressions and native launcher/session integration passed. New menu tests cover physical button
edges, scoped Escape restoration, independent seats, held-button recovery and foot/vehicle menu mappings.
**Actual GTA frontend behavior on a physical Xbox controller still needs an in-game retest.** These tests use portable
logic and dummy game processes. `SHA256SUMS.txt` contains the archive checksum.

[Setup and testing notes](https://github.com/timskap/sa-coop-local/blob/56d1d893db48a394e50992dc1d0304c659bbff19/docs/LOCAL_COOP.md)
