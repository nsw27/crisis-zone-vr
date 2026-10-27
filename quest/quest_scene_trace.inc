/* Main-thread diagnostic capture. It does not modify spawning or culling.
 * At most 600 published frames / 65536 entry records (~3 MB memory).
 * The single bounded local CSV is written only while paused or on shutdown.
 * Model IDs are not entity IDs: duplicated models must be distinguished using
 * their frame-local entry and translation. Nothing is uploaded automatically. */
enum { TRACE_FRAMES=600,TRACE_ROWS=65536 };
typedef struct {unsigned frame;unsigned short entry,model,polygons,flags;int xyz[3];float bounds[4];} TraceRow;
typedef struct {unsigned entries,polygons,start,count,truncated;} TraceFrame;
static TraceRow *trace_rows;
static TraceFrame trace_frames[TRACE_FRAMES];
static unsigned trace_nframes,trace_nrows;
static bool trace_active,trace_pending;
bool qvr_trace_active(void){return trace_active;}
void qvr_trace_set(bool enabled){
    if(!enabled){trace_active=false;trace_pending=trace_nframes>0;return;}
    if(trace_active)return;
    if(!trace_rows)trace_rows=calloc(TRACE_ROWS,sizeof *trace_rows);
    if(!trace_rows){fprintf(stderr,"[TRACE] allocation failed; capture not started\n");return;}
    trace_nframes=trace_nrows=0;trace_pending=false;trace_active=true;
    fprintf(stderr,"[TRACE] capturing up to 600 scenes / 65536 render entries; pause after the event to write the local CSV\n");
}
static void qvr_trace_capture(const tc2_built *b){
    if(!trace_active||!b)return;
    unsigned short counts[TC2_RENDER_MAX]={0};float bounds[TC2_RENDER_MAX][4];
    for(int i=0;i<TC2_RENDER_MAX;i++){bounds[i][0]=bounds[i][1]=FLT_MAX;bounds[i][2]=bounds[i][3]=-FLT_MAX;}
    for(int i=0;i<b->npoly&&i<TC2_MAX_POLYS;i++){
        const tc2_poly *p=&b->polys[i];int e=p->entry;if(e<0||e>=TC2_RENDER_MAX)continue;
        if(counts[e]!=65535)counts[e]++;
        for(int j=0;j<p->n&&j<8;j++){
            float x=p->pv[j].x+320-p->vp_size_x+p->vp_offset_x,y=p->pv[j].y+240-p->vp_size_y-p->vp_offset_y;
            if(!isfinite(x)||!isfinite(y))continue;
            bounds[e][0]=fminf(bounds[e][0],x);bounds[e][1]=fminf(bounds[e][1],y);
            bounds[e][2]=fmaxf(bounds[e][2],x);bounds[e][3]=fmaxf(bounds[e][3],y);
        }
    }
    TraceFrame *f=&trace_frames[trace_nframes];*f=(TraceFrame){.entries=b->fs.list.count,.polygons=b->npoly,.start=trace_nrows};
    for(int i=0;i<b->fs.list.count&&i<TC2_RENDER_MAX;i++){
        const tc2_render_entry *e=&b->fs.list.e[i];if(e->type!=TC2_RE_MODEL)continue;
        if(trace_nrows==TRACE_ROWS){f->truncated++;continue;}
        TraceRow *r=&trace_rows[trace_nrows++];*r=(TraceRow){.frame=trace_nframes,.entry=i,.model=e->model,.polygons=counts[i]};
        memcpy(r->xyz,e->v,sizeof r->xyz);
        bool valid=counts[i]&&bounds[i][0]!=FLT_MAX;
        if(valid){memcpy(r->bounds,bounds[i],sizeof r->bounds);r->flags=1;}
        f->count++;
    }
    trace_nframes++;
    if(trace_nframes==TRACE_FRAMES||trace_nrows==TRACE_ROWS){trace_active=false;trace_pending=true;}
}
void qvr_trace_flush(void){
    if(!trace_pending)return;
    FILE *out=fopen("quest-scene-trace.csv","w");
    if(!out){fprintf(stderr,"[TRACE] cannot write quest-scene-trace.csv\n");trace_pending=false;return;}
    int ok=fprintf(out,"# bounded render-list trace; no entity identity or spawning diagnosis inferred\n# frames=%u,records=%u\nframe,entry,model,polygons,x,y,z,bounds_valid,left,top,right,bottom\n",trace_nframes,trace_nrows)>0;
    for(unsigned i=0;i<trace_nframes&&ok;i++){
        TraceFrame *f=&trace_frames[i];ok=fprintf(out,"# frame=%u,submitted_entries=%u,built_polygons=%u,truncated_model_records=%u\n",i,f->entries,f->polygons,f->truncated)>0;
        for(unsigned j=f->start;j<f->start+f->count&&ok;j++){
            TraceRow *r=&trace_rows[j];ok=fprintf(out,"%u,%u,%u,%u,%d,%d,%d,%u,%.3f,%.3f,%.3f,%.3f\n",r->frame,r->entry,r->model,r->polygons,r->xyz[0],r->xyz[1],r->xyz[2],r->flags,r->bounds[0],r->bounds[1],r->bounds[2],r->bounds[3])>0;
        }
    }
    if(fclose(out))ok=0;
    fprintf(stderr,"[TRACE] %s: quest-scene-trace.csv (%u frames, %u entries)\n",ok?"saved":"write failed",trace_nframes,trace_nrows);
    trace_pending=false;trace_nframes=trace_nrows=0;
}
