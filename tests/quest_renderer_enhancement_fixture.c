/* Real GLES pipeline tests with synthetic assets only, never a ROM. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quest_gpu.h"
static int count_calls;
static GLuint tracked_fbo;static unsigned long long measured_switches;
static void tracked_bind(GLenum target,GLuint fbo){
    if(target==GL_FRAMEBUFFER){if(count_calls&&fbo!=tracked_fbo)measured_switches++;tracked_fbo=fbo;}
    glBindFramebuffer(target,fbo);
}
#define glBindFramebuffer tracked_bind
static unsigned long long measured_uploads,measured_texbytes,measured_vertexbytes;
static void tracked_image(GLenum target,GLint level,GLint internal,GLsizei w,GLsizei h,GLint border,GLenum fmt,GLenum type,const void *data);
static void tracked_sub(GLenum target,GLint level,GLint x,GLint y,GLsizei w,GLsizei h,GLenum fmt,GLenum type,const void *data);
static void tracked_buffer(GLenum target,GLsizeiptr bytes,const void *data,GLenum usage);
#define glTexImage2D tracked_image
#define glTexSubImage2D tracked_sub
#define glBufferData tracked_buffer
#ifndef RENDER_SOURCE
#define RENDER_SOURCE "../quest/tc2_gl.c"
#endif
#include RENDER_SOURCE

#undef glTexImage2D
#undef glTexSubImage2D
#undef glBufferData
#undef glBindFramebuffer
static unsigned channels(GLenum fmt){return fmt==GL_RGBA?4:fmt==GL_RGB?3:fmt==GL_RG?2:1;}
static void tracked_image(GLenum target,GLint level,GLint internal,GLsizei w,GLsizei h,GLint border,GLenum fmt,GLenum type,const void *data){
    if(count_calls&&data){measured_uploads++;measured_texbytes+=(unsigned long long)w*h*channels(fmt)*(type==GL_FLOAT?4:1);}
    glTexImage2D(target,level,internal,w,h,border,fmt,type,data);
}
static void tracked_sub(GLenum target,GLint level,GLint x,GLint y,GLsizei w,GLsizei h,GLenum fmt,GLenum type,const void *data){
    if(count_calls&&data){measured_uploads++;measured_texbytes+=(unsigned long long)w*h*channels(fmt)*(type==GL_FLOAT?4:1);}
    glTexSubImage2D(target,level,x,y,w,h,fmt,type,data);
}
static void tracked_buffer(GLenum target,GLsizeiptr bytes,const void *data,GLenum usage){
    if(count_calls&&data)measured_vertexbytes+=bytes;glBufferData(target,bytes,data,usage);
}
static int use_depth;
int tc2_depth_on(void){return use_depth;}
float tc2_depth_keep(void){return .98f;}
static uint8_t rom[8192*4096],attrs[2048*1024];
static uint32_t map[2048*1024];
void tc2_render_assets(const uint8_t **r,const uint32_t **m,const uint8_t **a){*r=rom;*m=map;*a=attrs;}
static void save_frame(FILE *f,int w,int h){
    uint8_t *p=malloc((size_t)w*h*4);assert(p);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,p);
    assert(glGetError()==GL_NO_ERROR);assert(fwrite(p,4,(size_t)w*h,f)==(size_t)w*h);free(p);
}
static void populate(tc2_built *b,int kind){
    memset(b,0,sizeof *b);b->fx.layers=kind%3==0?1:5;b->fx.bg=0x192c43;b->fx.alpha_pen=255;b->fx.alpha=kind%2?63:0;
    b->fx.alpha_color=1;b->fx.fr=32;b->fx.fg=64;b->fx.fb=96;b->fx.fflags=kind%4==1?1:0;b->fx.ffactor=kind%4==1?20:0;
    for(int i=0;i<256;i++){
        b->fs.gamma[0][i]=i;b->fs.gamma[1][i]=kind%5?i:i/2;b->fs.gamma[2][i]=i;
        b->fs.pens[i]=(uint32_t)i<<16|((uint32_t)(255-i)<<8)|(i/2);
    }
    memset(b->fs.sram,0xff,sizeof b->fs.sram);if(kind%5==2)memset(b->fs.sram,0x55,sizeof b->fs.sram);
    b->fs.c404[0x1f*2+1]=b->fx.layers;
    if(kind%3==1){
        for(int y=0;y<480;y++)for(int x=0;x<640;x++)if((x/24+y/24)%5==0)b->mix[y*640+x]=0x8000|64;
    }
    b->fs.list.count=b->npoly=4;
    for(int i=0;i<4;i++){
        tc2_render_entry *e=&b->fs.list.e[i];e->type=i==3?TC2_RE_DIRECT:TC2_RE_MODEL;e->model=1+i;e->vp_fov=772.53125f;
        tc2_poly *p=&b->polys[i];p->entry=i;p->model=e->model;p->n=i==2?5:4;p->vp_size_x=320;p->vp_size_y=240;p->index=i;
        p->color_base=0;p->prioverchar=i==3?7:2;p->blend=(i==1);p->alpha_enabled=(i==2);p->stencil=(i==2);p->zkey=(uint32_t)(4-i)*100;
        p->vp_offset_x=kind%4==2?32:0;p->vp_offset_y=kind%4==2?-16:0;e->vp_offset_x=p->vp_offset_x;e->vp_offset_y=p->vp_offset_y;
        const float xs[]={20,500,550,300,40},ys[]={25,35,330,450,400};
        for(int j=0;j<p->n;j++){
            p->pv[j].x=xs[j]+i*15;p->pv[j].y=ys[j]+i*8;
            float w=1.f/(4.f-i*.45f+j*.1f);p->pv[j].p[0]=w;
            p->pv[j].p[1]=(float)(j*13)*w;p->pv[j].p[2]=(float)(j*7+i*5)*w;p->pv[j].p[3]=(float)(32+i*8)*w;
        }
        b->order[i]=(uint32_t)i;
    }
}
#ifndef QTEST_CASES
#define QTEST_CASES 12
#endif
int main(int argc,char **argv){
    assert(argc==2);EGLDisplay d=eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL);
    assert(d!=EGL_NO_DISPLAY&&eglInitialize(d,NULL,NULL)&&eglBindAPI(EGL_OPENGL_ES_API));
    EGLint attr[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
    EGLConfig cfg;EGLint n;assert(eglChooseConfig(d,attr,&cfg,1,&n)&&n);
    EGLint sz[]={EGL_WIDTH,160,EGL_HEIGHT,120,EGL_NONE},ver[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    EGLSurface surface=eglCreatePbufferSurface(d,cfg,sz);EGLContext ctx=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ver);assert(eglMakeCurrent(d,surface,surface,ctx));
    for(int i=0;i<256;i++)rom[i]=(uint8_t)i;
    assert(tc2_gl_init());glViewport(0,0,160,120);
    tc2_built *b[2]={calloc(1,sizeof *b[0]),calloc(1,sizeof *b[0])};assert(b[0]&&b[1]);
    FILE *f=fopen(argv[1],"wb");assert(f);float v[16],p[16];projection(p,-.65f,.65f,-.5f,.5f);
    count_calls=1;
    for(int kind=0;kind<QTEST_CASES;kind++){
        tc2_built *frame=b[kind%2];populate(frame,kind);qvr_world_scale=1;qvr_view_distance=0;qvr_units_per_meter=1;qvr_flat_view=kind%2;
        use_depth=kind>=6;
        if(kind>=12){
            use_depth=kind%2;tc2_gl_set_filter((kind/2)%2);
            if(kind%5==0)frame->npoly=0; /* text-only frames */
            if(kind%5==1){frame->npoly=0;frame->fx.layers=0;} /* empty frames */
            frame->fx.fflags=1;frame->fx.ffactor=(kind*7)%256;
            frame->fx.pr=(kind*13)%256;frame->fx.pg=(kind*19)%256;frame->fx.pb=(kind*23)%256;
            frame->fx.alpha=(kind*11)%256;
            frame->fs.c404[0x1A*2+1]=2;frame->fs.c404[0x19*2+1]=kind%128;
            frame->fs.c404[0x15*2+1]=kind%64;
            for(int j=0;j<frame->npoly;j++)frame->polys[j].prioverchar=(kind+j)%8;
        }
        qvr_scene_prepare(frame);
        for(int eye=0;eye<2;eye++){
            view_matrix(v,v3(eye?.032f:-.032f,0,0),(Q4){0,0,0,1});
#ifndef QTEST_BASELINE
            QRenderStats before; qtc2_render_stats(&before);
#endif
            qtc2_draw(v,p,160,120,0);save_frame(f,160,120);
#ifndef QTEST_BASELINE
            QRenderStats after;qtc2_render_stats(&after);
            if(eye==1){assert(after.texture_uploads==before.texture_uploads);assert(after.geometry_uploads==before.geometry_uploads);}
#endif
        }
    }
    fclose(f);count_calls=0;fprintf(stderr,"TARGET_SWITCHES %llu\n",measured_switches);
    fprintf(stderr,"COMPARED texture_uploads=%llu texture_bytes=%llu buffer_bytes=%llu\n",measured_uploads,measured_texbytes,measured_vertexbytes);
