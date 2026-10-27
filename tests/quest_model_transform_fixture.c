/* Exact old/new model-builder comparison; no proprietary data. */
#ifndef MODEL_SOURCE
#define MODEL_SOURCE "../upstream/tc2/src/tc2_render.c"
#endif
#include MODEL_SOURCE
#include <assert.h>
static uint32_t seed=0x20261027;
static uint32_t random_bits(void){seed=seed*1664525u+1013904223u;return seed;}
int main(int argc,char **argv){
    assert(argc==2);FILE *out=fopen(argv[1],"wb");assert(out);
    BLD=calloc(1,sizeof *BLD);assert(BLD);FS=&BLD->fs;
    uint32_t data[2048]={0};ptrom=data;ptrom_limit=2048;data[0]=data[1]=16;data[2]=1024;
    tc2_render_entry *e=&BLD->fs.list.e[0];BLD->fs.list.count=1;
    e->type=TC2_RE_MODEL;e->model=1;e->vp_size_x=320;e->vp_size_y=240;e->vp_fov=772.53125f;
    for(int sample=0;sample<1024;sample++){
        memset(BLD->polys,0,sizeof BLD->polys);BLD->npoly=0;e->transpose=sample&1;e->model2=sample&2?2:0;e->model_blend_factor=random_bits()&16383;
        e->scaling=(sample%4+1)*.25f;e->light_ambient=sample%32;e->light_power=32;e->mirror_x=sample%7==0;
        for(int k=0;k<9;k++)e->m[k]=(int16_t)((int)(random_bits()%16385)-8192);
        e->m[0]=e->m[4]=e->m[8]=16384;
        for(int k=0;k<3;k++){e->v[k]=k==2?262144:(int32_t)(random_bits()%32768)-16384;e->light_vector[k]=(int32_t)(random_bits()%513)-256;}
        if(sample%17==0)e->light_vector[0]=e->light_vector[1]=e->light_vector[2]=0;
        int lmode=sample%4;unsigned offs=16;
        for(int poly=0;poly<8;poly++){
            unsigned begin=offs,ne=4;uint32_t type=(ne<<8)|(lmode<<19)|(poly==7?0x10000:0);
            data[offs++]=type;data[offs++]=sample%3==0?0x400:0;
            if(lmode==3)for(unsigned j=0;j<ne;j++)data[offs++]=random_bits();else data[offs++]=0x40404040;
            for(unsigned j=0;j<ne;j++){
                int x=(j==1||j==2)?8192:-8192,y=j>=2?8192:-8192,z=98304+poly*1024;
                data[offs++]=(uint32_t)x&0xffffff;data[offs++]=(uint32_t)y&0xffffff;data[offs++]=z;
            }
            /* same structure with a shifted second model, exercising morphing */
            for(unsigned j=begin;j<offs;j++)data[1024+j-16]=data[j];
            for(unsigned j=offs-12;j<offs;j+=3)data[1024+j-16]=(data[j]+256)&0xffffffu;
            if(lmode==3)for(unsigned j=begin+2;j<begin+6;j++)data[1024+j-16]^=0x00100401u;
        }
        render_model(e);assert(BLD->npoly>0);
        assert(fwrite(&BLD->npoly,sizeof BLD->npoly,1,out)==1);
        assert(fwrite(BLD->polys,sizeof BLD->polys[0],BLD->npoly,out)==(size_t)BLD->npoly);
    }
    fclose(out);free(BLD);puts("PASS: 1024 deterministic model-builder configurations including transposes, morphing, normals, light vectors and blending");
}
