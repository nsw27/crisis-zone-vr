/* Real GLES tests. Synthetic background, not a gameplay capture or comfort test. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "quest_gpu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned draws;
static void border_draw_spy(GLenum mode,GLint first,GLsizei count){draws++;glDrawArrays(mode,first,count);}
#define glDrawArrays border_draw_spy
#include "../quest/quest_ui.c"
#undef glDrawArrays
int qvr_menu_selected(void){return 5;}
static unsigned char images[4][640*480*4];
static void clear(void){
 glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glClearColor(.65f,.70f,.75f,.625f);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}
static void picture(int which){glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,images[which]);assert(glGetError()==GL_NO_ERROR);}
static void capture(const char *name,int which){FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n640 480\n255\n");for(int y=479;y>=0;y--)for(int x=0;x<640;x++)fwrite(images[which]+4*(y*640+x),1,3,f);fclose(f);}
int main(void){
 EGLDisplay d=eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL);assert(eglInitialize(d,0,0)&&eglBindAPI(EGL_OPENGL_ES_API));
 EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
 EGLConfig cfg;EGLint n;assert(eglChooseConfig(d,attrs,&cfg,1,&n)&&n);
 EGLint size[]={EGL_WIDTH,640,EGL_HEIGHT,480,EGL_NONE},ver[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
 EGLSurface surf=eglCreatePbufferSurface(d,cfg,size);EGLContext ctx=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ver);assert(eglMakeCurrent(d,surf,surf,ctx));
 assert(qui_init());glViewport(0,0,640,480);
 float p[16];projection(p,-.95f,.95f,-.90f,.90f);float saved[16];memcpy(saved,p,sizeof p);
 Q4 q={0,0,0,1};clear();picture(0);
 qui_draw_border(p,q,0);picture(1);assert(draws==0&&!border_attempted&&!memcmp(images[0],images[1],sizeof images[0]));
 for(int mode=1;mode<=2;mode++){
  clear();unsigned before=draws;qui_draw_border(p,q,mode);picture(mode);
  assert(draws==before+1);assert(!memcmp(p,saved,sizeof p));
  unsigned changed=0,central=0;
  for(int y=0;y<480;y++)for(int x=0;x<640;x++){
   int i=4*(y*640+x);assert(images[mode][i+3]==images[0][i+3]);
   bool diff=memcmp(images[mode]+i,images[0]+i,3)!=0;changed+=diff;
   if(x>=200&&x<440&&y>=130&&y<350){assert(!diff);central++;}
  }
  assert(changed>10000&&changed<640*480*3/4&&central==240*220);
  capture(mode==1?"border-soft.ppm":"border-goggles.ppm",mode);
 }
 assert(memcmp(images[1],images[2],sizeof images[1])!=0);
 /* Equal angle rays agree between the two shifted optical centres. For these
  * symmetric frusta the result must be horizontally mirrored, not the same
  * screen-centred stamped mask. */
 p[8]=.125f;clear();qui_draw_border(p,q,2);picture(1);
 p[8]=-.125f;clear();qui_draw_border(p,q,2);picture(2);
 assert(memcmp(images[1],images[2],sizeof images[1])!=0);
 for(int y=0;y<480;y++)for(int x=0;x<640;x++)for(int k=0;k<4;k++){
  int delta=images[1][4*(y*640+x)+k]-images[2][4*(y*640+639-x)+k];assert(abs(delta)<=1);
 }
 /* Eye cant is transformed back into the common head frame. */
 p[8]=0;Q4 yaw={0,sinf(.07f),0,cosf(.07f)};clear();qui_draw_border(p,yaw,2);picture(1);
 yaw.y=-yaw.y;clear();qui_draw_border(p,yaw,2);picture(2);
 for(int y=0;y<480;y++)for(int x=0;x<640;x++)for(int k=0;k<3;k++)assert(abs(images[1][4*(y*640+x)+k]-images[2][4*(y*640+639-x)+k])<=1);
 /* Invalid inputs and OFF leave the framebuffer untouched and do no draw. */
 for(int k=0;k<7;k++){
  memcpy(p,saved,sizeof p);q=(Q4){0,0,0,1};int mode=2;
  if(k==0)p[0]=0;if(k==1)p[9]=NAN;if(k==2)q.w=0;if(k==3)q.x=INFINITY;
  if(k==4)mode=0;if(k==5)mode=3;if(k==6)p[11]=0;
  unsigned before=draws;clear();qui_draw_border(p,q,mode);picture(3);assert(draws==before&&!memcmp(images[0],images[3],sizeof images[0]));
 }
 /* All modified GL state is restored; viewport, framebuffer, depth mask,
  * texture bindings and buffer bindings are not touched at all. */
 memcpy(p,saved,sizeof p);q=(Q4){0,0,0,1};glUseProgram(program);glBindVertexArray(vao);
 glEnable(GL_SCISSOR_TEST);glScissor(2,3,4,5);glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);
 glDepthMask(GL_FALSE);glColorMask(1,0,1,0);glBlendFuncSeparate(GL_ONE,GL_ZERO,GL_ONE,GL_ZERO);glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_MAX);
 qui_draw_border(p,q,2);GLint v;GLboolean b[4];
 assert(glIsEnabled(GL_SCISSOR_TEST)&&glIsEnabled(GL_DEPTH_TEST)&&glIsEnabled(GL_CULL_FACE)&&!glIsEnabled(GL_BLEND));
 glGetIntegerv(GL_CURRENT_PROGRAM,&v);assert(v==(GLint)program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&v);assert(v==(GLint)vao);
 glGetBooleanv(GL_COLOR_WRITEMASK,b);assert(b[0]&&!b[1]&&b[2]&&!b[3]);glGetBooleanv(GL_DEPTH_WRITEMASK,b);assert(!b[0]);
 glGetIntegerv(GL_BLEND_EQUATION_RGB,&v);assert(v==GL_FUNC_REVERSE_SUBTRACT);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&v);assert(v==GL_MAX);
 glGetIntegerv(GL_BLEND_SRC_RGB,&v);assert(v==GL_ONE);glGetIntegerv(GL_BLEND_DST_RGB,&v);assert(v==GL_ZERO);
 glGetIntegerv(GL_BLEND_SRC_ALPHA,&v);assert(v==GL_ONE);glGetIntegerv(GL_BLEND_DST_ALPHA,&v);assert(v==GL_ZERO);
 assert(glGetError()==GL_NO_ERROR);qui_shutdown();assert(!border_program&&!border_vao&&!border_attempted);
 /* A fresh context/UI initialization may recreate the optional resources. */
 assert(qui_init());qui_draw_border(p,q,1);assert(border_program&&border_vao);qui_shutdown();
 eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,ctx);eglDestroySurface(d,surf);eglTerminate(d);
 QOptions o;qoptions_load("missing.cfg",&o);assert(o.view_border==0);
 for(int mode=0;mode<3;mode++){o.view_border=mode;assert(qoptions_save("border.cfg",&o));qoptions_load("border.cfg",&o);assert(o.view_border==mode);}
 FILE *f=fopen("border.cfg","w");assert(f);fputs("view_border=-1\nview_border=3\nview_border=1oops\nview_border=9999999999999999999999\n",f);fclose(f);
 qoptions_load("border.cfg",&o);assert(o.view_border==0);
 puts("PASS: optional one-draw border; central image/alpha/projection unchanged, asymmetric stereo and eye rotation, invalid inputs, GL state restoration, cleanup and saved settings");
}
