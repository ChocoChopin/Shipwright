"""Keep the bounded semantic audit executable: required units, owners and real anchors."""
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
class TemporalInventoryTests(unittest.TestCase):
    def test_unit_contracts_and_source_anchors(self):
        data=json.loads((ROOT/'docs/native-simulation/player-temporal-units.json').read_text())
        allowed={'RATE','DURATION','IMPULSE','DISPLACEMENT','AUTHORED_ANIMATION_PHASE','ORDINAL_STATE',
                 'LEGACY_OPPORTUNITY','PLAYER_OPPORTUNITY','EVENT_BRIDGE','PRESENTATION_ONLY','MIXED / REQUIRES SPLIT'}
        entries=data['entries']
        self.assertEqual(len(entries),len({row['id'] for row in entries}))
        self.assertEqual({row['class'] for row in entries},allowed)
        for row in entries:
            with self.subTest(id=row['id']):
                for key in ['fields','class','units','owner','source','symbols','reset','future','fixtures']:
                    self.assertTrue(row[key])
                source=(ROOT/data['source_roots'][row['source']]).read_text(encoding='utf-8')
                for symbol in row['symbols']: self.assertIn(symbol,source)
        by_id={row['id']:row for row in entries}
        self.assertEqual(by_id['root_motion']['class'],'DISPLACEMENT')
        self.assertEqual(by_id['combo_mixed']['class'],'MIXED / REQUIRES SPLIT')
        self.assertEqual(by_id['blink_world']['class'],'LEGACY_OPPORTUNITY')
