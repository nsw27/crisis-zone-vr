#!/usr/bin/env python3
"""Verify generated arcade props, selective colour changes and actual GLES upload."""
from pathlib import Path
import argparse, hashlib, importlib.util, json, math, os, re, shlex, struct, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('assets',ROOT/'tools/build_arcade_weapons.py');assets=importlib.util.module_from_spec(spec);spec.loader.exec_module(assets)
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,default=ROOT/'build/arcade-assets');args=p.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
installed=ROOT/'quest/assets/models';evidence={}
with tempfile.TemporaryDirectory() as t:
 temp=Path(t);a=temp/'first';b=temp/'second';report=assets.build(a);assets.build(b)
 for file in a.iterdir():
  assert file.read_bytes()==(b/file.name).read_bytes(), 'Nondeterministic '+file.name
  assert file.read_bytes()==(installed/file.name).read_bytes(), 'Stale generated '+file.name
 evidence['deterministic_regeneration']='all generated assets and metadata identical'
 for name in ['arcade-machine-gun','arcade-shotgun']:
  raw=(a/(name+'.tcgun')).read_bytes();magic,n,ni,*tip=struct.unpack_from('<8sII3f',raw)
  assert magic==b'TCGUN002' and 0<n<=100000 and 0<ni<=300000 and ni%3==0
  assert len(raw)==28+n*40+ni*4
  vertices=list(struct.iter_unpack('<10f',raw[28:28+n*40]));idx=struct.unpack_from('<%dI'%ni,raw,28+n*40)
  assert all(0<=i<n for i in idx) and all(math.isfinite(x) for row in vertices for x in row)
  assert all(.99<sum(x*x for x in v[3:6])<1.01 and all(0<=x<=1 for x in v[6:9]) and v[9] in (0,1) for v in vertices)
  assert abs(tip[0])<1e-6 and tip[2]<0
  for i in range(0,ni,3):
   pa,pb,pc=[vertices[idx[i+k]][:3] for k in range(3)]
   normal=assets.cross(assets.sub(pb,pa),assets.sub(pc,pa));assert sum(x*x for x in normal)>1e-20
  evidence[name]={'vertices':n,'triangles':ni//3,'sha256':hashlib.sha256(raw).hexdigest(),'muzzle_m':tip}
 old=(installed/'namco-gun.tcgun').read_bytes();pink=(installed/'namco-gun-pink.tcgun').read_bytes()
 n,ni,w,h=struct.unpack_from('<4I',old,8);offset=36+n*32+ni*4
 assert old[:offset]==pink[:offset], 'Pistol geometry or calibration changed'
 unchanged=changed=0
 for i in range(offset,len(old),4):
  assert old[i+3]==pink[i+3]
  if old[i:i+3]==pink[i:i+3]:unchanged+=1
  else:
   r,g,b=old[i:i+3];hh,ss,vv=assets.colorsys.rgb_to_hsv(r/255,g/255,b/255)
   assert .48<=hh<=.69 and ss>=.22 and vv>=.035
   changed+=1
 assert changed==report['assets']['namco-gun-pink']['recoloured_pixels'] and changed>100000 and unchanged>100000
 evidence['pink_handgun']={'geometry_indices_normals_uvs_and_muzzle':'bit-identical','recoloured_blue_texels':changed,'unchanged_texels':unchanged}
 palette=(ROOT/'quest/quest_weapon_colours.h').read_text();triples=[tuple(map(int,x.split(','))) for x in re.findall(r'\{([0-9]+,[0-9]+,[0-9]+)\}',palette)]
 assert triples==list(assets.PALETTE.values());evidence['selector_palette']='matches all three asset identities'
flags=shlex.split(os.environ.get('CANDIDATE_TEST_CFLAGS',''));links=shlex.split(os.environ.get('CANDIDATE_TEST_LDFLAGS',''))
exe=out/'render'
subprocess.run(['gcc','-D_GNU_SOURCE','-std=gnu11','-O1','-g','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-I'+str(ROOT/'quest'),*flags,str(ROOT/'tests/quest_arcade_asset_fixture.c'),str(ROOT/'quest/quest_gun.c'),*links,'-lGLESv2','-lEGL','-lm','-o',str(exe)],check=True)
for name in ['arcade-machine-gun','namco-gun-pink','arcade-shotgun']:
 subprocess.run([str(exe),str(installed/(name+'.tcgun')),str(out/name)],check=True)
evidence['production_GLES']='12 real views; all render slots, recoil/flash, both-hand muzzle transforms and shutdown passed'
evidence['shotgun_gameplay']='NOT implemented or enabled by these asset tests'
(out/'results.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps(evidence,indent=2))
