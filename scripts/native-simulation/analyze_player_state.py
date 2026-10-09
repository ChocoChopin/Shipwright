"""Read-only canonical Player pose/contact ordering and exercised-coverage gate.

Uses the optional semantic records, never engine pointers or proprietary assets.
An endpoint golden alone cannot prove which transaction owned a contact.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from pathlib import Path
import sys

import run_corpus as replay

EXPECTED = frozenset('player-' + name for name in (
    'idle', 'slash', 'combo', 'shield', 'z-slash', 'turn-attack', 'move-attack',
    'static-wall', 'sign', 'z-sign'))
COLLISION_PHASES = ('collision.begin', 'collision.after_at', 'collision.after_oc',
                    'collision.after_damage', 'collision.cleared')


def require(condition, message):
    if not condition:
        raise replay.ReplayError(message)


def equal(a, b, message):
    require(not replay.first_differences(a, b, 1), message)


def check_tick(tick, previous, endpoint, events):
    """Check the actual previous-pose seam and phase order, not just final hashes."""
    samples = [e for e in events if e['kind'] == 'player_sample']
    player_id = previous['player']['identity']
    by_site = defaultdict(list)
    for e in samples:
        if e['site'].startswith('actor.update') and e['actor'] != player_id:
            continue
        by_site[e['site']].append(e)
    sites = (*COLLISION_PHASES, 'actor.update.begin', 'actor.update.end', 'pose.begin', 'pose.end')
    for site in sites:
        require(len(by_site[site]) == 1, f'tick {tick}: expected exactly one {site}')
    ordered = [by_site[site][0] for site in sites]
    require([e['sequence'] for e in ordered] == sorted(e['sequence'] for e in ordered),
            f'tick {tick}: canonical Player/collision/pose order')
    old = ordered[0]
    equal(old['player']['weapon_geometry'], previous['player']['weapon_geometry'],
          f'tick {tick}: collision must consume prior draw weapon history')
    equal(old['detail']['sword_quads'], previous['player_detail']['sword_quads'],
          f'tick {tick}: registered sword quads changed before collision')
    equal(by_site['actor.update.begin'][0]['player']['body_parts'], previous['player']['body_parts'],
          f'tick {tick}: Player update must receive prior body pose')
    pose_begin, pose_end = by_site['pose.begin'][0], by_site['pose.end'][0]
    equal(pose_end['detail']['pose_generation'], pose_begin['detail']['pose_generation'] + 1,
          f'tick {tick}: one pose generation')
    equal(pose_end['player']['weapon_geometry'], endpoint['player']['weapon_geometry'],
          f'tick {tick}: endpoint weapon history differs from completed pose')
    equal(pose_end['detail']['sword_quads'], endpoint['player_detail']['sword_quads'],
          f'tick {tick}: late sword registration differs from endpoint')
    for quad in pose_end['detail']['sword_quads'] + [pose_end['detail']['shield_quad']]:
        for category in ('at', 'ac', 'oc'):
            require(len(quad['registered_' + category]) <= 1,
                    f'tick {tick}: duplicate quad registration in {category}')
    registrations = [e for e in events if e['kind'] == 'player_registration']
    for e in registrations:
        require(e['index'] >= 0, f'tick {tick}: successful registration index')
        if e['collider']['shape'] == 3:  # COLSHAPE_QUAD
            require(pose_begin['sequence'] < e['sequence'] < pose_end['sequence'],
                    f'tick {tick}: weapon/shield registration escaped late pose')
    contacts = [e for e in events if e['kind'] == 'player_contact']
    for e in contacts:
        require(old['sequence'] < e['sequence'] < by_site['collision.after_at'][0]['sequence'],
                f'tick {tick}: contact outside original AT collision slot')
    return {'joint_mutation': pose_begin['detail']['joints'] != pose_end['detail']['joints'],
            'combo_draw_mutation': pose_begin['detail']['combo_count'] != pose_end['detail']['combo_count'],
            'registrations': len(registrations), 'contacts': len(contacts)}


def observe(output, fixture):
    manifest, snapshots = replay.load_run(output)
    replay.validate_requested_fixture(manifest, fixture)
    events = replay.load_trace(output, fixture['ticks'])
    measured = [e for e in events if e.get('measuring')]
    by_tick = defaultdict(list)
    for e in measured:
        by_tick[e['tick']].append(e)
    counts = Counter()
    for tick in range(fixture['ticks']):
        counts.update(check_tick(tick, snapshots[tick], snapshots[tick + 1], by_tick[tick]))
    contacts = [e for e in measured if e['kind'] == 'player_contact']
    transitions = []
    for before, after in zip(snapshots, snapshots[1:]):
        if (before['player']['action'], before['player']['melee_state'], before['player']['melee_animation']) != (
                after['player']['action'], after['player']['melee_state'], after['player']['melee_animation']):
            transitions.append({'tick': after['tick'], 'action': after['player']['action'],
                                'melee_state': after['player']['melee_state'],
                                'melee_animation': after['player']['melee_animation']})
    target_responses = []
    for tick, rows in by_tick.items():
        begins = {e['actor']: e for e in rows if e['kind'] == 'player_sample' and e['site'] == 'actor.update.begin'}
        for end in (e for e in rows if e['kind'] == 'player_sample' and e['site'] == 'actor.update.end'):
            begin = begins.get(end['actor'])
            if not begin:
                continue
            old = {s['identity']: s for s in begin['detail']['signs']}
            for sign in end['detail']['signs']:
                if sign['identity'] == end['actor'] and sign['identity'] in old and sign['parts'] != old[sign['identity']]['parts']:
                    same_tick = [c for c in contacts if c['tick'] == tick and c['defense']['actor'] == sign['identity']]
                    require(same_tick, f'tick {tick}: sign cut without real same-transaction contact')
                    require(old[sign['identity']]['collider']['ac_flags'] & 2, 'target did not consume AC_HIT')
                    player_end = next(e for e in rows if e['kind'] == 'player_sample' and
                                      e['site'] == 'actor.update.end' and
                                      e['actor'] == snapshots[tick]['player']['identity'])
                    require(max(c['sequence'] for c in same_tick) < player_end['sequence'] < begin['sequence'],
                            'sign response must follow collision and Player update')
                    target_responses.append({'tick': tick, 'identity': sign['identity'],
                        'before_parts': old[sign['identity']]['parts'], 'after_parts': sign['parts'],
                        'attack_pose_generation': snapshots[tick]['player_detail']['pose_generation']})
    if fixture['id'] in ('player-sign', 'player-z-sign'):
        require(target_responses, 'controlled target did not consume a real sword hit')
    if fixture['id'] == 'player-z-sign':
        require(any(s['player']['target'] in {sign['identity'] for sign in s['player_detail']['signs']} and
                    s['camera']['mode'] == 2 for s in snapshots),
                'Z-sign fixture did not lock onto a real sign with the friendly-target camera')
    if fixture['id'] == 'player-combo':
        starts = [after for before, after in zip(snapshots, snapshots[1:])
                  if before['player']['action'] != 'Player_Action_808502D0' and
                  after['player']['action'] == 'Player_Action_808502D0']
        require([s['player']['melee_animation'] for s in starts] == [4, 4, 6] and
                [s['player_detail']['combo_count'] for s in starts] == [1, 2, 3],
                'combo fixture must exercise two ordinary slashes and the third-attack transition')
        require(counts['combo_draw_mutation'] > 0, 'third attack did not exercise draw-owned combo mutation')
    if fixture['id'] == 'player-slash':
        require(snapshots[-1]['player']['melee_state'] == 0, 'representative attack did not finish')
    return {'status': 'pass', 'fixture': fixture['id'], 'output': str(output), 'ticks': fixture['ticks'],
            'counts': dict(counts), 'transitions': transitions, 'target_responses': target_responses,
            'contacts': [{'tick': e['tick'], 'ordinal': e['ordinal'], 'attack': e['attack'],
                          'defense': e['defense'], 'damage_flags': e['damage_flags']} for e in contacts],
            'targets': sorted({s['player']['target'] for s in snapshots if isinstance(s['player']['target'], str)}),
            'snapshots_sha256': replay.file_digest(output/'snapshots.jsonl'),
            'trace_sha256': replay.file_digest(output/'trace.jsonl')}


def analyze(corpus):
    receipt = replay.read_json(corpus/'corpus_result.json')
    require(receipt.get('status') == 'pass', 'corpus must pass before Player analysis')
    require({f['id'] for f in receipt['fixtures']} == EXPECTED, 'complete Player fixture set required')
    repeats = receipt.get('repeats')
    require(type(repeats) is int and repeats >= 1, 'positive declared repeat count required')
    results = []
    for name in sorted(EXPECTED):
        fixture = replay.read_json(corpus/name/'fixture.json')
        runs = sorted((corpus/name).glob('run-*/output'))
        require(len(runs) == repeats, 'complete declared repetition set required')
        for output in runs:
            results.append(observe(output, fixture))
    return {'schema': 1, 'status': 'pass', 'corpus': str(corpus), 'runs': results,
            'corpus_receipt_sha256': replay.file_digest(corpus/'corpus_result.json'),
            'scope': 'Canonical phase ownership and bounded real sign contacts; no high-rate admission.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--corpus', required=True, type=Path)
    args = parser.parse_args()
    corpus = args.corpus.resolve(strict=True)
    replay.local_output(corpus, create=False)
    try:
        result = analyze(corpus)
    except (OSError, ValueError, KeyError, IndexError, TypeError) as error:
        result = {'schema': 1, 'status': 'fail', 'error': str(error)}
    output = corpus/'player_state_result.json'
    replay.write_json(output, result)
    print(f"Player analysis {result['status']}: {output}")
    if 'error' in result:
        print(result['error'])
    return 0 if result['status'] == 'pass' else 1


if __name__ == '__main__':
    sys.exit(main())
