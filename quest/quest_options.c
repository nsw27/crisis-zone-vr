#include "quest_options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

void qoptions_load(const char *path,QOptions *options){
    /* New/missing settings use laser assistance. Explicit saved OFF stays OFF. */
    *options=(QOptions){.laser_enabled=true,.texture_filtering=true,.world_scale=QOPTIONS_WORLD_SCALE_DEFAULT,.view_distance=QOPTIONS_VIEW_DISTANCE_DEFAULT,.player_side=1,.enhanced=false,.machine_gun=true,.wide_fov=false,.haptics=75,.recoil_strength=75,.muzzle_flash=true};
    FILE *f=fopen(path,"r");if(!f)return;
    char line[128],extra;int value;
    while(fgets(line,sizeof line,f)){
        const char *new_keys[]={"scene_density=","depth_correction=","two_hand=","cover_toggle=","diagnostics=","scene_trace="};
        for(int k=0;k<6;k++){
            size_t n=strlen(new_keys[k]);if(strncmp(line,new_keys[k],n))continue;
            char *end;errno=0;long v=strtol(line+n,&end,10);
            if(end==line+n||errno||v<0||v>(k==0?2:1))continue;
            while(isspace((unsigned char)*end))end++;if(*end)continue;
            switch(k){case 0:options->scene_density=(int)v;break;case 1:options->depth_correction=v!=0;break;
            case 2:options->two_hand=v!=0;break;case 3:options->cover_toggle=v!=0;break;
            case 4:options->diagnostics=v!=0;break;case 5:options->scene_trace=v!=0;break;}
        }
        const char *extra_keys[]={"haptics=","recoil_strength=","refresh_preset=","muzzle_flash="};
        int limits[]={100,100,3,1};
        for(int k=0;k<4;k++){
            size_t n=strlen(extra_keys[k]);if(strncmp(line,extra_keys[k],n))continue;
            char *end;errno=0;long v=strtol(line+n,&end,10);
            if(end==line+n||errno||v<0||v>limits[k])continue;
            while(isspace((unsigned char)*end))end++;if(*end)continue;
            if(k==0)options->haptics=(int)v;else if(k==1)options->recoil_strength=(int)v;
            else if(k==2)options->refresh_preset=(int)v;else options->muzzle_flash=v!=0;
        }
        if(!strncmp(line,"resolution=",11)){
            char *end;errno=0;long preset=strtol(line+11,&end,10);
            if(end!=line+11&&!errno&&preset>=0&&preset<QRESOLUTION_COUNT){
                while(isspace((unsigned char)*end))end++;
                if(!*end)options->resolution=(int)preset;
            }
        }
        if(!strncmp(line,"texture_filtering=",18)){
            char *end;errno=0;long filtering=strtol(line+18,&end,10);
            if(end!=line+18&&!errno&&(filtering==0||filtering==1)){
                while(isspace((unsigned char)*end))end++;
                if(!*end)options->texture_filtering=filtering==1;
            }
        }
        if(sscanf(line,"laser_enabled=%d %c",&value,&extra)==1&&(value==0||value==1))options->laser_enabled=value==1;
        if(sscanf(line,"physical_crouch=%d %c",&value,&extra)==1&&(value==0||value==1))options->physical_crouch=value==1;
        if(sscanf(line,"left_handed=%d %c",&value,&extra)==1&&(value==0||value==1))options->left_handed=value==1;
        if(sscanf(line,"tracer_enabled=%d %c",&value,&extra)==1&&(value==0||value==1))options->tracer_enabled=value==1;
        if(sscanf(line,"enhanced=%d %c",&value,&extra)==1&&(value==0||value==1))options->enhanced=value==1;
        if(sscanf(line,"machine_gun=%d %c",&value,&extra)==1&&(value==0||value==1))options->machine_gun=value==1;
        if(sscanf(line,"wide_fov=%d %c",&value,&extra)==1&&(value==0||value==1))options->wide_fov=value==1;
        const char *keys[]={"gun_offset_y_mm=","gun_offset_z_mm=","player_side="};
        int *values[]={&options->gun_offset_y_mm,&options->gun_offset_z_mm,&options->player_side};
        for(int i=0;i<3;i++){
            size_t len=strlen(keys[i]);if(strncmp(line,keys[i],len))continue;
            char *end;errno=0;long n=strtol(line+len,&end,10);
            int low=i==2?1:QOPTIONS_GUN_OFFSET_MIN,high=i==2?2:QOPTIONS_GUN_OFFSET_MAX;
            if(end!=line+len&&!errno&&n>=low&&n<=high){
                while(isspace((unsigned char)*end))end++;
                if(!*end)*values[i]=(int)n;
            }
        }
        if(!strncmp(line,"world_scale=",12)){
            char *end;errno=0;long scale=strtol(line+12,&end,10);
            if(end!=line+12&&!errno&&scale>=QOPTIONS_WORLD_SCALE_MIN&&scale<=QOPTIONS_WORLD_SCALE_MAX){
                while(isspace((unsigned char)*end))end++;
                if(!*end)options->world_scale=(int)scale;
            }
        }
        if(!strncmp(line,"gun_pitch=",10)){
            char *end;errno=0;long pitch=strtol(line+10,&end,10);
            if(end!=line+10&&!errno&&pitch>=QOPTIONS_GUN_PITCH_MIN&&pitch<=QOPTIONS_GUN_PITCH_MAX){
                while(isspace((unsigned char)*end))end++;
                if(!*end)options->gun_pitch=(int)pitch;
            }
        }
        if(!strncmp(line,"view_distance=",14)){
            char *end;errno=0;long distance=strtol(line+14,&end,10);
            if(end!=line+14&&!errno&&distance>=QOPTIONS_VIEW_DISTANCE_MIN&&distance<=QOPTIONS_VIEW_DISTANCE_MAX){
                while(isspace((unsigned char)*end))end++;
                if(!*end)options->view_distance=(int)distance;
            }
        }
    }
    fclose(f);
    options->scene_trace=false; /* one-shot command, never record automatically on load */
}
bool qoptions_save(const char *path,const QOptions *options){
    FILE *f=fopen(path,"w");if(!f)return false;
    bool ok=fprintf(f,"resolution=%d\ntexture_filtering=%d\nlaser_enabled=%d\nphysical_crouch=%d\nleft_handed=%d\ngun_pitch=%d\nworld_scale=%d\nview_distance=%d\ntracer_enabled=%d\ngun_offset_y_mm=%d\ngun_offset_z_mm=%d\nplayer_side=%d\nenhanced=%d\nmachine_gun=%d\nwide_fov=%d\n",options->resolution,options->texture_filtering?1:0,options->laser_enabled?1:0,options->physical_crouch?1:0,options->left_handed?1:0,options->gun_pitch,options->world_scale,options->view_distance,options->tracer_enabled?1:0,options->gun_offset_y_mm,options->gun_offset_z_mm,options->player_side,options->enhanced?1:0,options->machine_gun?1:0,options->wide_fov?1:0)>0;
    if(fprintf(f,"haptics=%d\nrecoil_strength=%d\nrefresh_preset=%d\nmuzzle_flash=%d\n",
               options->haptics,options->recoil_strength,options->refresh_preset,options->muzzle_flash?1:0)<0)ok=false;
    if(fprintf(f,"scene_density=%d\ndepth_correction=%d\ntwo_hand=%d\ncover_toggle=%d\ndiagnostics=%d\nscene_trace=%d\n",
       options->scene_density,options->depth_correction,options->two_hand,options->cover_toggle,options->diagnostics,options->scene_trace)<0)ok=false;
    if(fclose(f))ok=false;
    return ok;
}
