# Focused player-test itinerary

Use the [current launcher](../../scripts/native-simulation/Play-Player120.cmd),
Player120 and the already accepted rendering settings. Keep normal playthrough
saves separate from disposable test saves. This route follows
[the48-area source review](COMPATIBILITY_REVIEW.md); it is not a requirement to
repeat everything already accepted or visit every room/enemy.

**Before asking for these sessions:** the agent must repair and run a focused
engine check for the known hookshot, freeze/electric, spell, fishing/gallery,
carry-delay and remaining movement issues listed in the review. Those are agent
work, not mystery bugs delegated to the player. Sessions involving an unresolved
known issue are marked below and should wait for its fix. Other sessions can
happen naturally during a playthrough. No need to complete the route in one sitting.

| Session | What to do once | What to watch for | Review IDs |
| --- | --- | --- | --- |
|1. Ordinary child route|Run/roll around Kokiri, talk/read/close, pick up and throw a pot, crawl both ways, climb/drop from a ledge.|One press per action; no reopened dialogue, missed offer, premature cancel or attachment jitter. Already accepted ordinary motion needs only a quick sanity check.|11–17,35–37,41|
|2. First actual combat|Fight a Deku Baba or Skulltula, then one moving/shielded enemy when encountered. Try ordinary/targeted/crouch/jump attacks, block, take one hit and recover.|One hit response per attack, sane recovery/invulnerability, working block/bounce, no body vibration or lost lock-on. No need to compare every enemy at20.|21–26,28–29|
|3. Dungeon coupling|Push then pull a block; release midway; stand/turn/jump off an actual lift or rotating platform; operate a floor switch and locked door.|No accumulating push force, sliding/drifting attachment, vibrating feet, early door release or duplicate key use. **After platform/block engine checks.**|19–20,42,46|
|4. Ranged objects|Use bow/slingshot on a switch and enemy; test the reported grotto beehive with the exact weapon noted; throw/recall a boomerang.|Stable hand/projectile alignment, one ammo use, sensible contact and return. A hive can legitimately shake rather than break depending on weapon.|05,31,33,46|
|5. Hookshot and carrying|Hookshot a fixed target near/far, release and land; hook a moving target if convenient. Lift/throw a heavy object and carry a bomb or cucco.|Travel duration, latch/release, control return and prop alignment. **After hookshot/carry fixes.**|32,36|
|6. Water and terrain|Swim across a current, reverse direction, dive/let go/resurface, equip Iron Boots underwater; later cross ice/steep slope and a Hover gap.|Correct elapsed timing with finer control, no sudden speed/turn changes, float/drift or stuck transition. **After remaining swim/slide fixes.**|18,40|
|7. Bottles/trade/magic|Catch and release one creature, drink a bottle, plant a bean, offer one trade item. Cast Din/Nayru/Farore when available.|One inventory/magic change, correct actor response and wait, camera/control return; no repeated sound or message. **Spells wait for their fix.**|35,37–38,41|
|8. Adverse states|Encounter real ice/electric damage and one grabbing enemy during normal play; test fairy revival when convenient.|Original-duration passive damage/recovery; fresh escape inputs still respond; no multiplied health loss. **After frozen/electric action fixes.**|27–30|
|9. Horse and minigames|Brief mounted ride with mount/dismount/carrot/fence; later gallery finish and fishing cast/reel/catch.|No missed A, rider/horse separation, premature completion or fast animation. Accepted ordinary ride needs no long repeat. **Gallery/fishing wait for fixes.**|06,39|
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
