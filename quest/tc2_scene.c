/* Recover the System 23 camera-space geometry before applying either eye pose.
 * OpenXR adds local head movement to the original game's camera. Text and
 * direct screen quads use the same optical camera at a distance of 2.5 m. */
#include "tc2_scene.h"
#include "crisis_zone_layers.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#ifdef TCVR_PC
#include "../tests/tc2_capture.h"
#endif
int qvr_flat_view;
float qvr_world_scale=1.f;
/* PORT: provisional System 23 scale inherited from TC2; headset calibration
 * is required. WORLD SIZE remains adjustable. */
float qvr_units_per_meter=2.f/3.f;
float qvr_view_distance;
int qvr_wide_fov_enabled; /* Legacy setting retained for source compatibility; no projection scaling. */
static V3 world_vertex(V3 v){return sub(mul(v,qvr_world_scale),v3(0,0,qvr_view_distance));}
typedef struct {float cx,cy,f;} Camera;
typedef struct {V3 a,b,c;Camera cam;} Triangle;
static Triangle triangles[TC2_MAX_POLYS*6];
static int triangle_count;
/* Conservative 32-triangle boxes. No sorting, approximate hits, or cached results. */
enum { RAY_GROUP=32 };
typedef struct {float lo[3],hi[3];} RayBounds;
static RayBounds ray_bounds[(TC2_MAX_POLYS*6+RAY_GROUP-1)/RAY_GROUP];
static unsigned long long ray_queries,ray_exact,ray_possible;
void qvr_ray_stats(unsigned long long *queries,unsigned long long *exact,unsigned long long *possible){
    *queries=ray_queries;*exact=ray_exact;*possible=ray_possible;
}
static void build_ray_bounds(void){
    for(int first=0;first<triangle_count;first+=RAY_GROUP){
        RayBounds *b=&ray_bounds[first/RAY_GROUP];
        for(int k=0;k<3;k++){b->lo[k]=FLT_MAX;b->hi[k]=-FLT_MAX;}
        int end=first+RAY_GROUP;if(end>triangle_count)end=triangle_count;
        for(int i=first;i<end;i++){
            V3 v[3]={triangles[i].a,triangles[i].b,triangles[i].c};
            for(int j=0;j<3;j++){float xyz[3]={v[j].x,v[j].y,v[j].z};
                for(int k=0;k<3;k++){b->lo[k]=fminf(b->lo[k],xyz[k]);b->hi[k]=fmaxf(b->hi[k],xyz[k]);}}
        }
        for(int k=0;k<3;k++){
            float pad=1e-4f*(1+fmaxf(fabsf(b->lo[k]),fabsf(b->hi[k])));
            b->lo[k]-=pad;b->hi[k]+=pad;
        }
    }
}
static bool ray_box(V3 origin,V3 direction,const RayBounds *b){
    double o[3]={origin.x,origin.y,origin.z},d[3]={direction.x,direction.y,direction.z};
    double low=0,high=DBL_MAX;
    for(int k=0;k<3;k++){
        if(d[k]==0){if(o[k]<b->lo[k]||o[k]>b->hi[k])return false;continue;}
        double a=(b->lo[k]-o[k])/d[k],c=(b->hi[k]-o[k])/d[k];
        if(a>c){double t=a;a=c;c=t;}if(a>low)low=a;if(c<high)high=c;
        if(high<low)return false;
    }
    return true;
}
static int nearest_triangle(V3 origin,V3 direction,float *nearest){
    int found=-1;ray_queries++;ray_possible+=triangle_count;
    for(int first=0;first<triangle_count;first+=RAY_GROUP){
#ifndef TCVR_RAY_REFERENCE
        if(!ray_box(origin,direction,&ray_bounds[first/RAY_GROUP]))continue;
#endif
        int end=first+RAY_GROUP;if(end>triangle_count)end=triangle_count;
        for(int i=first;i<end;i++){
            Triangle *t=&triangles[i];float distance=*nearest;ray_exact++;
            if(ray_triangle(origin,direction,t->a,t->b,t->c,&distance)&&distance<*nearest){*nearest=distance;found=i;}
        }
    }
    return found;
}

