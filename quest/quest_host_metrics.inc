/* Called once per visible, focused headset frame, not per eye. */
static void update_metrics(bool visible){
    static uint64_t start,frames,scene_start,bytes_start,draw_start;
    if(!visible||!options.diagnostics){start=frames=0;return;}
    uint64_t now=SDL_GetTicks64();QRenderStats stats;qtc2_render_stats(&stats);
    if(!start){start=now;scene_start=qvr_scene_serial();bytes_start=stats.texture_bytes+stats.geometry_bytes;draw_start=stats.draw_calls;frames=0;return;}
    frames++;
    if(now-start<500)return;
    double seconds=(now-start)/1000.0;char line[4][64],gpu[24];
    if(stats.gpu_supported&&stats.gpu_samples)snprintf(gpu,sizeof gpu,"%.2f",stats.gpu_ms);else snprintf(gpu,sizeof gpu,"n/a");
    snprintf(line[0],64,"SCENE CPU %.2f MS/EYE | GPU %s MS/EYE",stats.cpu_submit_ms,gpu);
    snprintf(line[1],64,"RENDER %.1f FPS | GAME %.1f FPS",frames/seconds,(qvr_scene_serial()-scene_start)/seconds);
    snprintf(line[2],64,"SCENE %d X %d (%d%%) | TRI %d",stats.scene_width,stats.scene_height,stats.scene_percent,stats.triangles);
    snprintf(line[3],64,"UPLOAD %.0f KB/S | WORLD DRAWS %.1f/FRAME",(stats.texture_bytes+stats.geometry_bytes-bytes_start)/1024.0/seconds,(stats.draw_calls-draw_start)/(double)frames);
    qui_metrics_update(line[0],line[1],line[2],line[3]);
    fprintf(stderr,"[QPERF] %s; %s; %s; %s\n",line[0],line[1],line[2],line[3]);
    start=now;frames=0;scene_start=qvr_scene_serial();bytes_start=stats.texture_bytes+stats.geometry_bytes;draw_start=stats.draw_calls;
}
