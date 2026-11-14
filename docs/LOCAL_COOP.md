# Local split-screen co-op (experimental)

This fork adds **`coop-local.exe`**, a native Windows setup screen inspired by sports-game side selection.
Assign a keyboard/mouse or a connected XInput controller to Player 1 and Player 2, ready both players, then play
with a top/bottom or left/right split on the primary monitor.

The implementation launches **two GTA SA processes**, embedded as two child viewports of **one borderless fullscreen window**, connected through
SA:COOP on localhost. It does not implement two cameras in one game process. Budget CPU, GPU and memory for two games.
No GTA files are included. Requires **Windows 10 or later**, your own **GTA San Andreas 1.0 US**, the ASI loader,
and the client/server from this build. Both launcher and clients use system DPI awareness so viewport coordinates
remain in physical pixels when Windows display scaling is above 100%.

## Start

1. Build the fork with `python tools/build.py dist`, or download the `SA-COOP` artifact from a successful Actions run.
2. Extract `SA-COOP.zip` into a test GTA installation and use the existing `install.bat` instructions to enable its ASI loader.
   Keep `coop-local.exe` next to `gta_sa.exe`; `coop.asi` and `coop/coop-server.exe` must be from this fork.
3. Connect your controllers and open `coop-local.exe`.
4. Use a controller's D-pad/stick left/right to choose Player 1/2, then A to ready. On the keyboard, use left/right and Enter.
   The on-screen assignment and readiness buttons work with a mouse too. B/Escape releases a device in the setup screen.
5. Choose a layout and story save. **S** cycles occupied save slots and New game; **L** changes layout.
   The primary display must provide at least 640 x 360 for each viewport.
6. Press the **Play local co-op** button (or controller Start after both players are ready).
   Player 1 starts the local server first; Player 2 starts only after the host's network handshake succeeds.
7. The setup screen hides and both views fill one fullscreen window, with one taskbar/Alt+Tab entry.
   **F10** opens session controls; **Return to game / F10** restores fullscreen and the assigned keyboard player.
   **Alt+F4** asks to end both games. Save first. Alt+Tab switches away from the whole session.
   If you change monitor resolution during play, end and restart the session to recreate both render targets.

**New game retains upstream behavior:** Player 1 plays the opening prologue alone and Player 2 waits. Load a save
after the prologue for an immediate two-player start.

## Shared audio and cutscenes

In local play, Player 1 (the host) provides the radio, cutscene soundtrack and shared mission dialogue.
Player 2's effects remain audible. Radio/cutscene playback still advances silently in Player 2 so native scene clocks
and completion checks keep working. Ordinary online sessions retain their existing audio behavior.

During the host's cutscenes and scripted mission camera scenes, its viewport automatically fills the whole container.
The host window receives a real resize through GTA's native camera/RenderWare path, with the local camera aspect based
on its current dimensions. The partner keeps running underneath it. Ending, skipping or leaving the scene restores the
selected split, including after a world reload or disconnect; device assignments stay unchanged.

Preview 6 fixes an omission in Preview 5's resize path: native camera resizing alone did not update GTA's
post-effect front raster, filter vertices and UVs. An undersized raster can retain the previous shot when the new
full-frame copy fails, producing a translucent old image over part of a cutscene. Local rendering now refreshes
these resources through `CPostEffects::SetupBackBufferVertex` immediately before post-processing, once the camera
and actual D3D backbuffer agree. It also refreshes on return to split-screen, including changes that fit in the same
texture allocation. Invalid or unavailable resources defer post-processing for that frame and retry; gameplay and
the new scene continue rendering. Normal color, grain and other effects remain enabled.

The refresh is logged as `Local post effects refreshed` with camera/backbuffer/effect dimensions. This correction
is based on the native render path; the user's photographed artifact still needs an in-game retest.

Restoring from the taskbar, Alt+Tab or F10 retries focus handoff to the keyboard seat, resets stale key/mouse state and
reacquires the native DirectInput mouse. No focus request activates a background session. If another application is
foreground, input stays released until the game is brought back.

## Devices and controls

