/* Exercise the real upstream polygon builder, not only the VR renderer.
 * Synthetic geometry; no game ROM and no replacement fade callbacks. */
#include "../upstream/tc2/src/tc2_render.c"
#include <assert.h>

static poly sample(int blend, float x) {
    poly p={0};p.n=3;p.blend=blend;
    for(int i=0;i<3;i++) {
        p.pv[i].x=x+(i==1?.1f:0);p.pv[i].y=i==2?.1f:0;
        p.pv[i].p[0]=1;p.pv[i].p[3]=64;
    }
    return p;
}
int main(void) {
    BLD=calloc(1,sizeof *BLD);assert(BLD);FS=&BLD->fs;
    tc2_render_entry *e=&BLD->fs.list.e[0];BLD->fs.list.count=1;
    e->type=TC2_RE_MODEL;e->model=1;e->vp_size_x=320;e->vp_size_y=240;e->vp_fov=772.53125f;
    unsetenv("TC2_XYCULL");unsetenv("TC2_XYCULL_ALL");
    setenv("TCVR_WIDE_FOV","1",1); /* retired setting must not change culling */
    for(int blend=0;blend<2;blend++) {
        poly p=sample(blend,0);
        assert(finish(&p,e,0,0,17,0x1FFFF,0xFFFC0000u));
        assert(p.blend==blend&&p.alpha_enabled==0&&p.model==1);
        p=sample(blend,6.f);
        assert(finish(&p,e,0,0,17,0x1FFFF,0xFFFC0000u)==!blend);
    }
    uint32_t data[64]={0};data[0]=data[1]=8;data[8]=0x10000|(3<<8);data[10]=0x40404040;
    const int xyz[3][3]={{-8192,-8192,98304},{8192,-8192,98304},{0,8192,98304}};
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)data[11+i*3+j]=(uint32_t)xyz[i][j]&0xFFFFFF;
    ptrom=data;ptrom_limit=64;e->m[0]=e->m[4]=e->m[8]=16384;e->scaling=1;
    for(int blend=0;blend<2;blend++) {
        BLD->npoly=0;data[9]=(uint32_t)blend<<10;
        render_model(e);
        assert(BLD->npoly==1&&BLD->polys[0].n==3);
        assert(BLD->polys[0].blend==blend&&BLD->polys[0].alpha_enabled==0);
    }
    free(BLD);
    puts("PASS: real upstream polygon builder links without retired host callbacks; authored opacity and smoke culling preserved");
    return 0;
}
