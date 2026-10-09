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
    return dict(horizontal_map_checks=motion, moving_intervals=moving, animation_phase_checks=animation,
                intermediate_camera_changes=camera, controls=controls)


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
            results.append(dict(output=str(output), sha256=file_digest(stream), **inspect_steps(rows,fixture)))
    if not results:
        raise ReplayError('No high-rate Player receipts')
    return dict(status='pass', runs=results)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--corpus', required=True, type=Path)
    args = parser.parse_args()
    result = analyze(args.corpus)
    write_json(args.corpus/'player_rates.json',result)
    print(f"Player rate analysis: {len(result['runs'])} runs pass")