Supported combinations are keyboard/mouse + controller (either player can use the keyboard), or two different
XInput controllers. One keyboard/mouse cannot belong to both players. Two independent keyboards and native
DirectInput-only controllers are not implemented. Controller numbers refer to Windows XInput slots, not USB port order.

| Control | On foot | In a vehicle |
| --- | --- | --- |
| Left stick | Move | Steer |
| Right stick | Camera | Camera / vehicle-specific controls |
| A | Sprint / swim | Handbrake |
| X | Jump / climb | Secondary action / nitro |
| Y | Enter vehicle | Exit vehicle |
| B | Fire / melee | Fire |
| RT | Fire | Accelerate |
| LT | Aim | Brake / reverse |
| LB / RB | Previous / next weapon | Look left / right |
| LB + RB | Pickup / action (GTA L1) | Look behind |
| Left stick click | Crouch | Horn |
| Right stick click | Look behind | Look behind |
| D-pad / Back / Start | GTA's normal pad actions, camera and pause | GTA's normal pad actions, camera and pause |

Keyboard/mouse uses the game's existing bindings. The controller seat uses GTA's classic pad mode with the mapping
above. Controllers continue to drive their own player while the shared fullscreen window is foreground. Alt-tabbing to an
unrelated application releases controller input; unplugging a pad produces neutral input until that XInput slot returns.
In setup, unplugging also removes assignment and readiness, so reconnecting cannot launch a session accidentally.

Custom SA:COOP overlay shortcuts (chat, F9 connection settings, invitations, etc.) are still keyboard-oriented.
Click the controller player's window to use its overlay shortcuts and mouse UI; these keys are blocked from GTA's
gameplay input. Click the keyboard player's window again to resume keyboard gameplay. Full controller navigation
of those overlays is not implemented. For initial testing use the stock supported game and
this mod without additional input hooks such as GInput. Compatibility with other input/window mods needs validation.

## Saves and local networking

- **Saves / Logs** opens `%LOCALAPPDATA%/SA-COOP/local-<install-id>/`.
- Each player has separate `local.ini`, `local.log`, `crash.log`, world snapshots and redirected GTA user files.
- A selected existing Player 1 save is copied into the local profile once, without replacing an existing local save.
  Later saves stay in the local profile; the original single-player save is not overwritten.
- The server's profile/world data is under `player1/server-data/`. Player 2 joins Player 1's world through upstream
  synchronization. Nicknames remain `Player1` and `Player2` so server profiles are stable across launches.
- The local server binds only `127.0.0.1`, has two player slots, and does not start UPnP, STUN, relay, or public invites.
  The launcher chooses a free UDP port. No firewall permission prompt is requested for local play.
- Voice chat is disabled in this mode. Both games otherwise retain their own sound output.
- Only one launcher per game installation can run. The launcher manages only the two processes it starts; local
  hosting does not run upstream's scan that terminates other servers from the same game folder.
- Both games and their server are assigned to an owned Windows job at process creation. Closing or forcibly
  terminating the launcher ends this process tree, including after a launcher crash. It does not save the game.

### First world load after getting off the bike

After the prologue, the host cannot create a world snapshot while riding. Getting off the bike allows the first snapshot,
which Player 2 then loads. Preview 3's path/checksum changes did **not** resolve the reported installation: GTA still
returned native storage error **7 (SLOT_INVALID)**. This is a generic wrapper result, not evidence of a version mismatch.

Preview 4 removes the replacement `GenerateGameFilename` hook. The original game builds filenames using a temporary short
prefix during validation, preserving its native calling convention and extension handling. The normal prefix is then
restored byte-for-byte. File-open redirection resolves the short alias to the full per-player path; no long path is copied
into the two 104-byte load buffers. Valid snapshots are aligned to a four-byte boundary with zero padding before the
checksum, matching the native final-buffer read requirement. An unaligned input save is loaded through a separate copy;
the original save is not rewritten. Corrupt checksums are never repaired or ignored, and main.scm compatibility is still checked.

