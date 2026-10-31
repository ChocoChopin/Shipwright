# Focused player-test itinerary

## Direct-boot scenario menu

Double-click [Playtest-Player120.cmd](../../scripts/native-simulation/Playtest-Player120.cmd).
Choose a number; the console lists exactly what to make Link do and what to watch
for, then opens the current `x64/Release/soh.exe`. No build, file-select sequence,
recorded input or trace capture. Each launch starts a fresh maxed temporary debug
save with the selected age/items; normal `build/playthrough/Save` is untouched.
Close that scenario's game before launching it again. Settings persist inside
`build/playtests/<name>`; assets are hardlinks, not copies. It does not write or
load a normal save slot. Age/equipment overrides apply only to opted-in debug boot.

From the repository root:

```powershell
python scripts/native-simulation/play_scenario.py --list
python scripts/native-simulation/play_scenario.py horse
python scripts/native-simulation/play_scenario.py water --rate 20
```

`--rate 20` changes Player cadence; in the Stalfos experiment it also restores
the enemy's original cadence. Rendering remains120 for a useful A/B.
`--prepare-only` prints the steps and build identity without launching.

| Name | Prepared setup | Startup check |
| --- | --- | --- |
|child-controls|Kokiri; child; slingshot/bombs/Bunny Hood; known open-ground position|Passed ordinary boot to Kokiri, exit0|
|child-items|Same Kokiri position; boomerang/stick/nut|Config prepared; not separately booted|
|adult-combat|Forest Temple entrance; adult; bow/bombs/longshot; real entrance Wolfos|Config prepared; not separately booted; enemies remain20|
|dungeon|Fire Temple entry room; adult; hammer/longshot/bombs; known position|Config prepared; position used by focused fixtures|
|ranged|Lake Hylia shore; adult; bow/fire arrow/longshot|Config prepared; known fixture position|
|water|Lake Hylia shore; child; empty bottle/boomerang/bean|Config prepared; known fixture position|
|water-adult|Same shore; adult; bottle/longshot/ocarina; boots in inventory|Config prepared; known fixture position|
|spells|Fire Temple entry room; Din/Nayru/Farore on C buttons|Config prepared; known fixture position|
|ice|Ice Cavern authored entrance; adult; empty bottle/bow/longshot|Config prepared; not separately booted|
|slopes|DMT authored entrance; child; bombs/Bunny Hood/ocarina|Config prepared; uphill force checked; extended artificial slope correctly enters a real void transition; no whole-terrain equivalence claim|
|horse|Hyrule Field; adult; existing already-mounted setup enabled|Passed boot to Field, exit0; no new human ride acceptance|
|fishing|Fishing Pond authored entrance; adult with rupees; obtain rod from owner|Passed boot to pond, exit0; actual minigame not played|
|gallery|Gallery authored entrance; child with slingshot and rupees|Config prepared; not separately booted|
|stalfos|Forest Temple upper bow-room ring; adult sword/shield; one real type1 enemy|Engine duel/animation/contact checks in TESTING.md; human feel pending|

The authored-entrance option uses the scene's real spawn/room rather than guessed
coordinates. The menu now has14 configurations; four config tests check
rate-only comparison, preferences, spawn selection and mounted-start isolation.
Three12-second ordinary startup checks pass on SHA256
`54f23ace408c9ad9e72a74a8a4e8d28bb019f24689a92fbab2a3f2ffd783a845`.

### First enemy comparison

[Play-Stalfos120.cmd](../../scripts/native-simulation/Play-Stalfos120.cmd) goes
straight to the upper Forest Temple duel. Lock on, circle the enemy, block a
slash, dodge the jumping attack, then counterattack. Look for smooth enemy
feet/sword/shield motion, normal attack duration, reliable damage/blocking and
no body-contact vibration. Compare with `play_scenario.py stalfos --rate 20`.
The enemy's tactical choices stay20 in both modes. Other enemies and variants
are not converted by this trial. The prepared game stays closed until requested.
Receipts: `build/motion-effects-20261030/scenario-startup/`. Startup is not visual
or gameplay acceptance. Fishing/gallery still use their real owner to start the
minigame. Progression/saving should also be tested through the ordinary launcher;
a maxed disposable save cannot validate normal reward progression.

The high-rate Stalfos arena is deliberately absent until the earlier repair list
is complete. These scenarios make the existing itinerary easy to launch; they do
not silently mark its outstanding mechanics as tested.

