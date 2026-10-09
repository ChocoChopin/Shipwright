import copy
import json
from pathlib import Path
import tempfile
import unittest
from analyze_player_rates import inspect_steps, inspect_pose_trace
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

    def test_target_requires_real_acquisition_and_held_world_sample(self):
        self.fixture.update(spawn_distant_target=True)
        self.fixture['input'].append(dict(time_num=1,time_den=120,sequence=1,buttons=8192))
        self.rows.append(copy.deepcopy(self.rows[1]))
        self.rows[2]['player']['position']['x']['value']=2.0
        self.rows[2]['player']['animation']['frame']['value']=.5
        self.rows[2]['temporal'].update(player_step_id=3,player_interval_start_q=2,pose_generation=3)
        for row in self.rows:
            row['tick']=0
            row['held_target']={'identity':'sign','position':1} if row is not self.rows[0] else None
            row['camera'].update(mode=2,target='sign')
        self.assertEqual(inspect_steps(self.rows,self.fixture)['held_target_checks'],1)
        self.rows[2]['held_target']['position']=2
        with self.assertRaises(ReplayError): inspect_steps(self.rows,self.fixture)
        for row in self.rows: row['held_target']=None
        with self.assertRaises(ReplayError): inspect_steps(self.rows,self.fixture)

    def test_sword_warmup_sweep_and_no_registration_controls(self):
        def sample(site,gen,active,base,tip,vertices):
            quad=dict(vertices=vertices,registered_at=[],registered_ac=[],registered_oc=[])
            weapon=dict(active=active,base=base,tip=tip)
            return dict(measuring=True,kind='player_sample',site=site,
                player=dict(weapon_geometry=[None,weapon,weapon],melee_state=1,target=None),
                detail=dict(pose_generation=gen,sword_quads=[quad,quad]))
        rows=[sample('pose.begin',0,0,0,0,[0]*4),sample('pose.end',1,1,0,1,[0]*4),
              sample('player_step.begin',1,1,0,1,[0]*4),sample('player_step.end',2,1,2,3,[2,3,0,1]),
              sample('player_step.begin',2,1,2,3,[2,3,0,1]),sample('player_step.end',3,1,2,3,[2,3,0,1])]
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)
            def check(data):
                (output/'trace.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in data))
                return inspect_pose_trace(output,dict(ticks=1,player_hz=60,expected_attack_edge_q=7))
            self.assertEqual(check(rows)['warm_samples'],2)
            self.assertEqual(check(rows)['valid_sweeps'],2)
            bad=copy.deepcopy(rows); bad[1]['detail']['sword_quads'][0]['vertices'][0]=1
            with self.assertRaises(ReplayError): check(bad)
            bad=copy.deepcopy(rows); bad[3]['detail']['sword_quads'][0]['registered_at']=[0]
            with self.assertRaises(ReplayError): check(bad)
