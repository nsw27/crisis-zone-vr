#include "tc2_scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
void qvr_ray_stats(unsigned long long*,unsigned long long*,unsigned long long*);
int main(void){
    tc2_built *b=calloc(1,sizeof *b);assert(b);
    b->fs.list.count=1;b->fs.list.e[0].type=TC2_RE_MODEL;
    b->fs.list.e[0].vp_fov=772.53125f;b->fs.list.e[0].vp_offset_x=15;b->fs.list.e[0].vp_offset_y=9;
    qvr_units_per_meter=1;qvr_world_scale=1;qvr_view_distance=0;qvr_flat_view=0;
    for(int y=0;y<32;y++)for(int x=0;x<32;x++){
        tc2_poly *p=&b->polys[b->npoly++];p->entry=0;p->model=1;p->n=4;p->vp_size_x=320;p->vp_size_y=240;
        p->vp_offset_x=15;p->vp_offset_y=9;
        float xy[4][2]={{x*20,y*15},{(x+1)*20,y*15},{(x+1)*20,(y+1)*15},{x*20,(y+1)*15}};
        for(int j=0;j<4;j++){p->pv[j].x=xy[j][0];p->pv[j].y=xy[j][1];p->pv[j].p[0]=1.f/6;}
    }
    qvr_scene_prepare(b);float nx,ny;V3 hit;
    for(int i=0;i<4096;i++){
        V3 o=v3(i&1?.3f:-.3f,-.1f,-.3f);
        float sx=(i%64)*10+1,sy=(i/64)*7.5f+1;
        V3 target=v3((sx-320)*6/772.53125f,(240-sy)*6/772.53125f,-6);
        bool yes=qvr_aim(o,sub(target,o),&nx,&ny,&hit);
        printf("%d %a %a %a %a %a\n",yes,(double)nx,(double)ny,(double)hit.x,(double)hit.y,(double)hit.z);
        if(yes){assert(fabsf(nx-(sx+15)/640)<1e-5f);assert(fabsf(ny-(sy-9)/480)<1e-5f);}
    }
    assert(!qvr_aim(v3(NAN,0,0),v3(0,0,-1),&nx,&ny,&hit));
    assert(!qvr_aim(v3(0,0,0),v3(0,0,1),&nx,&ny,&hit));
    unsigned long long q,e,p;qvr_ray_stats(&q,&e,&p);fprintf(stderr,"RAYS %llu EXACT %llu POSSIBLE %llu\n",q,e,p);
    free(b);return 0;
}
