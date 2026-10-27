"""Synthetic protocol/parser tests only; no headset measurements are fabricated."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('qd', Path(__file__).resolve().parents[1] / 'tools/quest_diagnostics.py')
qd = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(qd)

HEADER = ','.join(qd.EXPECTED_FIELDS)

def capture(frames, records, metadata):
    return '\n'.join(['# synthetic test input; NOT real headset data',
                      f'# frames={frames},records={len(records)}', HEADER, *metadata, *records]) + '\n'

def row(frame, entry, model, polygons=1, valid=1, bounds='10,10,20,20'):
    return f'{frame},{entry},{model},{polygons},0,0,1,{valid},{bounds}'

def frame(f, entries=1, built=1, truncated=0):
    return f'# frame={f},submitted_entries={entries},built_polygons={built},truncated_model_records={truncated}'

def perf(density=100, gpu='1.25', render=72.0):
    return (f'[QPERF] SCENE CPU 0.75 MS/EYE | GPU {gpu} MS/EYE; '
            f'RENDER {render} FPS | GAME 59.9 FPS; SCENE 1440 X 1512 ({density}%) | TRI 123; '
            'UPLOAD 100 KB/S | WORLD DRAWS 8.0/FRAME')

class TraceTests(unittest.TestCase):
    def test_first_frame_is_not_spawn_event(self):
        result=qd.analyse_trace(capture(1,[row(0,0,1)],[frame(0)]))
        self.assertEqual(result['presence_changes'],[])
        self.assertTrue(result['header_consistent'])

    def test_duplicate_models_are_not_entity_ids(self):
        s=capture(2,[row(0,0,1),row(0,1,1),row(1,0,1)], [frame(0,2,2),frame(1)])
        result=qd.analyse_trace(s)
        self.assertEqual(result['duplicate_model_instances'],1)
        self.assertEqual(result['presence_changes'],[])

    def test_model_presence_change_only(self):
        r=qd.analyse_trace(capture(2,[row(0,0,1),row(1,0,2)],[frame(0),frame(1)]))
        self.assertEqual(r['presence_changes'],[{'frame':1,'newly_present_model_ids':[2],'no_longer_present_model_ids':[1]}])

    def test_truncated_frame_not_absence_evidence(self):
        r=qd.analyse_trace(capture(3,[row(0,0,1),row(2,0,2)],[frame(0),frame(1,1,0,1),frame(2)]))
        self.assertEqual(r['presence_changes'],[])
        self.assertEqual(r['truncated_frames'],[1])

    def test_empty_complete_frame_supported(self):
        r=qd.analyse_trace(capture(2,[row(0,0,1)],[frame(0),frame(1,0,0)]))
        self.assertEqual(r['presence_changes'][0]['no_longer_present_model_ids'],[1])

    def test_missing_header_disables_comparison(self):
        s=capture(2,[row(0,0,1),row(1,0,2)],[frame(0),frame(1)]).replace('# frames=2,records=2\n','')
        r=qd.analyse_trace(s);self.assertFalse(r['header_consistent']);self.assertFalse(r['presence_changes'])

    def test_missing_frame_disables_comparison(self):
        r=qd.analyse_trace(capture(3,[row(0,0,1),row(2,0,2)],[frame(0),frame(2)]))
        self.assertFalse(r['presence_changes']);self.assertTrue(r['warnings'])

    def test_inconsistent_counters_disable_transition(self):
        r=qd.analyse_trace(capture(2,[row(0,0,1),row(1,0,2,5)],[frame(0),frame(1)]))
        self.assertFalse(r['presence_changes']);self.assertTrue(r['warnings'])

    def test_repeated_entry_rejected(self):
        with self.assertRaises(ValueError):qd.analyse_trace(capture(1,[row(0,0,1),row(0,0,2)],[frame(0,2,2)]))

    def test_nonfinite_bounds_rejected(self):
        with self.assertRaises(ValueError):qd.analyse_trace(capture(1,[row(0,0,1,bounds='nan,1,2,3')],[frame(0)]))

    def test_invalid_bounds_not_counted_as_visible(self):
        r=qd.analyse_trace(capture(1,[row(0,0,1,0,0,'0,0,0,0')],[frame(0,1,0)]))
        self.assertEqual(r['valid_bounds_records'],0);self.assertEqual(r['zero_built_records'],1)

    def test_old_arcade_bounds_not_visibility(self):
        r=qd.analyse_trace(capture(1,[row(0,0,1,bounds='700,0,900,100')],[frame(0)]))
        self.assertEqual(r['bounds_outside_arcade'],1)
        self.assertIn('do not establish invisibility',r['interpretation'])

    def test_truncated_data_line_rejected(self):
        with self.assertRaises(ValueError):qd.analyse_trace(capture(1,['0,0,1'],[frame(0)]))

    def test_malformed_header_rejected(self):
        with self.assertRaises(ValueError):qd.analyse_trace('x,y,z\n1,2,3\n')

    def test_over_limit_frame_rejected(self):
        with self.assertRaises(ValueError):qd.analyse_trace(capture(601,[],[]))

class PerformanceTests(unittest.TestCase):
    def test_unavailable_gpu_never_zero(self):
        r=qd.analyse_log(perf(gpu='n/a'));s=r['blocks'][0]['stats']['scene_gpu_ms_per_eye']
        self.assertEqual(s['samples'],0);self.assertIsNone(s['median'])

    def test_requested_refresh_not_fps(self):
        r=qd.analyse_log('[XR] requested 120 Hz\n'+perf(render=71.8))
        self.assertEqual(r['blocks'][0]['stats']['render_rate']['median'],71.8)

    def test_density_groups_not_combined_on_return(self):
        r=qd.analyse_log('\n'.join([perf(100),perf(85),perf(100)]))
        self.assertEqual(len(r['blocks']),3)

    def test_malformed_metric_not_silently_parsed(self):
        r=qd.analyse_log('[QPERF] changed format')
        self.assertEqual(r['sample_count'],0);self.assertEqual(r['unrecognised_metric_lines'],1)

    def test_no_metrics_handled(self):
        self.assertEqual(qd.analyse_log('regular gameplay log')['sample_count'],0)

class CollectionTests(unittest.TestCase):
    def test_authorisation_failure_clear(self):
        with patch.object(qd,'adb_run',return_value=b'List of devices attached\nquest unauthorized\n'):
            with self.assertRaisesRegex(RuntimeError,'authorised'):qd.choose_device('adb',None)

    def test_multiple_devices_never_arbitrarily_selected(self):
        with patch.object(qd,'adb_run',return_value=b'List of devices attached\na device\nb device\n'):
            with self.assertRaises(RuntimeError):qd.choose_device('adb',None)
            self.assertEqual(qd.choose_device('adb','b'),'b')

    def test_only_allowlisted_files_and_no_device_mutations(self):
        calls=[]
        def adb(_exe,args):
            calls.append(args)
            if args==['devices']:return b'List of devices attached\nprivate-serial device\n'
            if args[-1]=='pwd':return b'/data/user/0/org.crisiszone.vr.preview2\n'
            if args[-1]=='files/crisiszone-vr.log':return perf().encode()
            raise RuntimeError('No such file')
        with tempfile.TemporaryDirectory() as t,patch.object(qd,'adb_run',side_effect=adb):
            info=qd.collect('adb',qd.PACKAGES[0],None,Path(t))
            self.assertEqual(list(info['files']),['crisiszone-vr.log'])
            self.assertNotIn('private-serial',json.dumps(info))
            for args in calls[2:]:
                self.assertEqual(args[2:4],['shell','-T']);self.assertEqual(args[4:6],['run-as',qd.PACKAGES[0]])
                self.assertEqual(args[6:9],['head','-c',str(qd.MAX_BYTES+1)])
                self.assertIn(args[-1],['files/'+n for n in qd.FILES])

    def test_other_package_refused_before_adb(self):
        with patch.object(qd,'adb_run') as call:
            with self.assertRaises(ValueError):qd.collect('adb','other;command',None,Path('.'))
            call.assert_not_called()

    def test_no_success_without_readable_files(self):
        def adb(_exe,args):
            if args==['devices']:return b'List of devices attached\na device\n'
            if args[-1]=='pwd':return b'/data/user/0/org.crisiszone.vr.preview2\n'
            raise RuntimeError('No such file')
        with tempfile.TemporaryDirectory() as t,patch.object(qd,'adb_run',side_effect=adb):
            with self.assertRaisesRegex(RuntimeError,'No logs'):qd.collect('adb',qd.PACKAGES[0],None,Path(t))

    def test_cli_offline_and_overwrite_protection(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);src=root/'input';src.mkdir();out=root/'report'
            (src/'crisiszone-vr.log').write_text(perf(),encoding='utf-8')
            cmd=[sys.executable,str(Path(qd.__file__).resolve()),'--from-folder',str(src),'--out',str(out)]
            p=subprocess.run(cmd,capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stderr)
            report=json.loads((out/'summary.json').read_text());self.assertEqual(report['logs']['crisiszone-vr.log']['sample_count'],1)
            again=subprocess.run(cmd,capture_output=True,text=True);self.assertEqual(again.returncode,2)
            self.assertIn('never overwritten',again.stderr)

if __name__=='__main__':unittest.main()
