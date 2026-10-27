#include "tc2_scene.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void tc2_render_assets(const uint8_t **r,const uint32_t **m,const uint8_t **a){*r=NULL;*m=NULL;*a=NULL;}
int main(void){
    tc2_built *b=calloc(1,sizeof *b);assert(b);b->fs.list.count=2;
    for(int i=0;i<2;i++){b->fs.list.e[i].type=TC2_RE_MODEL;b->fs.list.e[i].model=7;b->fs.list.e[i].v[2]=1000;}
    b->npoly=1;b->polys[0].entry=0;b->polys[0].model=7;b->polys[0].n=3;
    qvr_trace_set(true);assert(qvr_trace_active());qvr_scene_prepare(b);qvr_trace_flush();
    assert(!fopen("quest-scene-trace.csv","r")); /* no filesystem writes while recording */
    b->fs.list.count=1;qvr_scene_prepare(b);qvr_trace_set(false);qvr_trace_flush();
    FILE *f=fopen("quest-scene-trace.csv","r");assert(f);char text[4096];size_t n=fread(text,1,sizeof text-1,f);text[n]=0;fclose(f);
    assert(strstr(text,"frames=2,records=3"));assert(strstr(text,"0,1,7,0,")); /* submitted but no built polygons */
    assert(strstr(text,"# frame=1,submitted_entries=1"));
    qvr_trace_set(true);for(int i=0;i<605;i++)qvr_scene_prepare(b);assert(!qvr_trace_active());qvr_trace_flush();
    f=fopen("quest-scene-trace.csv","r");assert(f);n=fread(text,1,sizeof text-1,f);text[n]=0;fclose(f);assert(strstr(text,"frames=600,records=600"));
    free(b);puts("PASS: bounded one-shot scene trace, duplicate model entries, submitted-vs-built distinction, deferred local write, 600-frame stop");
}
