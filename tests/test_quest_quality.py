"""Render-pass and spatial-filter regression tests; synthetic assets, no headset."""
from pathlib import Path
import argparse, hashlib, json, os, re, shlex, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--baseline-renderer',required=True,type=Path)
p.add_argument('--out',type=Path,default=ROOT/'build/quality-tests');a=p.parse_args()
out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
flags=['-D_GNU_SOURCE','-std=gnu11','-O1','-g','-ffunction-sections','-fdata-sections','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all']
flags+=['-I'+str(ROOT/x) for x in ('quest','upstream/tc2/src','upstream/tc2/include','upstream/tc2/gen')]
flags+=shlex.split(os.environ.get('CANDIDATE_TEST_CFLAGS',''))
libs=shlex.split(os.environ.get('CANDIDATE_TEST_LDFLAGS',''))+['-lSDL2','-lGLESv2','-lEGL','-lm']
def run(cmd,**kw):
    print('> '+shlex.join(list(map(str,cmd))),flush=True)
    return subprocess.run(list(map(str,cmd)),check=True,text=True,timeout=180,**kw)
def compile_fixture(name,source,extra=()):
    exe=out/name
    run(['gcc',*flags,*extra,ROOT/'tests'/source,ROOT/'quest/tc2_scene.c','-Wl,--gc-sections',*libs,'-o',exe]);return exe
results={}
exe=compile_fixture('menu-pointer','quest_menu_pointer_fixture.c')
r=run([exe],capture_output=True);print(r.stdout);results['menu_pointer']=r.stdout.strip()
for name,extra in [('grouped',[]),('preview2',['-DRENDER_SOURCE="'+str(a.baseline_renderer.resolve())+'"'])]:
    exe=compile_fixture(name,'quest_renderer_enhancement_fixture.c',['-DQTEST_CASES=96',*extra])
    image=exe.with_suffix('.rgba');r=run([exe,image],capture_output=True)
    (out/(name+'.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr)
    switches=re.search(r'TARGET_SWITCHES (\d+)',r.stderr)
    results[name]={'sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'target_switches':int(switches[1])}
assert results['grouped']['sha256']==results['preview2']['sha256'],'Grouped rendering differs from Preview 2'
assert results['grouped']['target_switches']<results['preview2']['target_switches'],'No target-switch reduction'
results['image_comparison']='192 eye views byte-identical to Preview 2; includes text-only and empty scenes'
exe=compile_fixture('edge-smoothing','quest_edge_smoothing_fixture.c')
r=run([exe],cwd=out,capture_output=True);print(r.stdout+r.stderr)
results['edge_smoothing']=r.stdout.strip()
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))
