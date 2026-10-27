#include "quest_gpu_timer.h"
#include <assert.h>
#include <stdio.h>
/* Mock the query protocol, not the measured value. No GL runtime or clock is
 * needed to verify that unavailable/disjoint samples are never consumed. */
static bool ready,disjoint;static unsigned begins,ends,reads,deleted;
void GL_APIENTRY glGetIntegerv(GLenum p,GLint *v){assert(p==0x8FBB);*v=disjoint;}
const GLubyte *GL_APIENTRY glGetStringi(GLenum n,GLuint i){(void)n;(void)i;return NULL;}
void *SDL_GL_GetProcAddress(const char *name){(void)name;return NULL;}
static void GL_APIENTRY begin(GLenum target,GLuint id){assert(target==0x88BF&&id);begins++;}
static void GL_APIENTRY end(GLenum target){assert(target==0x88BF);ends++;}
static void GL_APIENTRY get(GLuint id,GLenum p,GLuint *value){assert(id&&p==0x8867);*value=ready;}
static void GL_APIENTRY get64(GLuint id,GLenum p,uint64_t *value){assert(ready&&!disjoint&&id&&p==0x8866);*value=2000000;reads++;}
static void GL_APIENTRY del(GLsizei n,const GLuint *ids){assert(n==8&&ids[0]);deleted++;}
int main(void){
    QGpuTimer q={.initialized=true,.available=true,.active=-1,.begin=begin,.end=end,.get=get,.get64=get64,.del=del};for(int i=0;i<8;i++)q.ids[i]=i+1;
    qgpu_timer_begin(&q);assert(!begins);qgpu_timer_enable(&q,true);
    for(int i=0;i<8;i++){qgpu_timer_begin(&q);qgpu_timer_end(&q);}assert(begins==8&&ends==8&&!reads);
    qgpu_timer_begin(&q);assert(q.active==-1&&begins==8&&!reads); /* all pending: skip */
    ready=true;qgpu_timer_begin(&q);assert(reads==8&&q.samples==8&&q.milliseconds==2.0);qgpu_timer_end(&q);
    disjoint=true;qgpu_timer_begin(&q);assert(q.samples==0&&reads==8);qgpu_timer_end(&q);
    disjoint=false;qgpu_timer_enable(&q,false);qgpu_timer_begin(&q);assert(reads==8);
    qgpu_timer_enable(&q,true);qgpu_timer_begin(&q);assert(reads==8);qgpu_timer_end(&q);
    qgpu_timer_begin(&q);assert(reads==9&&q.samples==1);qgpu_timer_end(&q);
    qgpu_timer_close(&q);assert(deleted==1&&!q.available&&q.active==-1);
    puts("PASS: asynchronous GPU timer ring never reads an unavailable result, drops disjoint/disabled samples, skips full ring and cleans up");
}