Use the [current launcher](../../scripts/native-simulation/Play-Player120.cmd),
Player120 and the already accepted rendering settings. Keep normal playthrough
saves separate from disposable test saves. This route follows
[the48-area source review](COMPATIBILITY_REVIEW.md); it is not a requirement to
repeat everything already accepted or visit every room/enemy.

**Before asking for these sessions:** the agent must repair and run a focused
engine check for the remaining movement/effect issues listed in the review.
Fishing/gallery and carry-delay checks now pass. Those are agent
work, not mystery bugs delegated to the player. Sessions involving an unresolved
known issue are marked below and should wait for its fix. Other sessions can
happen naturally during a playthrough. No need to complete the route in one sitting.

| Session | What to do once | What to watch for | Review IDs |
| --- | --- | --- | --- |
|1. Ordinary child route|Run/roll around Kokiri, talk/read/close, pick up and throw a pot, crawl both ways, climb/drop from a ledge.|One press per action; no reopened dialogue, missed offer, premature cancel or attachment jitter. Already accepted ordinary motion needs only a quick sanity check.|11–17,35–37,41|
|2. First actual combat|Fight a Deku Baba or Skulltula, then one moving/shielded enemy when encountered. Try ordinary/targeted/crouch/jump attacks, block, take one hit and recover.|One hit response per attack, sane recovery/invulnerability, working block/bounce, no body vibration or lost lock-on. No need to compare every enemy at20.|21–26,28–29|
|3. Dungeon coupling|Push then pull a block; release midway; stand/turn/jump off an actual lift or rotating platform; operate a floor switch and locked door.|No accumulating push force, sliding/drifting attachment, vibrating feet, early door release or duplicate key use. Bounded Armos/lift engine checks pass; this session checks other variants and visible alignment.|19–20,42,46|
|4. Ranged objects|Use bow/slingshot on a switch and enemy; test the reported grotto beehive with the exact weapon noted; throw/recall a boomerang.|Stable hand/projectile alignment, one ammo use, sensible contact and return. A hive can legitimately shake rather than break depending on weapon.|05,31,33,46|
|5. Hookshot and carrying|Hookshot a fixed target near/far, release and land; hook a moving target if convenient. Lift/throw a heavy object and carry a bomb or cucco.|Travel duration, latch/release, control return and prop alignment. Hookshot timing is engine-checked; Heavy20/120 controlled pickup/throw now passes; other variants remain checks.|32,36|
|6. Water and terrain|Swim across a current, reverse direction, dive/let go/resurface, equip Iron Boots underwater; later cross ice/steep slope and a Hover gap.|Correct elapsed timing with finer control, no sudden speed/turn changes, float/drift or stuck transition. Swim and local slide clocks now pass; defer long slippery-slope paths while the known trajectory divergence is investigated.|18,40|
|7. Bottles/trade/magic|Catch and release one creature, drink a bottle, plant a bean, offer one trade item. Cast Din/Nayru/Farore when available.|One inventory/magic change, correct actor response and wait, camera/control return; no repeated sound or message. Din/Farore engine checks pass; Nayru and return warp remain checks.|35,37–38,41|
|8. Adverse states|Encounter real ice/electric damage and one grabbing enemy during normal play; test fairy revival when convenient.|Original-duration passive damage/recovery; fresh escape inputs still respond; no multiplied health loss. Passive waits/damage and analog freeze release are engine-checked; real enemy hits/grabs remain checks.|27–30|
|9. Horse and minigames|Brief mounted ride with mount/dismount/carrot/fence; later gallery finish and fishing cast/reel/catch.|No missed A, rider/horse separation, premature completion or fast animation. Accepted ordinary ride needs no long repeat. Bounded clock checks pass; full minigame progression is still untested.|06,39|
|10. Progression and busy scene|Open chest, play/cancel song, change room, save/reload; watch a normal cutscene and one busy encounter.|No stuck control, duplicate rewards, stale held objects, camera flash or recurring warmed hitch. Equip a different item/age naturally rather than testing every combination.|02–04,07–10,42–48|

For a suspected timing bug, compare only that action in Original20, with rendering
unchanged. Note scene, child/adult, equipment, action and whether120 differs.
A short description of the transition is more useful than another huge trace.
For motion defects, use visible motion first; capture a short bounded clip only
when needed to isolate the frame transition. Do not request another capture after
the user has already confirmed the fix.

Record each session as **accepted**, **defect with a reproduction**, or **not run**.
One successful example qualifies that example, not all variants. The agent owns
the narrow reproducer/fix/online check and updated build. A new native crash stops
the active campaign with logs preserved; no deliberate reproduction without the
user's direction. Successful runs need compact receipts, not archived full traces.
