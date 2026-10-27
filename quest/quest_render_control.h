#pragma once
#include <stdint.h>
typedef struct {
    uint64_t eyes,draw_calls,texture_uploads,texture_bytes,geometry_uploads,geometry_bytes;
    int vertices,triangles,scene_width,scene_height,scene_percent;
    double cpu_submit_ms,gpu_ms;uint64_t gpu_samples;int gpu_supported;
} QRenderStats;
/* Scene density changes raster workload only; optical aim and eye projections do not change. */
void qtc2_set_scene_percent(int percent);
void qtc2_render_stats(QRenderStats *out);

void qtc2_set_profiling(int enabled);
void qtc2_profile_close(void);

/* Optional spatial smoothing inside the existing final colour resolve. */
void qtc2_set_edge_smoothing(int enabled);
