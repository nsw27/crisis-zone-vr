#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "quest_gpu.h"
#include "quest_ui.h"
#include "quest_gun.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static int selected;
int qvr_menu_selected(void){return selected;}
static unsigned char pixels[640*480*4];
static unsigned long image_hash(void){glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels);unsigned long h=5381;for(unsigned i=0;i<sizeof pixels;i++)h=(h*33)^pixels[i];return h;}
int main(int argc,char **argv){
    assert(argc==2);
    EGLDisplay d=eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL);assert(d!=EGL_NO_DISPLAY);
    assert(eglInitialize(d,NULL,NULL));assert(eglBindAPI(EGL_OPENGL_ES_API));
    EGLint attr[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
    EGLConfig cfg;EGLint n;assert(eglChooseConfig(d,attr,&cfg,1,&n)&&n);
    EGLint size[]={EGL_WIDTH,640,EGL_HEIGHT,480,EGL_NONE},ver[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    EGLSurface s=eglCreatePbufferSurface(d,cfg,size);EGLContext c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ver);assert(eglMakeCurrent(d,s,s,c));
    QOptions o;qoptions_load("missing-options.cfg",&o);assert(qui_init());qui_set_resolution(0,1360,1440);
    float v[16],p[16];projection(p,-.65f,.65f,-.65f,.65f);glViewport(0,0,640,480);
    for(int eye=0;eye<2;eye++){
        view_matrix(v,v3(eye?.032f:-.032f,0,0),(Q4){0,0,0,1});
        for(int page=0;page<QOPTIONS_PAGE_COUNT;page++){
            unsigned long last=0;
            for(selected=0;selected<qoptions_row_count(page);selected++){
                glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                qui_draw(v,p,v3(0,0,0),(Q4){0,0,0,1},&o,page,1,true);
                unsigned long hash=image_hash();assert(hash!=last);last=hash;assert(glGetError()==GL_NO_ERROR);
                if(eye==0&&selected==0){char name[64];snprintf(name,64,"panel-%d.ppm",page);FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n640 480\n255\n");for(int y=479;y>=0;y--)for(int x=0;x<640;x++)fwrite(pixels+(y*640+x)*4,1,3,f);fclose(f);}
            }
        }
    }
    /* Changing the feature must invalidate the cached panel texture without
     * moving the highlighted row. */
    selected=3;glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    qui_draw(v,p,v3(0,0,0),(Q4){0,0,0,1},&o,QOPTIONS_PAGE_HANDLING,1,true);
    unsigned long before_mode=image_hash();o.expanded_weapons=!o.expanded_weapons;
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    qui_draw(v,p,v3(0,0,0),(Q4){0,0,0,1},&o,QOPTIONS_PAGE_HANDLING,1,true);
    assert(image_hash()!=before_mode);o.expanded_weapons=false;
    selected=5;
    for(int mode=0;mode<3;mode++){
        unsigned long before=image_hash();o.view_border=mode;
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);qui_draw(v,p,v3(0,0,0),(Q4){0,0,0,1},&o,QOPTIONS_PAGE_HANDLING,1,true);
        assert(image_hash()!=before);
    }
    /* Render pixel-aligned panel: both actual page buttons must be visible,
     * including when the bundled font has no less/greater-than glyphs. */
    float av[16]={0},ap[16]={0};av[0]=av[5]=av[10]=av[15]=1;
    ap[0]=1/.65f;ap[5]=1/.4875f;ap[15]=1;selected=0;
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    qui_draw(av,ap,v3(0,0,0),(Q4){0,0,0,1},&o,2,1,true);image_hash();
    for(int side=0;side<2;side++){
        int gold=0,left=side?584:16;
        for(int y=42;y<76;y++)for(int x=left;x<left+40;x++){
            const unsigned char *px=pixels+((479-y)*640+x)*4;
            if(px[0]>180&&px[1]>140&&px[2]<160)gold++;
        }
        assert(gold>=20);
    }
    /* Cursor is a uniform, not a per-frame upload of the entire menu texture. */
    selected=0;qui_set_pointer(100,100,false);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    qui_draw(v,p,v3(0,0,0),(Q4){0,0,0,1},&o,2,1,true);unsigned long no_pointer=image_hash();
    qui_set_pointer(100,100,true);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    qui_draw(v,p,v3(0,0,0),(Q4){0,0,0,1},&o,2,1,true);assert(image_hash()!=no_pointer);
    qui_set_pointer(NAN,100,true);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    qui_draw(v,p,v3(0,0,0),(Q4){0,0,0,1},&o,2,1,true);assert(image_hash()==no_pointer);
    qui_metrics_update("SCENE CPU 1.2 MS/EYE | GPU n/a MS/EYE","RENDER 90 FPS | GAME 59.9 FPS","SCENE 1120 X 1080 (85%) | TRI 1400","UPLOAD 1234 KB/S | WORLD DRAWS 12/FRAME");
    qui_draw_metrics(v,p,v3(0,0,0),(Q4){0,0,0,1});assert(glGetError()==GL_NO_ERROR);
    char file[2048];snprintf(file,sizeof file,"%s/quest/assets/models/namco-gun.tcgun",argv[1]);assert(qgun_init(file));
    assert(!qgun_available(QGUN_MACHINE_GUN));qgun_select(QGUN_MACHINE_GUN);assert(qgun_selected()==QGUN_PISTOL);
    snprintf(file,sizeof file,"%s/quest/assets/models/machine-gun.tcgun",argv[1]);assert(qgun_load_weapon(QGUN_MACHINE_GUN,file));
    for(int kind=0;kind<2;kind++){
        qgun_select(kind);assert(qgun_selected()==(QGunKind)kind);
        for(int hand=-1;hand<=1;hand+=2){
            qgun_draw(v,p,v3(hand*.2f,-.2f,-.5f),(Q4){0,0,0,1},.6f,1,1);
            V3 tip=qgun_muzzle(v3(hand*.2f,-.2f,-.5f),(Q4){0,0,0,1});assert(isfinite(tip.x)&&isfinite(tip.y)&&tip.z<0);assert(glGetError()==GL_NO_ERROR);
        }
    }
    qgun_shutdown();qui_shutdown();eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,c);eglDestroySurface(d,s);eglTerminate(d);
    int rows=0;for(int page=0;page<QOPTIONS_PAGE_COUNT;page++)rows+=qoptions_row_count(page);
    printf("PASS: all %d menu rows in both stereo eyes, selection invalidation, both weapon meshes, missing-model fallback, muzzle transforms and GLES errors\n",rows);return 0;
}