static const tc2_built *frame;
static uint64_t scene_serial;
uint64_t qvr_scene_serial(void){return scene_serial;}
static bool zone_select;
bool qvr_scene_is_panel(void){return qvr_flat_view||zone_select||triangle_count==0;}
static float effect_depth[TC2_RENDER_MAX];
static int letterbox_top,letterbox_bottom=480;
/* Retained through title screens with no polygons. */
static Camera main_camera={320,240,772.53125f};
static bool camera_valid(Camera c){return isfinite(c.f)&&c.f>1&&isfinite(c.cx)&&isfinite(c.cy);}
static Camera camera(const tc2_built *b,const tc2_poly *p){
    if(p->entry<0||p->entry>=b->fs.list.count)return main_camera;
    const tc2_render_entry *e=&b->fs.list.e[p->entry];
    Camera c={320.f+e->vp_offset_x,240.f-e->vp_offset_y,e->vp_fov};
    return camera_valid(c)?c:main_camera;
}
QTC2Layer qvr_tc2_layer(const tc2_built *b,const tc2_poly *p){
    if(p->entry<0||p->entry>=b->fs.list.count)return QTC2_OVERLAY;
    const tc2_render_entry *e=&b->fs.list.e[p->entry];
    if(b==frame&&zone_select)return QTC2_OVERLAY;
    if(e->type==TC2_RE_DIRECT)return QTC2_OVERLAY;
    if(e->type!=TC2_RE_MODEL)return QTC2_WORLD;
    if(csz_screen_effect(e))return QTC2_PROJECTED;
    /* PORT: CSZO4 Ver. B, measured in captured gameplay frames: 9010 is the
     * ejected HUD cartridge, 9011 ammunition, 9012 health, 9017 RELOAD,
     * 9094/9098 the pedal instruction, 9107/9108 the startup logos.
     * Their ~0.47 (or 0.047) synthetic depths are screen layout. Keep them
     * with the C361 status text at 2.5 physical metres and out of aim rays.
     * Nearby IDs such as 9039 are actual world effects; do not flatten a range. */
    if((p->model>=9010&&p->model<=9012)||p->model==9017||
       p->model==9094||p->model==9098||p->model==9107||p->model==9108)
        return QTC2_OVERLAY;
    return QTC2_WORLD;
}
static bool world(const tc2_built *b,const tc2_poly *p){return qvr_tc2_layer(b,p)==QTC2_WORLD;}
void qvr_tc2_letterbox(int *top,int *bottom){*top=letterbox_top;*bottom=letterbox_bottom;}
static V3 unproject(Camera c,float sx,float sy,float z){return v3((sx-c.cx)*z/c.f,(c.cy-sy)*z/c.f,-z);}
static V3 base_vertex(const tc2_built *b,const tc2_poly *p,int index){
    const tc2_pvert *v=&p->pv[index];
    float sx=v->x+(320-p->vp_size_x)+p->vp_offset_x;
    float sy=v->y+(240-p->vp_size_y)-p->vp_offset_y;
    if(world(b,p)&&isfinite(v->p[0])&&v->p[0]>0)
        return unproject(camera(b,p),sx,sy,1.f/v->p[0]/qvr_units_per_meter);
    return unproject(main_camera,sx,sy,2.5f);
}
V3 qvr_tc2_vertex(const tc2_built *b,const tc2_poly *p,int index){
    if(qvr_tc2_layer(b,p)==QTC2_PROJECTED){
        const tc2_pvert *v=&p->pv[index];
        float sx=v->x+320-p->vp_size_x+p->vp_offset_x;
        float sy=v->y+240-p->vp_size_y-p->vp_offset_y;
        float z=b==frame?effect_depth[p->entry]:0;
        /* Keep the original screen size and animation, but put the entire
         * billboard on its scene surface. Misses use the input fallback plane. */
        return z>0?world_vertex(unproject(camera(b,p),sx,sy,z)):
                   unproject(main_camera,sx,sy,2.5f);
    }
    V3 v=base_vertex(b,p,index);
    return world(b,p)?world_vertex(v):v;
}
bool qvr_flat_camera(float *cx,float *cy,float *focal){
    Camera c=qvr_flat_view?(Camera){320,240,500}:main_camera;
    *cx=c.cx;*cy=c.cy;*focal=c.f;return !qvr_flat_view;
}
const tc2_built *qvr_current_frame(void){return frame;}
#include "quest_scene_trace.inc"
void qvr_scene_prepare(const tc2_built *b){
    qvr_trace_capture(b);
    scene_serial++;frame=b;triangle_count=0;zone_select=false;memset(effect_depth,0,sizeof effect_depth);
    letterbox_top=0;letterbox_bottom=480;if(!b)return;
    zone_select=csz_zone_select(&b->fs);
    if(zone_select)for(int i=0;i<b->fs.list.count;i++){
        const tc2_render_entry *e=&b->fs.list.e[i];
        if(e->type==TC2_RE_MODEL&&e->model==9128){
            Camera c={320.f+e->vp_offset_x,240.f-e->vp_offset_y,e->vp_fov};
            if(camera_valid(c))main_camera=c;
            break;
        }
    }
#ifdef TCVR_PC
    static unsigned capture_frame;tc2_capture_frame(b,++capture_frame);
#endif
    static bool init;
    if(!init){init=true;const char *s=getenv("TC2_UNITS_PER_METER");if(s){float v=strtof(s,NULL);if(isfinite(v)&&v>.0001f)qvr_units_per_meter=v;}}
    struct {Camera cam;unsigned count;} candidates[64];int nc=0;
    for(int i=0;i<b->npoly;i++){
        const tc2_poly *p=&b->polys[i];if(p->n<3||p->n>8)continue;
        QTC2Layer layer=qvr_tc2_layer(b,p);
        if(layer==QTC2_LETTERBOX){
            float y0=FLT_MAX,y1=-FLT_MAX;
            for(int j=0;j<p->n;j++){float y=p->pv[j].y+240-p->vp_size_y-p->vp_offset_y;y0=fminf(y0,y);y1=fmaxf(y1,y);}
            if(y1<240)letterbox_top=(int)fmaxf(letterbox_top,ceilf(y1));
            if(y0>240)letterbox_bottom=(int)fminf(letterbox_bottom,floorf(y0));
        }
        if(layer!=QTC2_WORLD)continue;
        Camera c=camera(b,p);
        /* Aim must use the real optical camera. Do not scale its focal length. */
        int k;
        for(k=0;k<nc;k++)if(!memcmp(&c,&candidates[k].cam,sizeof c))break;
        if(k==nc&&nc<64){candidates[nc].cam=c;candidates[nc++].count=0;}
        if(k<nc)candidates[k].count+=(unsigned)(p->n-2);
    }
    unsigned best=0;for(int i=0;i<nc;i++)if(candidates[i].count>best){main_camera=candidates[i].cam;best=candidates[i].count;}
    float low=FLT_MAX,high=0;
    for(int i=0;i<b->npoly;i++){
        const tc2_poly *p=&b->polys[i];if(!world(b,p)||p->n<3||p->n>8)continue;
        V3 v[8];bool good=true;
        for(int j=0;j<p->n;j++){
            v[j]=base_vertex(b,p,j); /* Cache base metres so paused scale edits apply immediately. */
            if(!isfinite(v[j].x)||!isfinite(v[j].y)||!isfinite(v[j].z)||v[j].z>=0)good=false;
            low=fminf(low,-v[j].z);high=fmaxf(high,-v[j].z);
        }
        if(!good)continue;
        for(int j=1;j+1<p->n&&triangle_count<(int)(sizeof triangles/sizeof *triangles);j++)
            triangles[triangle_count++]=(Triangle){v[0],v[j],v[j+1],camera(b,p)};
    }
    build_ray_bounds();
    /* A screen-space effect is not a solid target. Resolve it only after
     * building the world cache, from the game's optical camera through its
     * screen anchor. Both eyes then see it at the same depth as that surface. */
    if(!zone_select)for(int i=0;i<b->fs.list.count;i++){
        const tc2_render_entry *e=&b->fs.list.e[i];if(!csz_screen_effect(e))continue;
        if(e->v[2]<=0)continue;
        V3 d=v3((float)e->v[0]/e->v[2],(float)e->v[1]/e->v[2],-1);
        float nearest=FLT_MAX;
        nearest_triangle(v3(0,0,0),d,&nearest);
        if(nearest<FLT_MAX)effect_depth[i]=nearest;
    }
    static unsigned frames;
    if(++frames%300==0)fprintf(stderr,"[TC2VR] geometry polygons=%d triangles=%d camera=%.3f,%.3f,%.3f depth=%.3f..%.3f m units/m=%.3f\n",b->npoly,triangle_count,main_camera.cx,main_camera.cy,main_camera.f,triangle_count?low:0,high,qvr_units_per_meter);
}
bool qvr_aim(V3 o,V3 d,float *nx,float *ny,V3 *hit){
    *nx=*ny=.5f;*hit=o;
    if(!isfinite(o.x)||!isfinite(o.y)||!isfinite(o.z)||
       !isfinite(d.x)||!isfinite(d.y)||!isfinite(d.z)||
       !isfinite(qvr_world_scale)||qvr_world_scale<=0||!isfinite(qvr_view_distance))return false;
    bool panel=qvr_flat_view||zone_select||triangle_count==0;
    V3 base=panel?o:mul(add(o,v3(0,0,qvr_view_distance)),1.f/qvr_world_scale),projected;
    float nearest=FLT_MAX;Camera c=main_camera;
    int found=panel?-1:nearest_triangle(base,d,&nearest);
    if(found>=0){c=triangles[found].cam;projected=add(base,mul(d,nearest));*hit=world_vertex(projected);}
    else {
        if(d.z>=-1e-5f)return false;
        float t=(-2.5f-base.z)/d.z;if(!isfinite(t)||t<=0)return false;
        projected=add(base,mul(d,t));*hit=panel?projected:world_vertex(projected);
        if(qvr_flat_view)c=(Camera){320,240,500};
    }
    if(!camera_valid(c)||!isfinite(projected.x)||!isfinite(projected.y)||
       !isfinite(projected.z)||projected.z>=-1e-6f)return false;
    float x=(c.cx+c.f*projected.x/-projected.z)/640.f;
    float y=(c.cy-c.f*projected.y/-projected.z)/480.f;
    if(!isfinite(x)||!isfinite(y))return false;
    *nx=fmaxf(0,fminf(1,x));*ny=fmaxf(0,fminf(1,y));
    /* Outside the arcade hit region is a miss, not a fake edge hit. */
    return x>=0&&x<=1&&y>=0&&y<=1;
}
