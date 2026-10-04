# Pass 4A: Player temporal core and pre-pilot controls

Status: in progress; no acceptance claimed. Starting source
`f8fe6a7fd005a508b2626de2685e8df41efafa59`; dependency pins unchanged from PASS3D.

The authorized scope is native temporal vocabulary, opportunity and lifecycle
contracts, bounded Player unit inventory, canonical pause/single-step and diagnostic
inspection. PASS3C/PASS3D own the phase order and admitted profile. No gameplay
arithmetic, cadence, animation, camera or collision is retimed. Effective Player
and world rates remain 20 Hz. Contact records are schema only, with no active bridge.

Implementation starts with standalone native tests of the same header used by the
engine. Canonical metadata stays in a separate diagnostic stream so the preserved
semantic snapshots/full phase traces remain exact oracles. QA gates the complete
transaction before input and retains the last presented image while paused; it
does not use the legacy collision-suppressing frame advance.

Validation will cover all ten Player fixtures with three repetitions, strict
phase/contact analysis and CPU purity, focused original/HUD regressions, native
core and Python tests, CLI/controls/startup, canonical single-step equivalence and
representative presentation independence. No historical large matrix is restored.

Next boundary, after a clean pushed handoff: separately authorized Player cadence,
queue ownership and input/camera/contact integration. That work does not start here.

## Native foundation checkpoint

`PlayerTemporalCore.hpp` implements distinct rate/time/duration/context types,
canonical-only capability, per-source opportunity cursors, integer duration and
signed sixth-remainder accounting, unwrapped authored-frame interval crossings,
animation marker consumption, scene/Player/scope generations, attack/window
identity, ordered input metadata and pointer-free future contact records.
No engine gameplay call uses a scaling primitive. `CanonicalControl` gates whole
transactions and refuses duplicate grants/commits. World guards require six-quanta
aligned intervals and a distinct source opportunity.

The standalone MSVC gate passed all 95 checks at
`build/pass4a-01/temporal-unit-02`. The first receipt incorrectly expected 96
despite all 95 checks passing; it remains retained as test-infrastructure failure.
No native fault occurred. Engine integration and full acceptance remain pending.
