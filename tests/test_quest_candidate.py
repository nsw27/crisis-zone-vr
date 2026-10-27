"""Run real host/scene and GLES tests, without a game ROM or Android build."""
from pathlib import Path
import argparse, json, os, shlex, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,default=ROOT/'build/candidate-tests');a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
flags=['-D_GNU_SOURCE','-std=gnu11','-O1','-g','-Wall','-Wextra','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-missing-field-initializers','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all']
flags+=['-I'+str(ROOT/x) for x in ['quest','upstream/tc2/src','upstream/tc2/include','upstream/tc2/gen']]
flags+=shlex.split(os.environ.get('CANDIDATE_TEST_CFLAGS',''))
libs=shlex.split(os.environ.get('CANDIDATE_TEST_LDFLAGS',''))
java=Path(os.environ.get('JAVA_HOME','/usr/lib/jvm/java-21-openjdk-amd64'))
results={}
def run(cmd,**kw):
    print('> '+shlex.join(list(map(str,cmd))),flush=True)
    return subprocess.run(list(map(str,cmd)),check=True,text=True,**kw)
def test(name,sources,extra=(),links=(),capture=False):
    exe=a.out/name
    run(['gcc',*flags,*extra,*[ROOT/s for s in sources],'-Wl,--gc-sections',*libs,*links,'-lm','-o',exe])
    with tempfile.TemporaryDirectory(dir=a.out,prefix=name+'-') as tmp:
        output=run([exe,ROOT] if name=='graphics' else [exe],cwd=tmp,capture_output=capture)
        if name=='graphics':
            import shutil
            for img in Path(tmp).glob('*.ppm'):shutil.copy2(img,a.out/img.name)
    results[name]='passed';return output
common=['quest/quest_options.c','quest/quest_cover.c','upstream/crszone/src/csz_game.c']
test('scene',['tests/crisis_zone_scene_fixture.c','quest/tc2_scene.c'])
x=test('rays',['tests/quest_candidate_ray_fixture.c','quest/tc2_scene.c'],capture=True)
y=test('rays-reference',['tests/quest_candidate_ray_fixture.c','quest/tc2_scene.c'],['-DTCVR_RAY_REFERENCE'],capture=True)
assert x.stdout==y.stdout,'Accelerated hits differ from exhaustive hits'
results['ray_comparison']='4096 bit-identical results';results['accelerated_counts']=x.stderr.strip();results['reference_counts']=y.stderr.strip()
test('host',['tests/quest_candidate_input_fixture.c',*common],['-DTCVR','-DXR_USE_PLATFORM_ANDROID','-DXR_USE_GRAPHICS_API_OPENGL_ES','-I'+str(java/'include'),'-I'+str(java/'include/linux')],['-lSDL2','-lGLESv2','-lEGL'])
test('graphics',['tests/quest_candidate_graphics_fixture.c','quest/quest_ui.c','quest/quest_options.c','quest/quest_gun.c'],[],['-lGLESv2','-lEGL'])
(a.out/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))
