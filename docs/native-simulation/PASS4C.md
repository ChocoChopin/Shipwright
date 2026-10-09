# Pass 4C: bounded Player/sign contact bridge

Implementation in progress from `2e51b2dccf296fb939361d102164ff3fde342c73`.
No acceptance or human-play claim yet. Canonical Player20 retains legacy contact.

## Implementation contract

- Capture registered authoritative sign cylinders after world actor work, before
  committing the first late Player pose; reuse held geometry between world ticks.
- Scene, world and monotonic actor generations bind copied geometry and masks.
  Actor addresses are live lookup keys only; events/proxies contain no pointers.
- Changed active sword sweeps alone produce candidates, in quad/triangle order.
  No high-rate insertion into global collision lists.
- Reserve by scene/Player/scope, attack epoch, authored opportunity, target
  generation and element group. Retain reservations through the attack after
  delivery to prevent subsequent observations from delivering again.
- Deliver due events after legacy AT at the next world collision boundary,
  before Player and target updates. Both sides respond at world20. Target cooldown,
  piece spawning, RNG and effects keep the existing actor update slot.
- Sign cut animation comes from the producing event only for bridged hits;
  canonical contacts continue reading live Player animation.
- Scope/scene/actor invalidation rejects pending events explicitly. Completed
  delivery data remains valid through the synchronous world actor traversal.
- Exclude sign body overlap at current/predicted Player positions, unknown shapes,
  actors, hostile contacts, alternate melee, dynamic surfaces and reciprocal OC.

## Planned focused gates

One run each: sign60/120, Z-sign60/120, repeated observation, target invalidation;
canonical slash/sign/Z-sign/combo against retained checkpoint hashes with direct
presentation purity; representative original/HUD; temporal units, online harness,
CLI, graceful mismatch and ordinary startup. No broad matrix or routine repeats.
Compact receipts and bounded failure rings; target far below 500 MB diagnostics.

Interactive selection and final acceptance remain pending automated qualification.

## Preliminary implementation evidence

- Build `1366b2037a83f4ea9638d2a4438abdfe2f691668a58c2045f9f4667466206686`: initial bridge engine compiled.
- Native core: 237 checks; Python: 151 tests.
- `build/pass4c-02/sign120`: sign120 passes, one horizontal cut, 210 Player steps / 35 target updates, 11,462 output bytes.
- `build/pass4c-02/contact`: sign60, Z-sign120/60 and repeated-observation engine cases pass. Z cuts are vertical; ordinary cuts horizontal. Invalidation initially failed an erroneous full-rate requirement.
- `build/pass4c-03/invalidation`: engine passes rejection/zero-cut assertions; runner initially assumed uninterrupted high-rate purity coverage. Its compact receipt accounting is corrected in source, pending final rebuild.
- Initial `build/pass4c-01/sign120` rejects nearby OC at the separate targeting fixture location. The final recipe uses the already-qualified ready-sword location; no unknown collider was admitted.
- No native crash occurred in these runs. Failures and bounded dumps retained.

Current UI: Settings / Graphics, Experimental Player Hz. Default Original20; 60/120 require Vsync off and the existing narrow profile. Selecting Original then a higher rate rearms after fallback. Live status distinguishes requested/effective Player, world20 and rendering FPS. Final build and startup acceptance pending.
