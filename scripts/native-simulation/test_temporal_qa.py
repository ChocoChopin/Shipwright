import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from run_corpus import ReplayError
from temporal_qa import validate_temporal, compare_temporal, publish_command

class TemporalReceiptTests(unittest.TestCase):
    def test_atomic_publication_retries_same_command_after_reader_lock(self):
        with patch('temporal_qa.os.replace',side_effect=[PermissionError(),None]) as replace, \
             patch('temporal_qa.time.monotonic',return_value=1), patch('temporal_qa.time.sleep'):
            publish_command(Path('pending'),Path('command'),2)
        self.assertEqual(replace.call_count,2)
        self.assertEqual(replace.call_args_list[0],replace.call_args_list[1])
    def test_atomic_publication_lock_has_bounded_timeout(self):
        with patch('temporal_qa.os.replace',side_effect=PermissionError()), \
             patch('temporal_qa.time.monotonic',return_value=2), self.assertRaises(ReplayError):
            publish_command(Path('pending'),Path('command'),2)
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name)
        self.fixture = {"ticks":1,"observe_player_state":True}
        self.rows = [{"tick":i,"fixture_time_q":6*i,"okay":True,"effective_player_hz":20,"world_hz":20,
                      "player_high_rate_admitted":False,"contact_bridge_active":False,"contact_queue_count":0,
                      "time_q":360+6*i,"world_step_id":60+i,"player_time_q":354+6*i,
                      "player_step_id":59+i,"queued_input_samples":0,"attack_epoch":0} for i in range(2)]
        self.result = {"status":"pass","single_step":True,"canonical_time_q":6,"canonical_transaction_id":1,
                       "qa_commands":1,"qa_holds":1}
    def check(self):
        (self.path/"temporal-result.json").write_text(json.dumps(self.result))
        (self.path/"temporal.jsonl").write_text(''.join(json.dumps(row)+'\n' for row in self.rows))
        return validate_temporal(self.path,self.fixture,True)
    def test_accepts_separate_elapsed_domains(self):
        self.assertEqual(self.check()["status"],"pass")
    def test_rejects_hidden_high_rate_or_bridge(self):
        original=copy.deepcopy(self.rows)
        for key,value in [("effective_player_hz",120),("world_hz",60),("player_high_rate_admitted",True),
                          ("contact_bridge_active",True),("contact_queue_count",1)]:
            self.rows=copy.deepcopy(original); self.rows[1][key]=value
            with self.subTest(key=key),self.assertRaises(ReplayError): self.check()
    def test_rejects_clock_drift_or_duplicate_step(self):
        original=copy.deepcopy(self.rows)
        for key in ["time_q","player_time_q","world_step_id","player_step_id","fixture_time_q"]:
            self.rows=copy.deepcopy(original); self.rows[1][key]+=1
            with self.subTest(key=key),self.assertRaises(ReplayError): self.check()
    def test_rejects_missing_hold_or_pending_edge(self):
        self.result["qa_holds"]=0
        with self.assertRaises(ReplayError): self.check()
        self.result["qa_holds"]=1; self.rows[1]["queued_input_samples"]=1
        with self.assertRaises(ReplayError): self.check()
    def test_metadata_comparison_does_not_hide_generation_change(self):
        self.check()
        candidate = self.path/'candidate'; candidate.mkdir()
        data = (self.path/'temporal.jsonl').read_bytes()
        (candidate/'temporal.jsonl').write_bytes(data)
        self.assertEqual(compare_temporal(self.path,candidate)['status'],'pass')
        self.rows[1]['attack_epoch'] = 1
        (candidate/'temporal.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in self.rows))
        self.assertEqual(compare_temporal(self.path,candidate)['status'],'mismatch')

    def test_high_rate_receipt_rejects_skipped_or_world_multiplied_step(self):
        for hz in (60,120):
            parts, q = hz//20, 120//hz
            self.fixture['player_hz'] = hz
            self.result['single_step'] = False
            self.rows[1].update(effective_player_hz=hz,player_high_rate_admitted=True,player_step_id=59+parts)
            steps=[]
            for index in range(parts):
                steps.append({'tick':0,'world_gameplay_frames':61,'world_opportunities':dict.fromkeys(
                    ('actors','collision','blink','scripts','environment','hud','message','audio'),1),'temporal':{
                    'okay':True,'effective_player_hz':hz,'player_step_id':60+index,
                    'player_interval_start_q':360+q*index,'player_interval_end_q':360+q*(index+1),
                    'world_step_id':60}})
            (self.path/'temporal-result.json').write_text(json.dumps(self.result))
            (self.path/'temporal.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in self.rows))
            def check_steps(rows,single=False):
                (self.path/'player-steps.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in rows))
                return validate_temporal(self.path,self.fixture,single)
            self.assertEqual(check_steps(steps)['player_steps'],parts)
            with self.assertRaises(ReplayError): check_steps(steps[:-1])
            for key in ('player_step_id','player_interval_start_q','world_step_id'):
                bad=copy.deepcopy(steps); bad[1]['temporal'][key]+=1
                with self.subTest(hz=hz,key=key),self.assertRaises(ReplayError): check_steps(bad)
            bad=copy.deepcopy(steps); bad[1]['world_gameplay_frames']+=1
            with self.assertRaises(ReplayError): check_steps(bad)
            bad=copy.deepcopy(steps); bad[1]['world_opportunities']['blink']=2
            with self.assertRaises(ReplayError): check_steps(bad)
            commands=[dict(sequence=i+1,tick=0,player_offset_q=i*q,operation='step_player') for i in range(parts)]
            self.result.update(single_step=True,qa_commands=parts,qa_holds=parts,player_commands=commands)
            (self.path/'temporal-result.json').write_text(json.dumps(self.result))
            self.assertTrue(check_steps(steps,True)['single_step'])
            commands[1]['player_offset_q'] += q
            (self.path/'temporal-result.json').write_text(json.dumps(self.result))
            with self.assertRaises(ReplayError): check_steps(steps,True)

    def test_fallback_requires_partial_interval_and_preserved_edge(self):
        self.fixture.update(ticks=4,player_hz=120,expected_fallback_edge_q=7,
            input=[dict(time_num=7,time_den=120,sequence=1,buttons=16)])
        self.result.update(single_step=False,canonical_time_q=24,canonical_transaction_id=4)
        base=copy.deepcopy(self.rows[0]); self.rows=[]
        for i in range(5):
            high=min(i*6,7); suffix=max(0,i-2)
            row=copy.deepcopy(base)
            row.update(tick=i,fixture_time_q=i*6,time_q=360+i*6,world_step_id=60+i,
                player_time_q=354+high+suffix*6,player_step_id=59+high+suffix,
                effective_player_hz=120 if i==1 else 20,player_high_rate_admitted=i==1,
                queued_input_samples=int(i==2),high_rate_fallback_latched=i>=2,
                high_rate_rejection='unsupported R',scope_generation=int(i>=2),
                last_input=dict(sequence=1,pressed=16),consumed_input_edges=int(i>=3),
                input_consuming_player_step=59+high+suffix)
            self.rows.append(row)
        steps=[dict(tick=i//6,world_gameplay_frames=61+i//6,
            world_opportunities=dict.fromkeys(('actors','collision','blink','scripts','environment','hud','message','audio'),1),
            temporal=dict(okay=True,effective_player_hz=120,player_step_id=60+i,
                player_interval_start_q=360+i,player_interval_end_q=361+i,world_step_id=60+i//6))
            for i in range(7)]
        (self.path/'player-steps.jsonl').write_text(''.join(json.dumps(s)+'\n' for s in steps))
        def check():
            (self.path/'temporal-result.json').write_text(json.dumps(self.result))
            (self.path/'temporal.jsonl').write_text(''.join(json.dumps(s)+'\n' for s in self.rows))
            return validate_temporal(self.path,self.fixture,False)
        self.assertEqual(check()['expected_player_packets'],9)
        for key,value in [('queued_input_samples',0),('player_step_id',67)]:
            old=self.rows[2][key]; self.rows[2][key]=value
            with self.assertRaises(ReplayError): check()
            self.rows[2][key]=old
        self.rows[3]['consumed_input_edges']=2
        with self.assertRaises(ReplayError): check()
