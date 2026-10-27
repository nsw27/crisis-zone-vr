"""Run real host/scene and GLES tests, without a game ROM or Android build."""
from pathlib import Path
import argparse, json, os, shlex, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--baseline-renderer',type=Path);p.add_argument('--baseline-model',type=Path);p.add_argument('--out',type=Path,default=ROOT/'build/candidate-tests');a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
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
test('gpu-query-protocol',['tests/quest_gpu_timer_fixture.c'])
test('support',['tests/quest_support_fixture.c','quest/quest_options.c'])
test('trace',['tests/quest_trace_fixture.c','quest/tc2_scene.c'])
common=['quest/quest_options.c','quest/quest_cover.c','upstream/crszone/src/csz_game.c']
test('world-renderer',['tests/quest_world_renderer_fixture.c'],links=['-pthread'])
test('scene',['tests/crisis_zone_scene_fixture.c','quest/tc2_scene.c'])
x=test('rays',['tests/quest_candidate_ray_fixture.c','quest/tc2_scene.c'],capture=True)
y=test('rays-reference',['tests/quest_candidate_ray_fixture.c','quest/tc2_scene.c'],['-DTCVR_RAY_REFERENCE'],capture=True)
assert x.stdout==y.stdout,'Accelerated hits differ from exhaustive hits'
results['ray_comparison']='4096 bit-identical results';results['accelerated_counts']=x.stderr.strip();results['reference_counts']=y.stderr.strip()
test('host',['tests/quest_candidate_input_fixture.c',*common],['-DTCVR','-DXR_USE_PLATFORM_ANDROID','-DXR_USE_GRAPHICS_API_OPENGL_ES','-I'+str(java/'include'),'-I'+str(java/'include/linux')],['-lSDL2','-lGLESv2','-lEGL'])
test('graphics',['tests/quest_candidate_graphics_fixture.c','quest/quest_ui.c','quest/quest_options.c','quest/quest_gun.c'],[],['-lGLESv2','-lEGL'])

# Real pipeline comparisons include gamma, alpha, stencil, text priorities,
# off-centre cameras, depth mode and both eyes. Normal and expanded fans must
# be pixel-identical; optional --baseline-renderer checks the actual old file.
def renderer(mode, extra=()):
    exe=a.out/('renderer-'+mode);image=exe.with_suffix('.rgba')
    run(['gcc',*flags,*extra,ROOT/'tests/quest_renderer_enhancement_fixture.c',ROOT/'quest/tc2_scene.c','-Wl,--gc-sections',*libs,'-lSDL2','-lGLESv2','-lEGL','-lm','-o',exe])
    measured=run([exe,image],capture_output=True);print(measured.stdout);print(measured.stderr)
    import re
    counts=re.search(r'COMPARED texture_uploads=(\d+) texture_bytes=(\d+) buffer_bytes=(\d+)',measured.stderr)
    if counts:results['renderer_'+mode+'_work']=dict(zip(['texture_uploads','texture_bytes','buffer_bytes'],map(int,counts.groups())))
    return image
import hashlib
candidate=renderer('indexed');reference=renderer('expanded',['-DTCVR_RENDER_REFERENCE'])
assert candidate.read_bytes()==reference.read_bytes(), 'Indexed/expanded output differs'
results['renderer_comparison']='24 eye views bit-identical'
results['renderer_sha256']=hashlib.sha256(candidate.read_bytes()).hexdigest()
if a.baseline_renderer:
    baseline=renderer('preview-1',['-DQTEST_BASELINE','-DRENDER_SOURCE="'+str(a.baseline_renderer.resolve())+'"'])
    assert candidate.read_bytes()==baseline.read_bytes(),'Output differs from working Preview 1'
    results['preview_1_comparison']='24 eye views bit-identical to actual Preview 1 source'
(a.out/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))

def model(name,extra=()):
    exe=a.out/name;data=exe.with_suffix('.bin')
    run(['gcc',*flags,'-fwrapv','-fno-strict-aliasing',*extra,ROOT/'tests/quest_model_transform_fixture.c','-Wl,--gc-sections','-pthread','-lm','-o',exe])
    run([exe,data]);return data
candidate_model=model('model-prepared')
if a.baseline_model:
    old_model=model('model-preview-1',['-DMODEL_SOURCE="'+str(a.baseline_model.resolve())+'"'])
    assert candidate_model.read_bytes()==old_model.read_bytes(),'Prepared model matrix/light differs from original'
    results['model_comparison']='1024 deterministic configurations bit-identical to Preview 1 model builder'
    results['model_output_sha256']=hashlib.sha256(candidate_model.read_bytes()).hexdigest()
(a.out/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))
