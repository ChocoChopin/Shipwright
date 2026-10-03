"""Reject changed ownership even when endpoint state could still match."""
import copy
import unittest
import analyze_player_state as analysis


def transaction():
    quad = {'registered_at': [], 'registered_ac': [], 'registered_oc': []}
    previous = {'player': {'identity': 'scene1:spawn1', 'weapon_geometry': [1], 'body_parts': [2]},
                'player_detail': {'sword_quads': [copy.deepcopy(quad)]}}
    endpoint = copy.deepcopy(previous)
    events = []
    for sequence, site in enumerate((*analysis.COLLISION_PHASES, 'actor.update.begin',
                                     'actor.update.end', 'pose.begin', 'pose.end')):
        events.append({'kind': 'player_sample', 'site': site, 'sequence': sequence * 10,
            'actor': 'scene1:spawn1', 'player': copy.deepcopy(previous['player']),
            'detail': {'sword_quads': [copy.deepcopy(quad)], 'shield_quad': copy.deepcopy(quad),
                       'pose_generation': int(site == 'pose.end'),
                       'joints': [0], 'combo_count': 0}})
    return previous, endpoint, events


class PlayerPhaseTests(unittest.TestCase):
    def test_canonical_order_and_previous_pose_are_accepted(self):
        self.assertEqual(analysis.check_tick(0, *transaction())['contacts'], 0)

    def test_moved_collision_cannot_hide_behind_identical_endpoints(self):
        previous, endpoint, events = transaction()
        events[0]['sequence'] = 75
        with self.assertRaisesRegex(analysis.replay.ReplayError, 'order'):
            analysis.check_tick(0, previous, endpoint, events)

    def test_collision_must_receive_prior_geometry(self):
        previous, endpoint, events = transaction()
        events[0]['player']['weapon_geometry'] = [99]
        with self.assertRaisesRegex(analysis.replay.ReplayError, 'prior draw'):
            analysis.check_tick(0, previous, endpoint, events)

    def test_duplicate_pose_fails(self):
        previous, endpoint, events = transaction()
        events.append(copy.deepcopy(events[-1]))
        with self.assertRaisesRegex(analysis.replay.ReplayError, 'exactly one'):
            analysis.check_tick(0, previous, endpoint, events)

    def test_registration_before_pose_fails(self):
        previous, endpoint, events = transaction()
        events.append({'kind': 'player_registration', 'index': 0, 'sequence': 65, 'collider': {'shape': 3}})
        with self.assertRaisesRegex(analysis.replay.ReplayError, 'escaped late pose'):
            analysis.check_tick(0, previous, endpoint, events)

    def test_contact_after_player_update_fails(self):
        previous, endpoint, events = transaction()
        events.append({'kind': 'player_contact', 'sequence': 65})
        with self.assertRaisesRegex(analysis.replay.ReplayError, 'outside original AT'):
            analysis.check_tick(0, previous, endpoint, events)

    def test_duplicate_registration_fails_even_with_matching_endpoint(self):
        previous, endpoint, events = transaction()
        events[-1]['detail']['sword_quads'][0]['registered_at'] = [0, 1]
        endpoint['player_detail']['sword_quads'][0]['registered_at'] = [0, 1]
        with self.assertRaisesRegex(analysis.replay.ReplayError, 'duplicate quad registration'):
            analysis.check_tick(0, previous, endpoint, events)
