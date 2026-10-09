"""Read-only high-rate motion/animation/control checks on real Player receipts.

Only free horizontal motion with no root displacement or wall correction is
compared to the declared velocity map. Collision/root transitions are explicitly
excluded from that equation, not silently accepted with a large tolerance.
"""
import argparse
import json
from pathlib import Path
import struct
from run_corpus import ReplayError, file_digest, read_json, write_json


def f32(value):
    return struct.unpack('f', struct.pack('f', value))[0]


def inspect_steps(rows, fixture):
    q = 120 // fixture['player_hz']
    motion = animation = camera = moving = 0
    for previous, current in zip(rows, rows[1:]):
        a, b = previous['player'], current['player']
        x, y = a['animation'], b['animation']
        temporal = current['temporal']
        intermediate = temporal['player_interval_start_q'] % 6 != 0
        if (x['movement_flags'] == y['movement_flags'] == 0 and
                not (a['bg_flags'] | b['bg_flags']) & 8 and intermediate):
            for axis in 'xz':
                expected = f32(a['position'][axis]['value'] + f32(b['velocity'][axis]['value'] * q * .25))
                if expected != b['position'][axis]['value']:
                    raise ReplayError(f"Horizontal velocity map mismatch at {temporal['player_step_id']}/{axis}")
            motion += 1
            moving += a['position'] != b['position']
        if (previous['temporal']['animation_generation'] == temporal['animation_generation'] and
                x['morph_weight']['value'] == y['morph_weight']['value'] == 0 and y['mode'] in (0, 2)):
            value, end = x['frame']['value'], y['end']['value']
            speed, length = y['speed']['value'], y['length']['value']
            expected = value if y['mode'] == 2 and value == end else f32(value + f32(speed * q * .25))
            if y['mode'] == 2 and (expected-end) * speed > 0:
                expected = end
            elif expected < 0:
                expected = f32(expected + length)
            elif expected >= length:
                expected = f32(expected - length)
            if expected != y['frame']['value']:
                raise ReplayError(f"Authored animation phase mismatch at {temporal['player_step_id']}")
            animation += 1
        if intermediate and current['camera']['input_direction'] != previous['camera']['input_direction']:
            camera += 1
        if temporal['pose_generation'] != previous['temporal']['pose_generation'] + 1:
            raise ReplayError('Missing or repeated authoritative pose')
    controls = []
    control_edges = fixture.get('expected_control_edges_q')
    if control_edges is None:
        control_edges = []
        for event in fixture['input']:
            numerator = event['time_num']*120
            if event['buttons'] or not (event['stick_x'] or event['stick_y']) or numerator % event['time_den']:
                continue
            edge = numerator//event['time_den']
            if edge % 6 and (((edge+q-1)//q)*q) % 6:
                control_edges.append(edge)
    for edge in control_edges:
        due = ((edge+q-1)//q)*q
        index = due//q
        before, after = rows[index-1], rows[index]
        event = next(e for e in fixture['input'] if e['time_num']*120 == edge*e['time_den'])
        t = after['temporal']
        if (due % 6 == 0 or t['last_input']['sequence'] != event['sequence'] or
                t['input_consuming_player_step'] != t['player_step_id'] or
                all(before['player'][key] == after['player'][key] for key in ('action','yaw','linear_velocity'))):
            raise ReplayError('Control did not respond at the next intermediate Player boundary')
        controls.append({'edge_q':edge,'response_q':due})
    if control_edges and min(motion,moving,animation,camera) == 0:
        raise ReplayError('Movement/control coverage was not exercised')
    target_checks = 0
    if fixture.get('spawn_distant_target'):
        edge = next(e for e in fixture['input'] if e['buttons'] & 8192)
        due = (edge['time_num']*120//edge['time_den']+q-1)//q*q
        first = next((r for r in rows if r.get('held_target')),None)
        origin = rows[0]['temporal']['player_interval_start_q']
        if (first is None or first['temporal']['player_interval_start_q'] != origin+due or
                due % 6 == 0 or first['camera']['mode'] != 2 or
                first['camera']['target'] != first['held_target']['identity']):
            raise ReplayError('Friendly target/camera did not acquire at the next Player boundary')
        for before,after in zip(rows,rows[1:]):
            if (before.get('held_target') and after.get('held_target') and
                    before['tick'] == after['tick']):
                if before['held_target'] != after['held_target']:
                    raise ReplayError('Held world target changed during an intermediate Player interval')
                target_checks += 1
        if target_checks == 0:
            raise ReplayError('No held-world target intervals observed')
    return dict(horizontal_map_checks=motion, moving_intervals=moving, animation_phase_checks=animation,
                intermediate_camera_changes=camera, controls=controls, held_target_checks=target_checks)


def analyze(corpus):
    receipt = read_json(corpus/'corpus_result.json')
    if receipt['status'] != 'pass':
        raise ReplayError('Corpus must pass before rate analysis')
    results = []
    for directory in sorted(corpus.iterdir()):
        if not directory.is_dir() or not (directory/'fixture.json').exists():
            continue
        fixture = read_json(directory/'fixture.json')
        if fixture.get('player_hz',20) == 20:
            continue
        for output in sorted(directory.glob('run-*/output')):
            stream = output/'player-steps.jsonl'
            rows = [json.loads(line) for line in stream.read_text().splitlines()]
            if fixture.get('expected_fallback_edge_q') is not None:
                continue  # The mixed-rate suffix has its separate temporal gate.
            results.append(dict(output=str(output), sha256=file_digest(stream), **inspect_steps(rows,fixture),
                                pose_trace=inspect_pose_trace(output,fixture)))
    if not results:
        raise ReplayError('No high-rate Player receipts')
    return dict(status='pass', runs=results)


def inspect_pose_trace(output, fixture):
    trace = output/'trace.jsonl'
    if not trace.exists():
        return {'status':'not-run','reason':'trace disabled'}
    pending = None
    poses = warm = sweeps = unchanged = target_updates = 0
    target_ids = set()
    with trace.open(encoding='utf-8') as source:
        for line in source:
            event=json.loads(line)
            if not event.get('measuring'): continue
            if event['kind'] in ('player_registration','player_contact'):
                raise ReplayError('Unbridged high-rate Player entered world collider/contact stream')
            if event['kind'] != 'player_sample': continue
            site=event['site']
            if site in ('pose.begin','player_step.begin'):
                if pending is not None: raise ReplayError('Overlapping Player pose intervals')
                pending=event
            elif site in ('pose.end','player_step.end'):
                if pending is None: raise ReplayError('Missing Player pose interval start')
                before,after=pending,event
                if after['detail']['pose_generation'] != before['detail']['pose_generation']+1:
                    raise ReplayError('Pose count differs from complete Player interval')
                for i,quad in enumerate(after['detail']['sword_quads'],1):
                    if any(quad['registered_'+k] for k in ('at','ac','oc')):
                        raise ReplayError('High-rate sword registered in legacy collision context')
                    a,b=before['player']['weapon_geometry'][i],after['player']['weapon_geometry'][i]
                    old_quad=before['detail']['sword_quads'][i-1]
                    if not a['active'] and b['active']:
                        if quad['vertices'] != old_quad['vertices']:
                            raise ReplayError('First active sample incorrectly formed a swept quad')
                        warm += 1
                    elif a['active'] and b['active'] and before['player']['melee_state']>0 and after['player']['melee_state']>0:
                        if a['base']==b['base'] and a['tip']==b['tip']:
                            if quad['vertices'] != old_quad['vertices']:
                                raise ReplayError('Unchanged endpoints incorrectly changed swept geometry')
                            unchanged += 1
                        else:
                            if quad['vertices'] != [b['base'],b['tip'],a['base'],a['tip']]:
                                raise ReplayError('Swept geometry lost the previous committed endpoints')
                            sweeps += 1
                if after['player']['target']:
                    target_ids.add(after['player']['target'])
                poses+=1; pending=None
            elif site=='actor.update.end' and event.get('actor') in target_ids:
                target_updates+=1
    if pending is not None or poses != fixture['ticks']*(fixture['player_hz']//20):
        raise ReplayError('Incomplete high-rate late-pose trace')
    if 'expected_attack_edge_q' in fixture and (warm<2 or sweeps<2):
        raise ReplayError('Sword fixture did not exercise warm-up and changed endpoint history')
    if fixture.get('spawn_distant_target') and not target_updates:
        raise ReplayError('Held target never received its world update')
    return dict(status='pass',poses=poses,warm_samples=warm,valid_sweeps=sweeps,
                no_motion_samples=unchanged,held_target_world_updates=target_updates)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--corpus', required=True, type=Path)
    args = parser.parse_args()
    result = analyze(args.corpus)
    write_json(args.corpus/'player_rates.json',result)
    print(f"Player rate analysis: {len(result['runs'])} runs pass")
