/* Real GLES test of the final scene resolve. Original synthetic image only. */
#define main pipeline_test_main
#include "quest_renderer_enhancement_fixture.c"
#undef main
static void resolve(GLuint src,GLuint protect,int smoothing,int mask,unsigned char *out){
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,96,96);glBindVertexArray(vao);
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);
    glUseProgram(prog_gamma);glUniform1i(ug[0],0);glUniform1i(ug[1],1);glUniform2f(ug[2],96,96);
    glUniform1i(ug[3],smoothing);glUniform1i(ug[4],mask);glUniform1i(ug[5],2);
    bind_tex(0,src);bind_tex(1,t_lut);bind_tex(2,protect);
    uint64_t draws=render_stats.draw_calls;draw_quad();assert(render_stats.draw_calls==draws+1);
    glReadPixels(0,0,96,96,GL_RGBA,GL_UNSIGNED_BYTE,out);assert(glGetError()==GL_NO_ERROR);
}
static void ppm(const char *name,const unsigned char *p){
    FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n96 96\n255\n");
    for(int y=95;y>=0;y--)for(int x=0;x<96;x++)assert(fwrite(p+4*(y*96+x),1,3,f)==3);fclose(f);
}
int main(void){
    EGLDisplay d=eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL);
    assert(d!=EGL_NO_DISPLAY&&eglInitialize(d,NULL,NULL)&&eglBindAPI(EGL_OPENGL_ES_API));
    EGLint attr[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
    EGLConfig cfg;EGLint n;assert(eglChooseConfig(d,attr,&cfg,1,&n)&&n);
    EGLint sz[]={EGL_WIDTH,96,EGL_HEIGHT,96,EGL_NONE},ver[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    EGLSurface s=eglCreatePbufferSurface(d,cfg,sz);EGLContext c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ver);
    assert(eglMakeCurrent(d,s,s,c)&&tc2_gl_init());
    static unsigned char src[96*96*4],mask[sizeof src],off[sizeof src],on[sizeof src],guarded[sizeof src];
    for(int y=0;y<96;y++)for(int x=0;x<96;x++){
        int pos=(y*96+x)*4;unsigned char shade=2*x+y>144?230:24;
        src[pos]=src[pos+1]=src[pos+2]=shade;src[pos+3]=255;
        mask[pos]=x>=25&&x<=72&&y>=44&&y<=50?255:0;mask[pos+3]=255;
    }
    unsigned char lut[256*3];for(int i=0;i<256;i++)for(int j=0;j<3;j++)lut[3*i+j]=(unsigned char)i;
    bind_tex(1,t_lut);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,1,GL_RGB,GL_UNSIGNED_BYTE,lut);
    GLuint a=texture(GL_RGBA8,96,96,GL_RGBA,src),m=texture(GL_RGBA8,96,96,GL_RGBA,mask);
    bind_tex(0,a);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    resolve(a,m,0,0,off);resolve(a,m,1,0,on);resolve(a,m,1,1,guarded);
    int changed=0,protected_pixels=0;
    for(int y=0;y<96;y++)for(int x=0;x<96;x++){
        int p=(y*96+x)*4;assert(!memcmp(src+p,off+p,3));
        if(memcmp(on+p,off+p,3))changed++;
        assert(on[p]>=24&&on[p]<=230); /* no added bright/dark halos */
        if(x>=25&&x<=72&&y>=44&&y<=50){assert(!memcmp(guarded+p,off+p,3));protected_pixels++;}
        if(abs(2*x+y-144)>5)assert(!memcmp(on+p,off+p,3));
    }
    assert(changed>50&&changed<600);assert(protected_pixels==336);
    ppm("edge-off.ppm",off);ppm("edge-on.ppm",on);ppm("edge-protected.ppm",guarded);
    /* A flat region, including its image boundaries, must remain exact. */
    for(int i=0;i<96*96;i++){src[4*i]=40;src[4*i+1]=60;src[4*i+2]=80;}
    bind_tex(0,a);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,96,96,GL_RGBA,GL_UNSIGNED_BYTE,src);
    resolve(a,m,1,0,on);for(int i=0;i<96*96;i++)assert(!memcmp(src+4*i,on+4*i,3));
    /* Nonidentity original gamma still applies after smoothing. */
    for(int i=0;i<256;i++){lut[3*i]=255-i;lut[3*i+1]=i/2;lut[3*i+2]=i;}
    bind_tex(1,t_lut);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,1,GL_RGB,GL_UNSIGNED_BYTE,lut);
    resolve(a,m,1,0,on);assert(on[0]==215&&on[1]==30&&on[2]==80);
    glDeleteTextures(1,&a);glDeleteTextures(1,&m);
    eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,c);eglDestroySurface(d,s);eglTerminate(d);
    printf("PASS: %d staircase-edge pixels softened; %d text-marked pixels exact; flat regions, gamma and one-pass resolve preserved\n",changed,protected_pixels);
    return 0;
}
