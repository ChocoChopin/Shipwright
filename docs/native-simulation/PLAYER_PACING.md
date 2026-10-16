# Player120 pacing follow-up

## Current qualification

The human pilot initially accepted the deferred-visibility build's VSync-on
gameplay as subjectively flawless, then reported recurring dips. Its initial
foreground 60-second capture recorded 36 missed refreshes, compared with 197 in
the previous queue-fix capture. These were ordinary live sessions, not identical
scripted routes. A later custom-equipment lookup fix has a foreground capture with
two missed refreshes in 7,195 frames; the human reports less frequent dips, not
their elimination. Strict zero-miss display qualification remains unmet.
The high-rate path keeps maximum frame latency at one. Canonical rendering keeps
the original two-frame setting. Therefore turning the Player mod off also changes
the permitted presentation queue; that comparison alone does not isolate GPU cost.

## Measured defects and corrections

- Native DXGI windows pumped SDL events but only removed controller hotplug
  events. Controller motion notifications accumulated; selective hotplug queries
  repeatedly scanned the growing list. A late CPU capture attributed about half
  the main thread samples to that scan. The native window now discards only the
  unused controller/joystick notifications, preserving hotplug and other events.
  Controller state is still polled fresh; no smoothing or extra input sample.
  The SDL regression has 22 checks, including a 20,000-event backlog, 7,200 drain
  cycles, retained events and unchanged current axis/button state. Live GUI/setup
  cost fell from about 2.5 ms to 0.11-0.22 ms per frame.
- DX11 previously discarded the vertex upload allocation on every draw. A captured
  hitch included a 10.4-ms Map stall. Uploads now append with NO_OVERWRITE and use
  DISCARD on wrap/frame reset, retaining the same 96-KiB allocation. Approximately
  344 draw uploads use five discards per frame instead of 344. This queues no image.
- High-rate pacing uses continuous deadlines and bypasses the legacy refresh-drop
  estimator; matching-rate VSync does not also wait on a software FPS deadline.
- Compiled shader bytecode is cached with bounded size and validated source/options
  and payload hashes. Measured first-use compilation is reported separately from
  steady pacing, with raw timing retained.

## Remaining refresh misses

`build/human-queue-fix-01/presentmon-foreground.csv` captured 6,999 foreground
VSync-on frames. Presentation interval averaged 8.568 ms; 197 displayed intervals
exceeded 12.5 ms. GPU completion was after the intended refresh in 196 of those
197 misses. Misses clustered at the six-frame world boundary. A separate blocking
stack capture found the environment depth readback blocking up to 1.3275 ms.

The checkpointed visibility change moves only that query after the first high-rate
frame, before the next Player input poll. Its next-world lens-flare/glow result
uses the current first frame's depth. Canonical20 and VSync-off retain the original
query position. See TIMING_SEMANTICS.md. The candidate capture contains 7,161 frames,
all independent-flip, SyncInterval1 and no tearing: displayed p99 8.3981 ms, maximum
16.7223 ms, 36 intervals above 12.5 ms. CPU submission p99 is 10.6222 ms.
`build/human-visibility-fix-01/` retains the capture and session receipt. Executable
SHA-256: `aaa01cd9f0cbb444c715b46ffc1b737d6fd4cad1eba4e9c70a24961360e6b6d0`.
The rebuilt canonical idle matches all 41 retained reference boundaries in
`build/player120-visibility-canonical/`. Startup and human gameplay succeeded.
The initial C-header declaration build failure was corrected before this build;
no new native crash occurred.

The foreground VSync-off capture submitted 2,396 frames with an 8.3333-ms average,
8.398-ms p99 and 10.260-ms maximum interval. That is software submission evidence;
it is not a claim that every physical scanout interval or input latency was measured.

## Optional custom-equipment lookup fix (2026-10-15)

The subsequent experiment moving the visibility query before the queue wait was
withdrawn. Its patches and failed live captures remain in
`build/human-visibility-overlap-01/`. Returning to the checkpointed readback slot
still exhibited dips (`build/human-visibility-restored-01/`). No experimental
readback-order change or dependency change is included in the equipment fix.

