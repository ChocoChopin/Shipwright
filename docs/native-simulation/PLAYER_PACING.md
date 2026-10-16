# Player120 pacing follow-up

## Current qualification

The human pilot accepted the deferred-visibility build's VSync-on gameplay as
subjectively flawless. Its foreground 60-second capture still recorded 36 missed
refreshes, compared with 197 in the previous queue-fix capture. These were ordinary
live sessions, not identical scripted routes. Preserve both findings: strong live
acceptance, with a strict zero-miss display criterion still unmet.
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

The current candidate moves only that visibility query after the first high-rate
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

## Evidence and scope

Keep original failure traces and compact summaries in the ignored build directories.
`build/derived-trace-cache-cleanup-20261015.json` records deletion of 113,754,443
bytes of regenerable ETLX indexes; their original ETLs remain. No successful
per-substep JSON campaign was added. Existing focused checks before the current
visibility candidate: native CLI48, animation/input53, canonical idle41 exact
boundaries, shader cache9, vertex upload7, texture24 and replay tooling32.
Those earlier results do not establish zero missed refreshes in the current build.