The original validation error is captured **before** GTA replaces it with 7. The screen reports the wrapper code, original
error, and successful/attempted opens. `local.log` also records the requested/resolved filename, file size, read position and
buffer state. If the failure remains, open **Saves / Logs** (F10 during play) and collect `player1/local.log` and
`player2/local.log` immediately after the failure. Keep `player2/world/partner_world.b` for diagnosis as well.
The new path is not yet confirmed against the user's actual game or failed snapshot.

## Implementation and verification

`src/local` is a separate native Win32/GDI application, following the repository's layer rules. Shared, platform-independent
assignment and viewport logic is in `common/lib/local-coop/Setup.h`. The client applies input isolation before
`CPad::Update` reconciles temporary keyboard, joystick and mouse states, preserving GTA's old/new button history and
steering/horn bookkeeping. Existing ordinary online input handling is unchanged unless `[local] enabled = 1`.
Embedded input uses a hidden top-level window in each game process for DirectInput background/nonexclusive ownership.
Keyboard/mouse and controller state are gated by the foreground session and assigned seat, including when minimized.
Structure offsets were checked against [plugin-sdk's SA CPad](https://github.com/DK22Pac/plugin-sdk/blob/master/plugin_sa/game_sa/CPad.h)
and the update order against [gta-reversed's Pad.cpp](https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Pad.cpp).

Automated checks:

```text
python tools/fsd_check.py
python tools/i18n_check.py
python tools/test_local_coop.py
python tools/build.py dist
python tools/test_local_launcher.py  # Windows only; creates and exercises the real launcher window
python tools/test_local_session.py   # Windows only; actual Session code with dummy games/server
```

`test_local_coop.py` checks all supported device pairs, device movement without duplicate ownership, readiness,
disconnect/reconnect, odd-resolution splits, every signed 16-bit axis value, and the actual controller mapping including
pickup, vehicle actions and neutral input. The Windows UI smoke test checks real native controls and launch gating.
Render-resize regressions cover split/full/vertical transitions, exact-size UV refresh within one texture capacity,
delayed device reset, missing/undersized allocations and retry after failure. They test the production refresh gating,
not GTA's native raster allocator or rendered pixels.
Save-file tests exercise paths longer than GTA's legacy buffers, scoped native prefixes, alignment without payload changes,
snapshot transfers, corrupt/truncated saves and recovery from a previous storage error. They test the shared path/checksum logic, without executing GTA's native save routines.
Session tests use dummy game/server processes to check private environments, failed starts, host-before-partner readiness,
available fullscreen layouts without gaps/overlaps, child HWND ownership, repeated cinematic resize/restore and sibling order,
both keyboard seats, repeated taskbar/Alt+Tab restoration with deliberately dropped focus requests, actual DirectInput mouse
unacquire/reacquire, stale activation messages, F10 controls, restart, one player exiting, normal shutdown and abrupt launcher termination.
Small CI displays explicitly reject undersized vertical layouts; portable tests cover both split calculations. These tests do not exercise
the GTA engine. Build outputs are 32-bit Windows binaries.
Linux cross-compilation can use the matching official llvm-mingw Linux distribution at `toolchain/llvm-mingw`.

**Gameplay is not yet verified.** Before treating this branch as release-ready, test on Windows with a legal game copy:

- Both keyboard/controller permutations and two controllers; walking, aiming, shooting, swimming and driving independently.
- Both layouts, controller reconnect, Alt+Tab, native pause menus, different desktop scaling and focus restoration.
- New-game prologue, an existing save, a story mission, a cutscene, death, safe-house save and restart.
- Radio/cutscene/mission speech audible once from the host, both players' effects still audible; scene completion without hangs.
- Fullscreen host cutscenes and scripted camera scenes, skip/end/abort restoration, correct aspect and subtitles in both layouts.
- Startup failure, a missing/old ASI, occupied port, one game closing, and launcher shutdown without orphaned servers.
- Controller camera behavior, overlay accessibility, audio overlap and compatibility with any additional mods.

The launcher has 120-second startup timeouts and reports errors instead of declaring a disconnected second player ready.
Closing one game ends the other local game as well. It does not autosave; save in-game before ending a session.
