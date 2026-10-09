import copy
import unittest
from analyze_player_rates import inspect_steps
from run_corpus import ReplayError


class PlayerRateReceiptTests(unittest.TestCase):
    def setUp(self):
        def value(x): return {'value':x}
        self.fixture = {'player_hz':120,'input':[dict(time_num=0,time_den=120,sequence=0,
                                                   buttons=0,stick_x=0,stick_y=0)]}
        self.rows = []
        for i in range(2):
            self.rows.append({'player':{
                'position':dict(x=value(float(i)),y=value(0),z=value(0)),
                'velocity':dict(x=value(4),y=value(0),z=value(0)), 'bg_flags':1,
                'animation':dict(movement_flags=0,morph_weight=value(0),mode=0,
                                 frame=value(i*.25),end=value(9),speed=value(1),length=value(10))},
                'temporal':dict(player_step_id=i+1,player_interval_start_q=i,
                                animation_generation=1,pose_generation=i+1),
                'camera':{'input_direction':{'y':i}}})

    def test_checks_actual_interval_maps(self):
        result = inspect_steps(self.rows,self.fixture)
        self.assertEqual(result['horizontal_map_checks'],1)
        self.assertEqual(result['animation_phase_checks'],1)

    def test_rejects_displacement_phase_and_duplicate_pose_controls(self):
        for field,wrong in [('position',1.5),('animation',1.5),('pose',3)]:
            rows = copy.deepcopy(self.rows)
            if field == 'position': rows[1]['player']['position']['x']['value'] = wrong
            elif field == 'animation': rows[1]['player']['animation']['frame']['value'] = wrong
            else: rows[1]['temporal']['pose_generation'] = wrong
            with self.subTest(field=field),self.assertRaises(ReplayError):
                inspect_steps(rows,self.fixture)
