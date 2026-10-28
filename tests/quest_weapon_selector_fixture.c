/* Real GLES rendering plus texture-upload counters. No game/device measurements. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "quest_gpu.h"
#include <assert.h>
#include <stdio.h>
static unsigned full_uploads,small_uploads,draw_calls;
static void image_spy(GLenum a,GLint b,GLint c,GLsizei d,GLsizei e,GLint f,GLenum g,GLenum h,const void *i){
    full_uploads++;glTexImage2D(a,b,c,d,e,f,g,h,i);
}
static void subimage_spy(GLenum a,GLint b,GLint c,GLint d,GLsizei e,GLsizei f,GLenum g,GLenum h,const void *i){
    small_uploads++;glTexSubImage2D(a,b,c,d,e,f,g,h,i);
}
static void draw_spy(GLenum a,GLint b,GLsizei c){draw_calls++;glDrawArrays(a,b,c);}
#define glTexImage2D image_spy
#define glTexSubImage2D subimage_spy
#define glDrawArrays draw_spy
#include "../quest/quest_ui.c"
#undef glTexImage2D
#undef glTexSubImage2D
#undef glDrawArrays
int qvr_menu_selected(void){return 0;}
static unsigned char frame[640*480*4];
static unsigned long picture(void){
    glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,frame);
    unsigned long hash=5381;for(unsigned i=0;i<sizeof frame;i++)hash=(hash*33)^frame[i];return hash;
}
static void clear(void){glClearColor(.25f,.25f,.25f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
int main(void){
    EGLDisplay d=eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL);assert(d!=EGL_NO_DISPLAY);
    assert(eglInitialize(d,NULL,NULL)&&eglBindAPI(EGL_OPENGL_ES_API));
    EGLint attr[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
    EGLConfig cfg;EGLint n;assert(eglChooseConfig(d,attr,&cfg,1,&n)&&n);
    EGLint size[]={EGL_WIDTH,640,EGL_HEIGHT,480,EGL_NONE},ver[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    EGLSurface surf=eglCreatePbufferSurface(d,cfg,size);EGLContext c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ver);
    assert(eglMakeCurrent(d,surf,surf,c));assert(qui_init());glViewport(0,0,640,480);
    float v[16],p[16];projection(p,-.65f,.65f,-.65f,.65f);Q4 rot={0,0,0,1};V3 head=v3(0,0,0);
    QWeapons s={.enabled=true};qweapons_new_game(&s);
    clear();unsigned long blank=picture();view_matrix(v,v3(0,0,0),rot);
    qui_weapons_update(&s,false);qui_draw_weapons(v,p,head,rot);assert(picture()==blank&&!draw_calls&&!full_uploads);
    unsigned long first=0;
    for(int eye=0;eye<2;eye++){
        view_matrix(v,v3(eye?.032f:-.032f,0,0),rot);clear();
        qui_weapons_update(&s,true);qui_draw_weapons(v,p,head,rot);
        unsigned long hash=picture();assert(hash!=blank);if(!eye)first=hash;else assert(hash!=first);
        assert(full_uploads==1&&!small_uploads&&draw_calls==(unsigned)eye+1);
        /* Centre is untouched: this is not a second aiming marker/shield. */
        for(int y=210;y<270;y++)for(int x=290;x<350;x++){
            unsigned char *px=frame+4*(y*640+x);assert(px[0]>=63&&px[0]<=65&&px[1]==px[0]&&px[2]==px[0]);
        }
        assert(glGetError()==GL_NO_ERROR);
    }
    s.ammo[0]=37;qui_weapons_update(&s,true);clear();qui_draw_weapons(v,p,head,rot);
    assert(full_uploads==1&&small_uploads==1);unsigned long ammo=picture();
    s.requested=QWEAPON_HANDGUN;qui_weapons_update(&s,true);clear();qui_draw_weapons(v,p,head,rot);assert(picture()!=ammo&&small_uploads==2);
    s.selected=QWEAPON_HANDGUN;qui_weapons_update(&s,true);clear();qui_draw_weapons(v,p,head,rot);assert(small_uploads==3);
    s.ammo[1]=0;qui_weapons_update(&s,true);clear();qui_draw_weapons(v,p,head,rot);assert(small_uploads==4);
    unsigned long empty=picture();
    for(int i=0;i<20;i++){qui_weapons_update(&s,true);clear();qui_draw_weapons(v,p,head,rot);assert(picture()==empty);}
    assert(full_uploads==1&&small_uploads==4);
    /* Invalid/unavailable states are hidden, never clamped into made-up ammo. */
    for(int i=0;i<6;i++){
        QWeapons bad=s;if(i==0)bad.ready=false;if(i==1)bad.enabled=false;if(i==2)bad.fault=true;
        if(i==3)bad.ammo[1]=13;if(i==4)bad.selected=QWEAPON_COUNT;if(i==5)bad.requested=QWEAPON_COUNT;
        unsigned draws=draw_calls;qui_weapons_update(&bad,true);clear();qui_draw_weapons(v,p,head,rot);assert(picture()==blank&&draw_calls==draws);
    }
    qui_weapons_update(NULL,true);clear();qui_draw_weapons(v,p,head,rot);assert(picture()==blank);
    /* Direct texture preview uses the same production raster, not a mockup. */
    s.ammo[1]=12;qui_weapons_update(&s,true);qui_draw_weapons(v,p,head,rot);
    FILE *out=fopen("cover-selector.ppm","wb");assert(out);fprintf(out,"P6\n640 128\n255\n");
    for(int i=0;i<640*128;i++)fwrite(weapon_selector_pixels+4*i,1,3,out);fclose(out);
    qui_shutdown();assert(!weapon_selector_texture&&!weapon_selector_visible);
    assert(glGetError()==GL_NO_ERROR);eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroyContext(d,c);eglDestroySurface(d,surf);eglTerminate(d);
    puts("PASS: stereo cover selector, confirmed/requested magazines, empty/invalid states, no central marker, cached uploads and cleanup");
}
