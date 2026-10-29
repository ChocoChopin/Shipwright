# Upstream fix shortlist

Keep these independent of the native Player-rate mod. This is a list of PR
candidates, not a claim that they have been submitted or verified against the
latest upstream branch. Split mixed commits and check the current upstream base
when preparing each PR. Ordinary interpolated gameplay must exercise each fix
without enabling Player60/120.

| Fix | Destination / source | Current evidence and remaining work |
| --- | --- | --- |
| Avoid blocking resource loads for missing optional equipment | Shipwright; `4f7bd33ae`, `customequipment.cpp` | Captured 33.6-ms wait removed; canonical idle matches 41 boundaries; live follow-up found no equipment waits. Extract the existing-hook fix without the Player pose-admission caller. A real custom-model compatibility check remains. |
| Drain unused controller motion notifications in native Windows windows | libultraship; queue helper and DXGI event-loop hunks of `8dc5c763` | 22 focused checks, including backlog, hotplug retention and unchanged polled input. Live setup cost fell substantially. Split from the other renderer edits. |
| Append DX11 vertex uploads instead of discarding the buffer on every draw | libultraship; vertex cursor and DX11 hunks of `8dc5c763` | Seven cursor checks; captured Map stall motivated the change. Same allocation, approximately five discards per frame instead of 344. Separate ordinary-renderer qualification before PR. |
| Cache compiled DX11 shader bytecode | libultraship; shader-cache hunks of `8dc5c763` | Nine helper checks; bounded cache with source/options and payload validation. Independent improvement, not a claim that all stutters were shader compilation. Split from diagnostics and Player-specific switches. |
| Bound palette and I4 texture reads to resource storage | libultraship; `c6bbb8c3`, then `9280b17d` | 31 palette and 24 texture checks plus rebuilt-game validation. Preserve the explicit missing-byte policy. The I4 change builds on the palette helper; prepare in that order or as one small series. |
| Stop interpolating across the crawl-exit camera cut | Shipwright; `Camera_Demo9`, `788b5d9b1` | Recording shows malformed interpolation at the first-person-to-exit-shot cut. Build08's reset at the end did not fix it; Build09 resets the first evaluation of9601/9602 instead. 261 canonical boundaries match; user confirmed the flash fixed in human-mechanics-04. |
| Animate targeting letterboxes at presentation FPS | Shipwright; `shrink_window.c`, frame/view drawing and replay | Built. 57 native command checks pass; canonical Player20/render120 crawl matches 261 saved boundaries. World-owned values and duration are unchanged. Human motion review pending. |
| Interpolate CPU-skinned horse animation vertices | Shipwright; `z_skin.c`, `z_skin_awb.c`, `frame_interpolation.*`, replay call site | Standalone rendering fix, independent of Player-rate selection. Existing interpolation covers matrices but misses CPU-deformed skin vertices. Separate presentation vertices use the world interpolation fraction; canonical skin buffers and20-Hz callbacks remain intact.14 helper checks passed; mounted Player20/120 each generate intermediate meshes and match31 prior boundaries (`epona-skin-01`). Human motion review pending. No PR submitted. |

## Candidates needing further separation

- **DXGI pacing:** continuous deadlines, avoiding a second limiter when VSync
  already paces, and avoiding needless refresh-estimator work are in
  `1398809a` / `8dc5c763`. Current activation is tied to the one-frame-latency
  Player path. They need an independent renderer setting/contract and ordinary
  interpolation checks; they are not ready for a wholesale cherry-pick.
- **Visibility readback placement:** `5ad2ca80` supplies the generic pre-Present
  callback, but the Shipwright call site currently selects it for Player120.
  Timing and occlusion-source changes are documented in
  [TIMING_SEMANTICS.md](TIMING_SEMANTICS.md). The accepted live result does not by
  itself qualify a global renderer change.

Movement, crawl clearance at high Player rates, vine timing, NPC contact bridging,
high-rate animation, horse/rider cadence and input scheduling remain mod work.
Do not include them in these PRs. No upstream PR is created by this checklist.

Detailed existing evidence: [PLAYER_PACING.md](PLAYER_PACING.md),
[TESTING.md](TESTING.md), and [WORLD_INTERACTIONS.md](WORLD_INTERACTIONS.md).
