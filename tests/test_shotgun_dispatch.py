#!/usr/bin/env python3
"""Execute real CSZO4 gun acceptance, native ray projection and hit dispatch.

Only the final collision consumer is recorded, not a simulated enemy health
system. This establishes three separate ordinary hit calls, not a universal
threefold damage guarantee across the game's enemies and bosses.
"""
from pathlib import Path
import ast, hashlib, importlib.util, json, re, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/shotgun-dispatch';out.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('hooks',ROOT/'tools/prepare_weapon_hooks.py')
hooks=importlib.util.module_from_spec(spec);spec.loader.exec_module(hooks)
parts={p:(ROOT/f'upstream/crszone/gen/crszone_lifted_{p}.c').read_bytes() for p in ['00','08','30']}
entries={'00':['80004BD0','80006B60'],'08':['80090930','800913F0','80091484','800947F0','80095038'],'30':['801F3CA0']}
def extract(modified):
 code=[]
 for p,names in entries.items():
  data=hooks.instrument(parts[p],p) if modified and p in hooks.SOURCE_HASHES else parts[p]
  s=data.decode()
  for name in names:
   m=re.search(r'/\* ---- [^\n]*? @ 0x'+name+r' ---- \*/\n(.*?)(?=\n/\* ---- |\Z)',s,re.S)
   assert m,name
   code.append(m[1])
 return '\n'.join(code)
# Reuse scheduler/register/memory scaffolding, not the old fixture's tests/stubs.
base=(ROOT/'tests/test_weapon_hooks.py').read_text()
headers=next(ast.literal_eval(n.value) for n in ast.parse(base).body
             if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='headers' for t in n.targets))
headers+='''
static int fixture_x,fixture_y,hit_count;
static int hits[32][3];
static uint64_t hit_ticks[32];
'''
code=extract(True)
owned=set(re.findall(r'void L_([0-9A-F]{8})_at',code))
callees=set(re.findall(r'\bL_([0-9A-F]{8})(?:_at)?\(',code))-owned
stubs=[]
for name in sorted(callees):
 body='RS8(0x10,0);'
 if name=='800AEC2C':body='MWR4((uint32_t)RG4(0x24),fixture_x);MWR4((uint32_t)RG4(0x2C),fixture_y);RS8(0x10,1);'
 if name=='800648BC':body='''assert(hit_count<32); uint32_t p=(uint32_t)RG4(0x2C);
 for(int i=0;i<3;i++)hits[hit_count][i]=(int32_t)MRD4(p+4*i);
 hit_ticks[hit_count++]=csz_weapons_state()->tick; RS8(0x10,1);'''
 stubs.append(f'void L_{name}_at(uint32_t pc){{(void)pc;{body}rr_return((uint32_t)RG4(0xFC));}}')
harness=(ROOT/'tests/quest_shotgun_dispatch_harness.inc').read_text()
reports={}
for label,modified in [('candidate',True),('original',False)]:
 path=out/(label+'.c');path.write_text(headers+extract(modified)+'\n'+'\n'.join(stubs)+'\n'+harness)
 exe=out/label
 cmd=['gcc','-std=gnu11','-O0','-g0','-w','-fwrapv','-fno-strict-aliasing','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-DTC2_ADDR_MASK=0x1FFFFFFFu',
      '-I'+str(ROOT/'quest'),'-I'+str(ROOT/'upstream/tc2/include'),'-I'+str(ROOT/'upstream/crszone/gen'),str(path),str(ROOT/'quest/crisis_zone_weapons.c'),'-lm','-o',str(exe)]
 print('Compiling',label,flush=True);subprocess.run(cmd,check=True,timeout=90)
 image=out/(label+'.bin')
 run=subprocess.run([str(exe),'compare',str(image)],check=True,text=True,capture_output=True,timeout=30)
 if modified:
  result=subprocess.run([str(exe)],check=True,text=True,capture_output=True,timeout=30)
  print(result.stdout,flush=True);print(result.stderr,file=sys.stderr)
  reports['dispatch']=result.stdout.strip()
reports['disabled_comparison']= '128 native dispatch updates, registers/RAM/instruction budgets byte-identical'
a=(out/'candidate.bin').read_bytes();b=(out/'original.bin').read_bytes();assert a==b
reports['comparison_sha256']=hashlib.sha256(a).hexdigest()
for part,original in parts.items():
 assert (ROOT/f'upstream/crszone/gen/crszone_lifted_{part}.c').read_bytes()==original
reports['source_files_unchanged']=True
reports['source_sha256']={part:hashlib.sha256(data).hexdigest() for part,data in parts.items()}
reports['actual_functions']=sorted(owned);reports['mocked_callees']=sorted(callees)
reports['limits']='Final enemy collision consumer is recorded, not a live enemy/boss damage test; no headset measurement.'
(out/'results.json').write_text(json.dumps(reports,indent=2)+'\n')
print(json.dumps(reports,indent=2))
