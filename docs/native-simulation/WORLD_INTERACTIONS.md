# Player/world interaction implementation

The user accepted pacing on 2026-10-15 and authorized ordinary world collision
and NPC interaction. The accepted source checkpoint is `7cd77f92d`, with
libultraship `5ad2ca80`. Preserve input cadence, movement and presentation pacing.

## First implementation slice

- Accept a talk-button edge at the Player step using the most recent world-owned
  offer. NPC offer production and message progression remain world20. Retain
  message-owned button edges until the world consumes them; do not defer rolling
  or other ordinary Player input to world20.
- Register the Player body and shield with the existing world collision pass.
  Geometry continues to follow Player updates; collision response, damage and
  reciprocal object displacement execute once per world opportunity.
- Retain every produced sword sweep within the current 50-ms transaction and
  deliver it through the engine's existing collider-shape dispatch at the next
  collision opportunity. Keep the already-qualified sign adapter separate.
  Preserve source damage data through target consumption, avoid duplicate hits
  from substeps, and cancel pending work on Player/scene invalidation.
- Keep canonical20 arithmetic and ordering unchanged. No world actor, NPC, AI,
  global collision pass or dynamic-platform update runs six times faster.

## Focused acceptance

Use compact online receipts: a real NPC conversation with an intermediate A edge
and message advancement; body blocking; a non-sign cuttable object; ordinary
incoming damage/shield coverage; the existing sign and next-frame fixtures;
canonical idle/slash regression. Add cheap native shape/lifetime checks where
they directly cover the generic sword path. Default one run per case. No pacing
campaign, broad configuration matrix or successful per-substep trace archive.

General collider participation is not proof of every enemy-specific attack,
moving-platform attachment, climbing, swimming, projectile or item action.
Record the tested interactions and remaining gaps separately at handoff.

## Implemented ownership

`Player_ActionHandler_Talk` accepts an explicit edge against the latest NPC offer
on a Player step. `PlayerTemporal_WorldInputButtons` and the existing PadMgr edge
accumulator retain dialogue-owned buttons until the message dispatcher consumes
them. Ordinary movement and action input still reaches the next Player step.

`PlayerContact_RegisterBody` and `PlayerContact_RegisterShield` update membership
without adding duplicate entries or repeatedly resetting collision flags. Their
geometry follows Player cadence; the original world collision pass owns overlap
displacement, incoming damage and shield responses. Inactive membership is removed.

`PlayerContact::Detect` captures produced sword sweeps, including damage fields.
`ConsumeWorldSweeps` uses the original four AC shape dispatchers after world AT,
before damage accumulation and actor updates. It filters stale actors and already
delivered attack/opportunity/target-collider identities. AC hit source pointers
refer to stable per-target storage, retained through culling until replacement or
target destruction. Scene reset clears that storage. Player invalidation cancels
pending sweeps. The existing sign adapter retains its producing-animation data;
the nearest sign distance and reservation identity arbitrate competing objects.
This does not add intermediate world collision passes or retime world actors.

## Validation on 2026-10-15

Incremental Release builds succeeded. Final executable SHA256:
`1a8221aa455ced19ad6bb9469ab9329ec9b6c196ca089f653319202f7085ddc7`.
Built from `7cd77f92dc83a66f4a696a7558a8710ae95305b2` plus this interaction
change. Dependencies remain libultraship
`5ad2ca80a138df8bcded4edde0b674fd39663a34` and Torch
`2ab12fe9660aec04e02ee89fe81baed304a1a1d6`. The local corpus receipts contain
the exact tracked diff, fixture, executable and asset hashes; assets are not committed.

| Focused gate | Result | Local evidence below `build/` |
| --- | --- | --- |
| Native shape/lifetime/response checks | 33/33; includes both sign/generic nearest-contact directions, reciprocal mass, incoming damage and shield block | `player-world-03/contacts/player-world-contact-checks` |
| Real grass at Player120 | One bridged contact, one cut, target destroyed; 45 world ticks | `player-world-03/contacts/player-world-grass-120` |
| Existing Player120 sign | Pass after final nearest-contact correction | `player-world-03/contacts/player-contact-sign-120` |
| Asset-free animation/input ownership | 53/53 | `player-world-03/input` |
| Real Kokiri conversation at Player120 | A at quantum61 starts Player response at61; textbox opens62; all three subsequent dialogue edges reach world; NPC updates100 times in100 world ticks | `player-world-02/focused/player-world-npc-120` |
| Player120 body contact | Nine observed overlaps; NPC updates55 times in55 world ticks | `player-world-01/objects/player-world-body-120` |
| Next-frame movement | All four sampled input changes consumed, moved pose and rendered at their first due submission | `player-world-02/regression/player-next-frame-motion-120` |
| Canonical idle / slash | Exact 41 / 71 common-boundary comparisons against retained references | `player-world-02/canonical-idle`, `player-world-02/canonical-slash` |

The final build changed generic-contact nearest arbitration and added its native
checks. Only contact checks, sign and grass were rerun for that change. Earlier
NPC, body, movement and canonical results cover unchanged relevant paths; they
are not represented as executions of the final binary. Each case ran once per
relevant implementation change. No configuration matrix or pacing campaign ran.
The existing general Python/tooling, HUD corpus and exhaustive enemy/item suites
were not rerun: this pass did not change that tooling or HUD logic.

The initial NPC run exited gracefully with a test-observer failure, retained at
`player-world-01/npc`: it incorrectly equated textbox opening with the first
Player response. The recorded Player action accepted A at61 and the textbox
opened at62. The observer now records both separately and still requires Player
acceptance at61. No gameplay timing was changed to satisfy that observation.
No new native crash occurred. All successful evidence is compact online receipts
and ordinary runtime logs; no new full per-substep snapshot/trace archive.
Total retained pass evidence is about 40 MiB, including the failed observer run.
`player-world-03/retention.json` records the retained directories and empty
deletion manifest; the corpus runner released its staged runtime archives.

Ordinary startup of the final binary succeeded with the configured child Kokiri
scene selector, Player120 and VSync enabled. The menu selection was sent once;
subsequent live Player animation/frame counters confirm gameplay is running,
with the game foregrounded. Local session receipt:
`human-world-interactions-01/session.json`. It is left open for the user. The
automated interaction results above are not human acceptance of this new build.
Use `.tmp/Play-Shipwright.cmd` for later launches with the correct asset directory;
the direct executable remains `x64/Release/soh.exe`.

## Next gameplay work

Exercise this build in ordinary Kokiri gameplay, including NPC conversation,
blocking against actors and grass/sign cuts. The native incoming-damage/shield
checks establish collision delivery, not every enemy's attack reaction. Next
qualify one real enemy attack and its damage/recoil/invulnerability timing, then
item/door interactions and moving-platform coupling as separate focused cases.
Keep accepted pacing and next-frame Player controls intact. No claim is made
that every enemy, projectile, item, climbing or swimming action is converted.
