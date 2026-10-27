#!/usr/bin/env python3
"""Verify exact lifted hook placement and execute real game functions on synthetic RAM.

Only audio/graphics/device callees are mocked. Uses tracked generated C, no ROM.
This is a bounded integration test, not a full-game or headset performance test.
"""
from pathlib import Path
import argparse, hashlib, importlib.util, json, re, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('weapon_hooks',ROOT/'tools/prepare_weapon_hooks.py')
hooks=importlib.util.module_from_spec(spec);spec.loader.exec_module(hooks)
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,default=ROOT/'build/weapon-hooks');args=p.parse_args()
out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
src=ROOT/'upstream/crszone/gen'
original={part:(src/f'crszone_lifted_{part}.c').read_bytes() for part in hooks.SOURCE_HASHES}
patched={part:hooks.instrument(data,part).decode() for part,data in original.items()}
for part,data in original.items():
    try:hooks.instrument(data+b'\n',part)
    except ValueError:pass
    else:raise AssertionError('Source drift was accepted')
    assert (src/f'crszone_lifted_{part}.c').read_bytes()==data
# Extract complete real generated functions; do not reimplement the instructions.
entries={'00':['80004BD0'],'08':['80090930','800913F0','80091484','800947F0']}
def extract(source):
    functions=[]
    for part,names in entries.items():
        for name in names:
            m=re.search(r'/\* ---- [^\n]*? @ 0x'+name+r' ---- \*/\n(.*?)(?=\n/\* ---- |\Z)',source[part],re.S)
            assert m,name
            functions.append(m[1])
    return '\n'.join(functions)
code=extract(patched)
owned=set(re.findall(r'void L_([0-9A-F]{8})_at',code))
callees=set(re.findall(r'\bL_([0-9A-F]{8})(?:_at)?\(',code))-owned
stubs=[]
for name in sorted(callees):
    body='RS8(0x10,0);'
    if name=='800AEC2C':body='MWR4((uint32_t)RG4(0x24),0);MWR4((uint32_t)RG4(0x2C),0);RS8(0x10,1);'
    stubs.append(f'void L_{name}_at(uint32_t pc){{(void)pc;{body}rr_return((uint32_t)RG4(0xFC));}}')
headers='''#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lift_rt.h"
#include "tc2_mips_rt.h"
#include "crszone_lifted.h"
#include "crisis_zone_weapons.h"
uint8_t R[RR_REGSPACE],tc2_ram[0x1000000];
int32_t rr_budget;uint32_t rr_pc,rr_ret_to;
int rd_on,rd_stop_on,rd_poll_rec;
static uint32_t returns[64];static int depth;
int rd_hook(uint32_t p){(void)p;return 0;}
int rd_jump_stop(uint32_t t,uint32_t at){(void)t;(void)at;return 0;}
void rd_poll_snap(void){}
void rr_tick(void){fprintf(stderr,"unexpected scheduler tick %x\\n",rr_pc);abort();}
int rr_call_push(uint32_t ret){assert(depth<64);returns[depth]=ret;return depth++;}
int rr_return(uint32_t t){assert(depth>0&&returns[depth-1]==t);depth--;rr_ret_to=t;return 1;}
int rr_after_call(int index){assert(depth==index);return 0;}
void rr_jump(uint32_t t,uint32_t at){fprintf(stderr,"unexpected jump %x at %x\\n",t,at);abort();}
uint32_t rr_read(uint32_t a,int n){fprintf(stderr,"unexpected IO read %x/%d\\n",a,n);abort();}
void rr_write(uint32_t a,int n,uint32_t v){(void)v;fprintf(stderr,"unexpected IO write %x/%d\\n",a,n);abort();}
void rr_mips_setISAMode(uint32_t pc,uint64_t mode){(void)pc;assert(!mode);}
void rr_fpe(uint32_t cause,uint32_t fcsr,char op){(void)cause;(void)fcsr;(void)op;abort();}
'''
harness=(ROOT/'tests/quest_weapon_lifted_harness.inc').read_text()
path=out/'weapon-lifted-fixture.c';path.write_text(headers+'\n'+code+'\n'+'\n'.join(stubs)+'\n'+harness)
cmd=['gcc','-std=gnu11','-O1','-g','-w','-fwrapv','-fno-strict-aliasing','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-DTC2_ADDR_MASK=0x1FFFFFFFu',
     '-I'+str(ROOT/'quest'),'-I'+str(ROOT/'upstream/tc2/include'),'-I'+str(src),str(path),str(ROOT/'quest/crisis_zone_weapons.c'),'-lm','-o',str(out/'lifted-integration')]
subprocess.run(cmd,check=True,timeout=90)
r=subprocess.run([str(out/'lifted-integration')],text=True,capture_output=True,timeout=30)
print(r.stdout,end='');print(r.stderr,end='',file=sys.stderr);r.check_returncode()
report={'source_hash_guards':'passed','original_sources_unchanged':True,'actual_generated_functions':sorted(owned),'mocked_callees':sorted(callees),'result':r.stdout.strip()}
images=[]
for label,source in [('original',extract({p:b.decode() for p,b in original.items()})),('instrumented',code)]:
    compare_source=out/(label+'.c')
    compare_source.write_text(headers+'\n'+source+'\n'+'\n'.join(stubs)+'\n'+harness)
    compare_exe=out/label;image=out/(label+'.bin')
    command=[str(compare_source) if c==str(path) else str(compare_exe) if c==str(out/'lifted-integration') else c for c in cmd]
    command.insert(1,'-DQTEST_COMPARE')
    subprocess.run(command,check=True,timeout=90)
    subprocess.run([str(compare_exe),str(image)],check=True,timeout=30)
    images.append(image.read_bytes());image.unlink()
assert images[0]==images[1],'Default-disabled mode changed arcade registers, ammo or recorded game RAM'
report['default_mode_comparison']='640 game updates: registers, recorded RAM and instruction budgets byte-identical to unmodified lifted functions'
report['default_mode_sha256']=hashlib.sha256(images[0]).hexdigest()
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(report['default_mode_comparison'])
