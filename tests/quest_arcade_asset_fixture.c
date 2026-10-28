#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "quest_gpu.h"
#include "quest_gun.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static V3 norm(V3 v){return mul(v,1/sqrtf(dot(v,v)));}
static void look(float *m,V3 eye,V3 target){
 V3 z=norm(sub(eye,target)),x=norm(cross(v3(0,1,0),z)),y=cross(z,x);
 float a[16]={x.x,y.x,z.x,0,x.y,y.y,z.y,0,x.z,y.z,z.z,0,-dot(x,eye),-dot(y,eye),-dot(z,eye),1};
 memcpy(m,a,sizeof a);
}
int main(int argc,char **argv){
 assert(argc==3);EGLDisplay d=eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL);
 assert(eglInitialize(d,0,0)&&eglBindAPI(EGL_OPENGL_ES_API));EGLConfig cfg;EGLint n;
 EGLint a[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
 assert(eglChooseConfig(d,a,&cfg,1,&n)&&n);EGLint sz[]={EGL_WIDTH,800,EGL_HEIGHT,600,EGL_NONE},ver[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
 EGLSurface s=eglCreatePbufferSurface(d,cfg,sz);EGLContext c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ver);assert(eglMakeCurrent(d,s,s,c));
 assert(qgun_init(argv[1]));
 /* Load all render slots; this does not enable shotgun gameplay. */
 assert(qgun_load_weapon(QGUN_MACHINE_GUN,argv[1]));assert(qgun_load_weapon(QGUN_SHOTGUN,argv[1]));
 qgun_select(QGUN_MACHINE_GUN);static unsigned char pixels[800*600*4];
 V3 eyes[]={ { .54f,.02f,-.10f },{.32f,.18f,.35f},{-.32f,.18f,.35f},{.018f,.085f,.26f} };
 for(int i=0;i<4;i++){
 float v[16],p[16];look(v,eyes[i],i==3?v3(0,.03f,-.50f):v3(0,-.04f,-.10f));projection(p,-.52f,.52f,-.39f,.39f);
 glViewport(0,0,800,600);glClearColor(.07f,.085f,.11f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 qgun_draw(v,p,v3(0,0,0),(Q4){0,0,0,1},0,0,0);assert(glGetError()==GL_NO_ERROR);
 glReadPixels(0,0,800,600,GL_RGBA,GL_UNSIGNED_BYTE,pixels);assert(glGetError()==GL_NO_ERROR);
 char name[2048];snprintf(name,sizeof name,"%s-%d.ppm",argv[2],i);FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n800 600\n255\n");
 int foreground=0;for(int y=599;y>=0;y--)for(int x=0;x<800;x++){unsigned char *px=pixels+4*(y*800+x);fwrite(px,1,3,f);foreground+=px[0]>40||px[1]>50||px[2]>60;}fclose(f);assert(foreground>1000);
 }
 /* Both hands and arbitrary rotations preserve the stored muzzle position. */
 V3 origin=qgun_muzzle(v3(0,0,0),(Q4){0,0,0,1});
 for(int hand=-1;hand<=1;hand+=2){
     Q4 rot={0,sinf(.4f),0,cosf(.4f)};V3 p=v3(hand*.2f,-.2f,-.4f);
     V3 a=qgun_muzzle(p,rot),b=add(p,rotate(rot,origin));assert(dot(sub(a,b),sub(a,b))<1e-10f);
 }
 for(int kind=0;kind<QGUN_COUNT;kind++){
     qgun_select(kind);assert(qgun_selected()==kind);
     float v[16],p[16];look(v,eyes[1],v3(0,-.04f,-.1f));projection(p,-.52f,.52f,-.39f,.39f);
     qgun_draw(v,p,v3(0,0,0),(Q4){0,0,0,1},.5f,1,1);assert(glGetError()==GL_NO_ERROR);
 }
 qgun_shutdown();for(int kind=0;kind<QGUN_COUNT;kind++)assert(!qgun_available(kind));
 eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,c);eglDestroySurface(d,s);eglTerminate(d);puts("PASS: four production-GLES asset views and cleanup");
}
