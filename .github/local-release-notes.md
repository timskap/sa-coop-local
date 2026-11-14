Experimental local two-player split-screen for GTA San Andreas 1.0 US on Windows 10 or later.

Preview 6 corrects the post-effect resize path introduced with fullscreen host cutscenes in Preview 5.

- Refreshes GTA's post-effect front raster, filter vertices and UV coordinates when the host changes between split-screen and fullscreen.
- Waits until the native camera and actual D3D backbuffer agree before rebuilding. Invalid or undersized effect resources are retried without blending their stale image over the new scene.
- Refreshes coordinates even when the new viewport fits in the same texture allocation. Normal color, grain and other effects remain enabled.
- Retains host-only radio/dialogue, automatic host cutscene presentation, split restoration and keyboard/mouse focus recovery from Preview 5.

The reported photo is consistent with the old half-screen effect raster retaining a previous shot after a full-frame copy
fails. The missing native post-effect reinitialization is corrected, but **the photographed issue still needs an in-game retest**.
This environment does not run GTA or validate its rendered pixels.

Download **SA-COOP.zip**. Close both games and `coop-local.exe`, extract the **complete archive** over the same GTA folder,
then start `coop-local.exe` again. Save before closing. No GTA game files are included.

If the overlay remains, open F10 → Saves / Logs and collect `player1/local.log`. New lines beginning
`Local post effects refreshed` report camera, backbuffer and effect-raster dimensions. The prior bike/save-load report is
not claimed resolved by these render changes; its Preview 4 diagnostics are retained.

This archive comes unchanged from [Windows build 43612172812](https://github.com/timskap/sa-coop-local/actions/runs/43612172812)
of commit `265234f0c54be9f22c45ec9938957342ac82554d`. Full compilation, FSD/i18n checks, portable
input/save/audio/render-resize regressions, launcher smoke and native session integration passed. The new render tests cover
split/full/vertical transitions, same-capacity UV changes, delayed device resets, recreated rasters and allocation recovery;
they do not execute GTA's native allocator. `SHA256SUMS.txt` contains the archive checksum.

[Setup and testing notes](https://github.com/timskap/sa-coop-local/blob/265234f0c54be9f22c45ec9938957342ac82554d/docs/LOCAL_COOP.md)