A 10-second blocking-stack trace of the restored path caught a 33.5665-ms main
thread wait in `LoadCustomGfx -> ResourceGetIsCustomByName -> LoadResource`, called
by the Player limb hook. `GetResourceIsCustom` synchronously loads the resource;
the asynchronous loader's fast cache path accepts only a non-null resource, so an
absent optional model repeatedly queues work and waits. This hook and loader
behavior predate the native Player experiment; increased pose cadence amplifies
the issue. Original evidence: `build/human-visibility-restored-waits-01/`.

`customequipment.cpp::IsCustomEquipmentResource` now checks the existing archive
extension index before querying a resource's custom type. `LoadCustomGfx`, the
custom fallback chain, and the optional-equipment pose-admission checks share it.
Present alternate files preserve their precedence, existing original files still
receive their custom-type query, and `.meta` aliases are included in the existing
index. This adds no retained miss cache and leaves real model loading, equipment
selection and asset-change behavior in their existing owners. No gameplay timing,
input polling, frame queue limit or world cadence changes.

Validation of this fix, once per case:

- Build succeeded. Executable SHA-256:
  `e132ee2d0757170a6792d2aafdc2d16ddd28698f8bbec71313070239f095c2ed`.
  Source is `90059f219cd69bb3317af0f878cf1aeb6bf64334` plus the equipment guard;
  libultraship remains `8dc5c7639bd266ab54f552c27190100495924387`.
- Canonical idle matches all 41 retained boundaries:
  `build/player120-custom-equipment-canonical/`.
- The compact 60-second Player120 performance case produced 7,200 fresh poses,
  zero dropped submissions, all eight first-due input responses, 119.82 average submission FPS and no shader
  compilations. It still fails strict pacing: 11 intervals over 12.5 ms, maximum
  18.0381 ms, p99 10.7 ms. Foreground was lost after 19.094 seconds of the process;
  this is not a sustained foreground qualification. Receipt and focus transitions
  are retained in `build/player120-custom-equipment-performance/`.
- A 10-second follow-up scheduling trace found zero custom-equipment waits among
  571 main-thread waits; the maximum depth-readback wait was 0.5467 ms:
  `build/human-custom-equipment-waits-01/`.
- Ordinary startup directly into child Kokiri Forest succeeded, with real human
  controls. `build/human-custom-equipment-fix-01/presentmon-foreground.csv` records
  a continuously foreground 60-second window: 7,195 independent-flip frames,
  VSync1, two displayed intervals above 12.5 ms, displayed p99 8.3751 ms and maximum
  16.678 ms. Both missed frames finished on the GPU about 0.15-0.25 ms after the
  estimated next refresh. The preceding background/compositor capture is retained
  separately and is not foreground acceptance evidence.
- No broad matrix, repeated fixture corpus, physical input-to-scanout test or
  custom-model mod-pack campaign was run. Canonical behavior and the observed
  absent-model case are verified; preservation of custom-model resolution is
  source-reviewed, not claimed as a full mod compatibility test. No native crash.

The remaining misses require investigation; do not equate removal of the captured
resource wait with complete pacing repair. Frame latency remains one at Player120.
A subsequent 16-MiB ring capture, stopped on an actual foreground hitch, found a
4.1656-ms depth-readback wait and zero equipment waits. The corresponding frame
reported 4.98 ms outside rendering and a 16.7-ms completion interval. Retained in
`build/human-custom-equipment-hitch-01/`; this motivates separating GPU query
submission from CPU result collection, rather than another whole-callback move.

## Evidence and scope

Keep original failure traces and compact summaries in the ignored build directories.
`build/derived-trace-cache-cleanup-20261015.json` records deletion of 113,754,443
bytes of regenerable ETLX indexes; their original ETLs remain. No successful
per-substep JSON campaign was added. Existing focused checks before the current
visibility candidate: native CLI48, animation/input53, canonical idle41 exact
boundaries, shader cache9, vertex upload7, texture24 and replay tooling32.
Those earlier results do not establish zero missed refreshes in the current build.
