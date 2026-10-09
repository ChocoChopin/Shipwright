"""Compare compact native phase/contact fingerprints to a retained canonical trace.

Reads historical detail once; candidates never emit full phase snapshots. Common
boundary semantic hashes remain a separate mandatory comparison.
"""
import argparse
import json
from pathlib import Path
from run_corpus import read_json, write_json, file_digest
from semantic_checkpoints import semantic_hash


def project(event):
    kind = event.get('kind')
    if kind == 'player_sample':
        result = dict(kind=kind, site=event['site'], melee_state=event['player']['melee_state'],
                      melee_animation=event['player']['melee_animation'], combo=event['detail']['combo_count'])
    elif kind == 'player_registration':
        result = {k: event[k] for k in ('kind', 'category', 'index', 'collider')}
    elif kind == 'player_contact':
        result = {k: event[k] for k in ('kind', 'ordinal', 'attack', 'defense', 'damage_flags', 'position')}
    else:
        return None
    result.update({k: event[k] for k in ('tick', 'phase', 'actor')})
    return result


def historical(reference):
    trace = reference/'trace.jsonl'
    digest = file_digest(trace)
    cached = reference/'player-phase-contact-reference.json'
    if cached.exists():
        document = read_json(cached)
        if document.get('trace_sha256') == digest:
            return document
    fixture = read_json(reference/'result.json')['fixture']
    order, count, tick = 14695981039346656037, 0, 0
    boundaries = [dict(tick=0, events=0, order=str(order))]
    with trace.open(encoding='utf-8') as stream:
        for line in stream:
            event = json.loads(line)
            if not event.get('measuring'):
                continue
            record = project(event)
            if record is None:
                continue
            while tick < event['tick']:
                tick += 1
                boundaries.append(dict(tick=tick, events=count, order=str(order)))
            for byte in semantic_hash(record).encode('ascii'):
                order = ((order ^ byte) * 1099511628211) & ((1 << 64)-1)
            count += 1
    while tick < fixture['ticks']:
        tick += 1
        boundaries.append(dict(tick=tick, events=count, order=str(order)))
    document = dict(format='player-phase-contact-v1', fixture=fixture,
                    boundaries=boundaries, trace_sha256=digest)
    write_json(cached, document)
    return document


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    reference = historical(args.reference)
    candidate = read_json(args.candidate/'player-phase-contact.json')
    assert candidate['format'] == reference['format']
    assert candidate['fixture'] == reference['fixture'], 'fixture identity mismatch'
    assert candidate['boundaries'] == reference['boundaries'], 'ordered phase/contact fingerprint mismatch'
    write_json(args.output, dict(status='pass', boundaries=len(candidate['boundaries']),
        events=candidate['boundaries'][-1]['events'], reference_trace_sha256=reference['trace_sha256'],
        candidate_sha256=file_digest(args.candidate/'player-phase-contact.json'),
        scope='Exact ordered canonical samples, melee/combo state, registrations and contacts; no new full trace'))
    print(f"Canonical phase/contact audit PASS: {len(candidate['boundaries'])} boundaries")


if __name__ == '__main__':
    main()