#ifndef QTEST_BASELINE
    /* Actual render targets change, but full-resolution eye viewport is restored. */
    qvr_flat_view=0;populate(b[0],1);qvr_scene_prepare(b[0]);
    for(int percent=70;percent<=100;percent+=15){
        qtc2_set_scene_percent(percent);qtc2_draw(v,p,160,120,0);
        QRenderStats st;qtc2_render_stats(&st);assert(st.scene_width==(160*percent+99)/100&&st.scene_height==(120*percent+99)/100);
        GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);assert(viewport[2]==160&&viewport[3]==120);assert(glGetError()==GL_NO_ERROR);
    }
    /* A density change while depth is OFF must resize its lazy target when re-enabled. */
    use_depth=0;qtc2_set_scene_percent(70);qtc2_draw(v,p,160,120,0);
    use_depth=1;qtc2_draw(v,p,160,120,0);assert(near_width==112&&near_height==84);
    qtc2_set_scene_percent(100);qtc2_draw(v,p,160,120,0);
    qtc2_set_profiling(1);qtc2_draw(v,p,160,120,0);qtc2_set_profiling(0);
    assert(glGetError()==GL_NO_ERROR);
    /* In-place buffer publication must invalidate cache; unchanged data need no texture upload. */
    QRenderStats a,c;qtc2_render_stats(&a);qvr_scene_prepare(b[0]);qtc2_draw(v,p,160,120,0);qtc2_render_stats(&c);
    assert(c.texture_uploads==a.texture_uploads);
    b[0]->fs.pens[64]^=0x010101;qvr_scene_prepare(b[0]);qtc2_draw(v,p,160,120,0);QRenderStats z;qtc2_render_stats(&z);assert(z.texture_uploads==c.texture_uploads+1);
    qvr_flat_view=1;qvr_scene_prepare(b[0]);qtc2_set_scene_percent(70);qtc2_draw(v,p,160,120,0);qtc2_render_stats(&z);assert(z.scene_percent==100);
    fprintf(stderr,"RENDER vertices=%d triangles=%d uploads=%llu geometry_bytes=%llu draws=%llu\n",z.vertices,z.triangles,(unsigned long long)z.texture_uploads,(unsigned long long)z.geometry_bytes,(unsigned long long)z.draw_calls);
#endif
#ifndef QTEST_BASELINE
    qtc2_profile_close();
#endif
    free(b[0]);free(b[1]);eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,ctx);eglDestroySurface(d,surface);eglTerminate(d);
    puts("PASS: real GLES scene/priority/alpha/stencil/gamma/depth pipeline and full-density raster; opt-in scene density/cache tests");return 0;
}
