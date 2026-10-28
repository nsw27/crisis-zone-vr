/* Included in quest_host.c after the option setters. No duplicated input state. */
static void save_comfort(void){options_saved=qoptions_save(options_path,&options);}
static void adjust_horizontal(int direction){
    if(options_page==QOPTIONS_PAGE_GRAPHICS){
        if(menu_selected==0)set_resolution((options.resolution+direction+QRESOLUTION_COUNT)%QRESOLUTION_COUNT);
        else if(menu_selected==1)toggle_texture_filter();
        else if(menu_selected==2){options.refresh_preset=(options.refresh_preset+direction+4)%4;save_comfort();}
        else if(menu_selected==3){options.scene_density=(options.scene_density+direction+3)%3;qtc2_set_scene_percent(qoptions_scene_percent(options.scene_density));save_comfort();}
        else if(menu_selected==4){options.depth_correction=!options.depth_correction;tc2_depth_mode=options.depth_correction?1:0;save_comfort();}
        else if(menu_selected==5){options.edge_smoothing=!options.edge_smoothing;qtc2_set_edge_smoothing(options.edge_smoothing);save_comfort();}
        else{options.diagnostics=!options.diagnostics;save_comfort();}
    }else if(options_page==QOPTIONS_PAGE_HANDLING){
        if(menu_selected==0){options.two_hand=!options.two_hand;reset_fire_input();}
        else if(menu_selected==1){options.cover_toggle=!options.cover_toggle;reset_fire_input();}
        else if(menu_selected==2){recenter_requested=true;cover_calibration_pending=true;reset_fire_input();}
        else if(menu_selected==3){options.expanded_weapons=!options.expanded_weapons;apply_weapon_model();reset_fire_input();}
        else if(menu_selected==4){options.scene_trace=!options.scene_trace;qvr_trace_set(options.scene_trace);}
        else{options.view_border=(options.view_border+direction+3)%3;}
        save_comfort();
    }else if(options_page==1){
        if(menu_selected==0)set_gun_offset(true,options.gun_offset_y_mm+direction*QOPTIONS_GUN_OFFSET_STEP);
        else if(menu_selected==1)set_gun_offset(false,options.gun_offset_z_mm+direction*QOPTIONS_GUN_OFFSET_STEP);
        else if(menu_selected==2)toggle_weapon_model();
        else if(menu_selected==3){options.recoil_strength=(int)fmaxf(0,fminf(100,options.recoil_strength+direction*25));save_comfort();}
        else if(menu_selected==4){options.haptics=(int)fmaxf(0,fminf(100,options.haptics+direction*25));stop_weapon_feedback();save_comfort();}
        else {options.muzzle_flash=!options.muzzle_flash;save_comfort();}
    }else{
        if(menu_selected==0)set_gun_pitch(options.gun_pitch+direction);
        else if(menu_selected==1)set_world_scale(options.world_scale+direction*QOPTIONS_WORLD_SCALE_STEP);
        else if(menu_selected==2)set_view_distance(options.view_distance+direction*QOPTIONS_VIEW_DISTANCE_STEP);
        else if(menu_selected==3)cycle_aim_guide();
        else if(menu_selected==4){options.physical_crouch=!options.physical_crouch;cover_calibration_pending=true;save_comfort();}
        else change_weapon_hand();
    }
}
static void adjust_vertical(int direction){adjust_horizontal(direction);}
static void reset_page_setting(void){
    if(options_page==QOPTIONS_PAGE_GRAPHICS){
        if(menu_selected==0)set_resolution(0);else if(menu_selected==1){if(!options.texture_filtering)toggle_texture_filter();}
        else if(menu_selected==2){options.refresh_preset=0;save_comfort();}
        else if(menu_selected==3){options.scene_density=0;qtc2_set_scene_percent(100);save_comfort();}
        else if(menu_selected==4){options.depth_correction=false;tc2_depth_mode=0;save_comfort();}
        else if(menu_selected==5){options.edge_smoothing=false;qtc2_set_edge_smoothing(0);save_comfort();}
        else{options.diagnostics=false;save_comfort();}
    }else if(options_page==QOPTIONS_PAGE_HANDLING){
        if(menu_selected==0)options.two_hand=false;
        else if(menu_selected==1)options.cover_toggle=false;
        else if(menu_selected==2){recenter_requested=true;cover_calibration_pending=true;}
        else if(menu_selected==3){options.expanded_weapons=true;apply_weapon_model();}
        else if(menu_selected==4){options.scene_trace=false;qvr_trace_set(false);qvr_trace_flush();}
        else options.view_border=0;
        reset_fire_input();save_comfort();
    }else if(options_page==1){
        if(menu_selected==0)set_gun_offset(true,0);else if(menu_selected==1)set_gun_offset(false,0);
        else if(menu_selected==2){choose_weapon(QWEAPON_MACHINE_GUN);apply_weapon_model();save_comfort();}
        else if(menu_selected==3){options.recoil_strength=75;save_comfort();}
        else if(menu_selected==4){options.haptics=75;save_comfort();}
        else{options.muzzle_flash=true;save_comfort();}
    }else{
        if(menu_selected==0)set_gun_pitch(0);else if(menu_selected==1)set_world_scale(100);
        else if(menu_selected==2)set_view_distance(0);
        else if(menu_selected==3){options.laser_enabled=true;options.tracer_enabled=false;save_comfort();}
        else if(menu_selected==4){options.physical_crouch=false;save_comfort();}
        else if(options.left_handed)change_weapon_hand();
    }
}
