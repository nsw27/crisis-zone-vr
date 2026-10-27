#pragma once
/* Optional, non-blocking GLES timer queries. A CPU submission timer is not GPU
 * execution time. Results are read only after AVAILABLE; disjoint samples are
 * discarded. The fallback is explicitly unavailable, never a fabricated 0 ms. */
#include "quest_gpu.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
typedef struct {
    void (GL_APIENTRY *gen)(GLsizei,GLuint*);
    void (GL_APIENTRY *del)(GLsizei,const GLuint*);
    void (GL_APIENTRY *begin)(GLenum,GLuint);
    void (GL_APIENTRY *end)(GLenum);
    void (GL_APIENTRY *get)(GLuint,GLenum,GLuint*);
    void (GL_APIENTRY *query)(GLenum,GLenum,GLint*);
    void (GL_APIENTRY *get64)(GLuint,GLenum,uint64_t*);
    GLuint ids[8];bool pending[8],discard[8],initialized,available,enabled;
    int active;double milliseconds;uint64_t samples;
} QGpuTimer;
static void qgpu_timer_init(QGpuTimer *q){
    if(q->initialized)return;q->initialized=true;q->active=-1;
    /* PCVR uses different timer-query rules; it remains an explicit n/a here. */
#ifndef TCVR_PC
    GLint count=0;glGetIntegerv(GL_NUM_EXTENSIONS,&count);bool supported=false;
    for(int i=0;i<count;i++){const char *s=(const char*)glGetStringi(GL_EXTENSIONS,(GLuint)i);if(s&&!strcmp(s,"GL_EXT_disjoint_timer_query"))supported=true;}
    if(!supported)return;
    q->gen=(void(GL_APIENTRY*)(GLsizei,GLuint*))SDL_GL_GetProcAddress("glGenQueriesEXT");
    q->del=(void(GL_APIENTRY*)(GLsizei,const GLuint*))SDL_GL_GetProcAddress("glDeleteQueriesEXT");
    q->begin=(void(GL_APIENTRY*)(GLenum,GLuint))SDL_GL_GetProcAddress("glBeginQueryEXT");
    q->end=(void(GL_APIENTRY*)(GLenum))SDL_GL_GetProcAddress("glEndQueryEXT");
    q->get=(void(GL_APIENTRY*)(GLuint,GLenum,GLuint*))SDL_GL_GetProcAddress("glGetQueryObjectuivEXT");
    q->get64=(void(GL_APIENTRY*)(GLuint,GLenum,uint64_t*))SDL_GL_GetProcAddress("glGetQueryObjectui64vEXT");
    q->query=(void(GL_APIENTRY*)(GLenum,GLenum,GLint*))SDL_GL_GetProcAddress("glGetQueryivEXT");
    if(!q->gen||!q->del||!q->begin||!q->end||!q->get||!q->get64||!q->query)return;
    GLint bits=0;q->query(0x88BF,0x8864,&bits);if(bits<=0)return;
    q->gen(8,q->ids);q->available=true;
#endif
}
static void qgpu_timer_begin(QGpuTimer *q){
    if(!q->enabled)return;qgpu_timer_init(q);if(!q->available)return;
    GLint disjoint=0;glGetIntegerv(0x8FBB,&disjoint);
    if(disjoint){q->samples=0;q->milliseconds=0;for(int i=0;i<8;i++)if(q->pending[i])q->discard[i]=true;}
    for(int i=0;i<8;i++)if(q->pending[i]){
        GLuint ready=0;q->get(q->ids[i],0x8867,&ready);
        if(!ready)continue;
        if(!q->discard[i]){uint64_t ns=0;q->get64(q->ids[i],0x8866,&ns);double ms=(double)ns/1e6;
            q->milliseconds=q->samples?.9*q->milliseconds+.1*ms:ms;q->samples++;}
        q->pending[i]=q->discard[i]=false;
    }
    for(int i=0;i<8;i++)if(!q->pending[i]){q->begin(0x88BF,q->ids[i]);q->active=i;return;}
    /* All eight queries pending: skip this sample, never wait for the GPU. */
    q->active=-1;
}
static void qgpu_timer_end(QGpuTimer *q){
    if(q->active<0||!q->available)return;
    q->end(0x88BF);q->pending[q->active]=true;q->active=-1;
}
static void qgpu_timer_enable(QGpuTimer *q,bool enabled){
    if(q->enabled==enabled)return;q->enabled=enabled;q->samples=0;q->milliseconds=0;
    for(int i=0;i<8;i++)if(q->pending[i])q->discard[i]=true;
}
static void qgpu_timer_close(QGpuTimer *q){
    if(q->available){qgpu_timer_end(q);q->del(8,q->ids);}*q=(QGpuTimer){.active=-1};
}
