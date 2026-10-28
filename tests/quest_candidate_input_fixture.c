/* Real production host against the original deterministic OpenXR mock. */
#define main legacy_handedness_main
#include "handedness_fixture.c"
#undef main
unsigned char tc2_ram[0x1000000];
static QGunKind test_model;
static bool test_shotgun_available;
static bool test_scene_panel;
bool qvr_scene_is_panel(void){return test_scene_panel;}
int tc2_depth_mode;
void qvr_trace_set(bool on){(void)on;}
void qvr_trace_flush(void){}
static int test_density=100,test_smoothing;
void qtc2_set_edge_smoothing(int on){test_smoothing=on!=0;}
void qtc2_set_scene_percent(int p){test_density=p;}
bool qgun_available(QGunKind k){return k==QGUN_PISTOL||k==QGUN_MACHINE_GUN||(k==QGUN_SHOTGUN&&test_shotgun_available);}
void qgun_select(QGunKind k){test_model=k;}
int main(void){

    /* A successful request is not proof of application. Retry while focused,
     * stop on measured success, and cap retries if the runtime refuses to switch. */
    get_rate=mock_get_rate;request_rate=mock_request_rate;preferred_rate=120;focused=true;
    display_rate_check(1);assert(!rate_requests);display_rate_check(2000000001LL);assert(rate_requests==1);
    reported_rate=120;display_rate_check(4000000001LL);assert(rate_attempts==4);
    display_rate_check(6000000001LL);assert(rate_requests==1);
    reported_rate=72;rate_attempts=0;rate_check_time=0;rate_requests=0;
    for(int i=0;i<8;i++)display_rate_check(1+(int64_t)i*2000000000LL);
    assert(rate_requests==3&&rate_attempts==4);
    focused=false;rate_attempts=0;rate_check_time=0;display_rate_check(30000000000LL);assert(!rate_check_time);
    get_rate=NULL;request_rate=NULL;
    options_path="handedness-test.cfg";qoptions_load(options_path,&options);
    options.expanded_weapons=false;assert(!options.left_handed);instance=(XrInstance)(uintptr_t)1;session=(XrSession)(uintptr_t)1;
    running=focused=origin_set=true;assert(actions_init());assert(space_count==2&&binding_count==17);
    for(int h=0;h<2;h++){
        char name[100];const char *prefix=h?"/user/hand/right":"/user/hand/left";
        snprintf(name,sizeof name,"%s/input/aim/pose",prefix);bound(aim_action,name);
        snprintf(name,sizeof name,"%s/input/trigger/value",prefix);bound(trigger_action,name);
        snprintf(name,sizeof name,"%s/input/squeeze/value",prefix);bound(grip_action,name);
        snprintf(name,sizeof name,"%s/input/%s/click",prefix,h?"a":"x");bound(lower_action,name);
        snprintf(name,sizeof name,"%s/input/%s/click",prefix,h?"b":"y");bound(upper_action,name);
        snprintf(name,sizeof name,"%s/output/haptic",prefix);bound(haptic_action,name);
    }
    bound(pause_action,"/user/hand/left/input/menu/click");bound(hand_action,"/user/hand/right/input/thumbstick/click");
    tick();assert(gun_position.x==.25f&&gun_origin.x==.25f&&aim_valid);
    /* Either grip exposes; only releasing both hides. Trigger no longer opens cover. */
    grips[LEFT_HAND]=.9f;tick();assert((arcade_bits&0x30)==0x20);
    grips[RIGHT_HAND]=.8f;grips[LEFT_HAND]=0;tick();assert((arcade_bits&0x30)==0x20);
    grips[LEFT_HAND]=.9f;grips[RIGHT_HAND]=0;tick();assert((arcade_bits&0x30)==0x20);
    grips[RIGHT_HAND]=.8f;grips[LEFT_HAND]=0;tick();assert(pedal==.8f);
    grips[RIGHT_HAND]=0;triggers[RIGHT_HAND]=.9f;tick();assert((arcade_bits&0x30)==0x10);
    ss22_input_rumble(0,48000,45);assert(last_haptic==hand_paths[RIGHT_HAND]);
    /* Opposite trigger pressed while old one remains held: new arcade fire edge. */
    triggers[LEFT_HAND]=.9f;tick();assert(weapon_hand()==LEFT_HAND&&!(arcade_bits&0x10));
    assert(last_stop==hand_paths[RIGHT_HAND]&&gun_position.x==-.25f&&aim_valid);
    tick();assert(arcade_bits&0x10);ss22_input_rumble(0,48000,45);assert(last_haptic==hand_paths[LEFT_HAND]);
    /* Scripted machine-gun sections need a sustained arcade trigger level. */
    for(int n=0;n<120;n++){tick();assert(weapon_hand()==LEFT_HAND&&(arcade_bits&0x10));}
    triggers[LEFT_HAND]=0;tick();assert(!(arcade_bits&0x10)&&weapon_hand()==LEFT_HAND);
    assert(!options.left_handed); /* Handoffs do not change the saved button layout. */
    click(lower_action,RIGHT_HAND);assert(coin_frames>0);coin_frames=0;
    click(upper_action,RIGHT_HAND);assert(!options.laser_enabled&&!paused);
    QOptions saved;qoptions_load(options_path,&saved);assert(!saved.left_handed&&!saved.laser_enabled);
    triggers[RIGHT_HAND]=0;tick();triggers[RIGHT_HAND]=.9f;tick();assert(weapon_hand()==RIGHT_HAND&&(arcade_bits&0x10));
    /* A tap between simulation ticks must still produce one press. */
    triggers[0]=triggers[1]=0;tick();triggers[LEFT_HAND]=.9f;sync_input(100,true,1.7f);
    triggers[LEFT_HAND]=0;sync_input(101,true,1.7f);ss22_input_update();assert(arcade_bits&0x10);
    tick();assert(!(arcade_bits&0x10));
    /* Simultaneous samples use event time; exact ties keep the active hand. */
    triggers[0]=triggers[1]=.9f;trigger_times[0]=10;trigger_times[1]=20;tick();assert(weapon_hand()==RIGHT_HAND);
    triggers[0]=triggers[1]=0;tick();triggers[0]=triggers[1]=.9f;trigger_times[0]=trigger_times[1]=30;
    tick();assert(weapon_hand()==RIGHT_HAND);
    triggers[0]=triggers[1]=0;tick();triggers[0]=triggers[1]=.9f;trigger_times[0]=50;trigger_times[1]=40;
    tick();assert(weapon_hand()==LEFT_HAND);
    /* Keep cover operational without an aim pose; recovery requires trigger release. */
    grips[RIGHT_HAND]=.8f;tracked[LEFT_HAND]=false;tick();assert(!gun_tracked&&!aim_valid&&trigger==0&&pedal==.8f);
    tracked[LEFT_HAND]=true;tick();assert(trigger==0);triggers[LEFT_HAND]=0;tick();triggers[LEFT_HAND]=.9f;tick();assert(trigger==1);
    connected[RIGHT_HAND]=false;tick();assert(pedal==0&&gun_tracked);connected[RIGHT_HAND]=true;
    /* Action loss also cancels an unconsumed shot, even if pose remains valid. */
    triggers[LEFT_HAND]=0;tick();triggers[LEFT_HAND]=.9f;sync_input(100,true,1.7f);assert(shot_pending);
    trigger_active[LEFT_HAND]=false;tick();assert(!(arcade_bits&0x10));
    trigger_active[LEFT_HAND]=true;tick();assert(!(arcade_bits&0x10));
    /* Focus loss/pause/sync errors cannot replay a held trigger or button. */
    focused=false;tick();assert(arcade_bits==0);buttons[id(upper_action)][RIGHT_HAND]=true;
    focused=true;tick();assert(!options.laser_enabled&&trigger==0);
    buttons[id(upper_action)][RIGHT_HAND]=false;tick();click(upper_action,RIGHT_HAND);assert(options.tracer_enabled&&!options.laser_enabled);click(upper_action,RIGHT_HAND);assert(options.laser_enabled);
    sync_result=XR_ERROR_RUNTIME_FAILURE;tick();assert(trigger==0&&pedal==0);
    buttons[id(upper_action)][RIGHT_HAND]=true;sync_result=XR_SUCCESS;tick();assert(options.laser_enabled&&trigger==0);
    buttons[id(upper_action)][RIGHT_HAND]=false;tick();
    click(hand_action,2);assert(!options.left_handed&&!paused); /* Menu-only preference. */
    click(pause_action,2);assert(paused&&arcade_bits==0);recoil_started=500;click(hand_action,2);
    assert(options.left_handed&&paused&&options_saved&&!recoil_started&&weapon_hand()==LEFT_HAND);
    qoptions_load(options_path,&saved);assert(saved.left_handed&&saved.laser_enabled);
    click(pause_action,2);assert(!paused&&trigger==0);
    triggers[0]=triggers[1]=0;tick();triggers[RIGHT_HAND]=.9f;tick();assert(weapon_hand()==RIGHT_HAND&&(arcade_bits&0x10));
    click(lower_action,LEFT_HAND);assert(coin_frames>0);coin_frames=0;recenter_requested=false;
    click(lower_action,RIGHT_HAND);assert(recenter_requested&&coin_frames==0);
    click(upper_action,LEFT_HAND);assert(!options.laser_enabled&&!paused);
    click(upper_action,RIGHT_HAND);assert(!options.physical_crouch&&!paused);
    click(pause_action,2);click(upper_action,RIGHT_HAND);assert(options.physical_crouch);
    click(pause_action,2);grips[0]=grips[1]=0;tick();assert(pedal==1);
    grips[0]=grips[1]=1;sync_input(100,true,1.45f);assert(pedal==0);tick();assert(pedal==1);
    sync_input(100,false,1.7f);assert(pedal==0);tick();
    /* Held face buttons must not acquire new roles on manual layout change. */
    click(pause_action,2);buttons[id(lower_action)][LEFT_HAND]=true;buttons[id(upper_action)][LEFT_HAND]=true;tick();
    bool laser=options.laser_enabled;bool physical=options.physical_crouch;recenter_requested=false;
    buttons[id(hand_action)][2]=true;tick();assert(!options.left_handed&&paused);
    for(int n=0;n<6;n++)tick();
    assert(!recenter_requested&&coin_frames==0&&options.laser_enabled==laser&&options.physical_crouch==physical);
    memset(buttons,0,sizeof buttons);triggers[0]=triggers[1]=0;tick();
    click(lower_action,RIGHT_HAND);assert(coin_frames==48);coin_frames=0;
    click(lower_action,LEFT_HAND);assert(recenter_requested);
    click(upper_action,RIGHT_HAND);assert(options.laser_enabled!=laser);
    click(upper_action,LEFT_HAND);assert(options.physical_crouch!=physical);

    /* New menu contract: left Y selects rows, left X pages; right edits. */
    memset(buttons,0,sizeof buttons);triggers[0]=triggers[1]=grips[0]=grips[1]=0;
    focused=running=origin_set=true;paused=true;options_page=0;menu_selected=0;
    connected[0]=connected[1]=tracked[0]=tracked[1]=true;
    trigger_active[0]=trigger_active[1]=true;stick_active=distance_active=true;
    stick_x=stick_y=distance_x=distance_y=0;reset_input_edges();tick();
    for(int page=0;page<QOPTIONS_PAGE_COUNT;page++){
        assert(options_page==page);
        for(int row=0;row<qoptions_row_count(page);row++){
            assert(menu_selected==row);distance_y=-1;tick();tick();
            assert(menu_selected==(row+1)%qoptions_row_count(page));distance_y=0;tick();
        }
        distance_x=1;tick();assert(options_page==(page+1)%QOPTIONS_PAGE_COUNT);tick();assert(options_page==(page+1)%QOPTIONS_PAGE_COUNT);
        distance_x=0;tick();
    }
    menu_selected=0;options.gun_pitch=0;reset_pitch_input();tick();stick_x=1;tick();assert(options.gun_pitch==1);
    tick();assert(options.gun_pitch==1);sync_input(500000100,true,1.7f);assert(options.gun_pitch==2);
    stick_x=0;tick();
    options_page=1;menu_selected=2;options.machine_gun=false;reset_pitch_input();tick();
    stick_x=1;tick();assert(options.machine_gun&&test_model==QGUN_MACHINE_GUN);
    sync_input(9999999999LL,true,1.7f);assert(options.machine_gun); /* toggle cannot autorepeat */
    stick_x=0;tick();menu_selected=4;options.haptics=75;stick_x=-1;tick();assert(options.haptics==50);
    stick_x=0;tick();click(pitch_reset_action,2);assert(options.haptics==75);
    menu_selected=5;stick_x=1;tick();assert(!options.muzzle_flash);stick_x=0;tick();
    QOptions roundtrip;qoptions_load(options_path,&roundtrip);assert(!roundtrip.muzzle_flash&&roundtrip.haptics==75&&roundtrip.machine_gun);
    options_page=2;menu_selected=2;options.refresh_preset=0;stick_x=1;tick();assert(options.refresh_preset==1);
    stick_x=0;tick();assert(qoptions_refresh_hz(1)==72&&qoptions_refresh_hz(2)==90&&qoptions_refresh_hz(3)==120);
    menu_selected=3;stick_x=1;tick();assert(options.scene_density==1&&test_density==85);stick_x=0;tick();
    menu_selected=4;stick_x=1;tick();assert(options.depth_correction&&tc2_depth_mode==1);stick_x=0;tick();
    menu_selected=5;stick_x=1;tick();assert(options.edge_smoothing&&test_smoothing);stick_x=0;tick();
    qoptions_load(options_path,&roundtrip);assert(roundtrip.edge_smoothing);
    click(pitch_reset_action,2);assert(!options.edge_smoothing&&!test_smoothing);
    options_page=QOPTIONS_PAGE_HANDLING;menu_selected=0;stick_x=1;tick();assert(options.two_hand);stick_x=0;tick();
    menu_selected=1;stick_x=1;tick();assert(options.cover_toggle);stick_x=0;tick();
    qoptions_load(options_path,&roundtrip);assert(roundtrip.scene_density==1&&roundtrip.depth_correction&&roundtrip.two_hand&&roundtrip.cover_toggle);
    options.two_hand=options.cover_toggle=false;
    /* Point-and-trigger uses the same production menu actions, while paused.
     * Label clicks select; +/- clicks edit; a held trigger must never repeat. */
    focused=running=origin_set=paused=true;menu_head_valid=true;menu_head_position=v3(0,0,0);
    menu_head_rotation=origin_rotation=controller_rotation=(Q4){0,0,0,1};origin=v3(0,1.7f,0);
    options.gun_pitch=options.gun_offset_y_mm=options.gun_offset_z_mm=0;
    tracked[0]=tracked[1]=connected[0]=connected[1]=true;select_weapon_hand(RIGHT_HAND);
    options_page=QOPTIONS_PAGE_GRAPHICS;menu_selected=0;options.edge_smoothing=false;
    menu_click=(QMenuClick){.hand=-1};triggers[0]=triggers[1]=0;
    controller_positions[RIGHT_HAND]=(XrVector3f){(604.f/640-.5f)*1.3f,1.7f+(.5f-252.f/480)*.975f,-.5f};
    tick();assert(menu_pointer_visible);triggers[RIGHT_HAND]=1;tick();
    assert(menu_selected==5&&options.edge_smoothing&&!(tc2_inputs&0x30));
    tick();assert(options.edge_smoothing);triggers[RIGHT_HAND]=0;tick();
    controller_positions[RIGHT_HAND].x=(200.f/640-.5f)*1.3f;
    controller_positions[RIGHT_HAND].y=1.7f+(.5f-132.f/480)*.975f;
    bool old_filter=options.texture_filtering;triggers[RIGHT_HAND]=1;tick();assert(menu_selected==1&&options.texture_filtering==old_filter);
    /* Press outside, move inside while held: no delayed button activation. */
    triggers[RIGHT_HAND]=0;tick();controller_positions[RIGHT_HAND].x=5;triggers[RIGHT_HAND]=1;tick();assert(!menu_pointer_visible);
    controller_positions[RIGHT_HAND].x=(604.f/640-.5f)*1.3f;tick();assert(options.texture_filtering==old_filter);
    triggers[RIGHT_HAND]=0;tick();controller_positions[RIGHT_HAND].y=1.7f+(.5f-58.f/480)*.975f;
    triggers[RIGHT_HAND]=1;tick();assert(options_page==QOPTIONS_PAGE_HANDLING);tick();assert(options_page==QOPTIONS_PAGE_HANDLING);
    tracked[RIGHT_HAND]=false;tick();assert(!menu_pointer_visible);tracked[RIGHT_HAND]=true;tick();assert(options_page==QOPTIONS_PAGE_HANDLING);
    menu_head_valid=false;triggers[0]=triggers[1]=0;tick();assert(!menu_pointer_visible);
    controller_positions[RIGHT_HAND]=(XrVector3f){.25f,1.4f,-.5f};
    /* Illegal inputs cannot generate undefined float/integer conversions. */
    nx=NAN;ny=INFINITY;aim_valid=true;ss22_input_update();assert(g_ss22_gun_off);
    nx=-1000;ny=1000;ss22_input_update();assert(g_ss22_gun_x==64&&g_ss22_gun_y==255&&g_ss22_gun_off);
    nx=1;ny=0;ss22_input_update();assert(g_ss22_gun_x==831&&g_ss22_gun_y==32);
    /* Trigger NaN cancels; reconnect/finite recovery needs a new release. */
    paused=false;options.physical_crouch=false;reset_input_edges();triggers[0]=triggers[1]=0;tick();
    triggers[weapon_hand()]=1;tick();assert(trigger==1);triggers[weapon_hand()]=NAN;tick();assert(trigger==0);
    triggers[weapon_hand()]=1;tick();assert(trigger==0);triggers[weapon_hand()]=0;tick();triggers[weapon_hand()]=1;tick();assert(trigger==1);
    /* Real host path: off-hand support does not also operate cover. */
    focused=running=origin_set=true;paused=false;options.physical_crouch=false;
    options.two_hand=options.machine_gun=true;options.cover_toggle=false;
    options.gun_pitch=options.gun_offset_y_mm=options.gun_offset_z_mm=0;
    origin=v3(0,1.7f,0);origin_rotation=controller_rotation=(Q4){0,0,0,1};
    connected[0]=connected[1]=tracked[0]=tracked[1]=true;select_weapon_hand(RIGHT_HAND);
    controller_positions[LEFT_HAND]=(XrVector3f){.25f,1.36f,-.8f};
    triggers[0]=triggers[1]=grips[0]=grips[1]=0;reset_input_edges();tick();
    grips[LEFT_HAND]=1;tick();assert(support_grip.held&&pedal==0);
    grips[RIGHT_HAND]=1;tick();assert(support_grip.held&&pedal==1);
    grips[LEFT_HAND]=0;tick();assert(!support_grip.held&&pedal==1);
    options.cover_toggle=true;grips[RIGHT_HAND]=0;tick();grips[RIGHT_HAND]=1;tick();assert(pedal==1);
    grips[RIGHT_HAND]=0;tick();assert(pedal==1);grips[RIGHT_HAND]=1;tick();assert(pedal==0);
    grips[RIGHT_HAND]=0;tick();grips[RIGHT_HAND]=1;tick();assert(pedal==1);
    tracked[RIGHT_HAND]=false;tick();assert(pedal==0);tracked[RIGHT_HAND]=true;tick();assert(pedal==0);
    options.two_hand=options.cover_toggle=false;
    /* Pure burst clock: no repeats without permissions, immediate first shot. */
    QFeedback f={0};assert(qfeedback_tick(&f,false,1)==0);assert(qfeedback_tick(&f,true,2)==1);
    assert(qfeedback_tick(&f,true,99)==0);assert(qfeedback_tick(&f,true,102)==1);
    assert(qfeedback_tick(&f,false,103)==-1);assert(qfeedback_tick(&f,false,104)==0);assert(qfeedback_tick(&f,true,105)==1);
    (void)SDL_GetTicks64();SDL_Delay(3);
    /* Actual output function: exposing without trigger gives no effects. */
    focused=running=gun_tracked=true;paused=false;options.haptics=75;
    trigger=0;pedal=1;fire_output_down=false;stop_weapon_feedback();ss22_out_poll(2);
    assert(!feedback.active&&!haptic_active&&!recoil_started);
    trigger=1;fire_output_down=true;ss22_out_poll(2);assert(feedback.active&&haptic_active&&recoil_started);
    trigger=0;ss22_out_poll(2);assert(!feedback.active&&!haptic_active&&!recoil_started);
    trigger=1;pedal=0;ss22_out_poll(2);assert(!feedback.active);
    pedal=1;ss22_out_poll(2);assert(feedback.active);paused=true;ss22_out_poll(2);assert(!feedback.active&&!haptic_active);
    paused=false;ss22_out_poll(2);assert(feedback.active);focused=false;ss22_out_poll(2);assert(!feedback.active&&!haptic_active);
    focused=true;gun_tracked=false;ss22_out_poll(2);assert(!feedback.active);
    gun_tracked=true;options.haptics=0;ss22_out_poll(2);assert(feedback.active&&!haptic_active);ss22_input_neutral();assert(!feedback.active);
    /* Expanded weapons use directional right-stick movement only IN COVER.
     * These host tests model confirmations explicitly; a separate fixture
     * executes the actual lifted ammo/decrement code. */
    memset(tc2_ram,0,sizeof tc2_ram);
    #define TEST_W32(a,v) do{unsigned _a=(a)&0xffffff,_v=(v);for(int _j=0;_j<4;_j++)tc2_ram[_a+_j]=(unsigned char)(_v>>(24-8*_j));}while(0)
    TEST_W32(0x803ba3f4,0x80549844);TEST_W32(0x803ba3f8,0x805495a0);TEST_W32(0x80549848,40);
    csz_weapons_init(0x80549844);
    options.expanded_weapons=true;options.machine_gun=true;options.two_hand=true;
    options.cover_toggle=true;options.physical_crouch=false;options.left_handed=false;
    focused=running=origin_set=true;paused=false;menu_head_valid=false;select_weapon_hand(RIGHT_HAND);
    triggers[0]=triggers[1]=grips[0]=grips[1]=0;reset_input_edges();tick();csz_weapons_begin(0x803b4e00);
    grips[RIGHT_HAND]=1;tick();grips[RIGHT_HAND]=0;tick();assert(pedal==1);
    float old_x=nx,old_y=ny;bool old_off=g_ss22_gun_off;
    unsigned old_gx=g_ss22_gun_x,old_gy=g_ss22_gun_y;
    triggers[RIGHT_HAND]=1;tick();assert(fire_output_down);
    stick_x=1;tick();csz_weapons_begin(0x803b4e00);
    assert(options.machine_gun&&!weapon_select_visible&&pedal==1); /* exposed: ignored */
    grips[RIGHT_HAND]=1;tick();grips[RIGHT_HAND]=0;tick();assert(pedal==0&&weapon_select_visible);
    assert(options.machine_gun); /* entering cover with held direction cannot switch */
    stick_x=0;tick();stick_x=1;tick();csz_weapons_begin(0x803b4e00);
    assert(!options.machine_gun&&test_model==QGUN_PISTOL&&pedal==0);
    for(int i=0;i<30;i++){tick();csz_weapons_begin(0x803b4e00);assert(!fire_output_down&&!csz_weapons_pending()&&pedal==0);}
    assert(nx==old_x&&ny==old_y&&g_ss22_gun_x==old_gx&&g_ss22_gun_y==old_gy&&g_ss22_gun_off==old_off);
    assert(csz_weapons_state()->ammo[1]==12);
    stick_x=0;triggers[RIGHT_HAND]=0;tick();
    grips[RIGHT_HAND]=1;tick();grips[RIGHT_HAND]=0;tick();assert(pedal==1&&!weapon_select_visible);
    triggers[RIGHT_HAND]=1;tick();assert(csz_weapons_pending()&&fire_output_down);
    TEST_W32(0x80549848,11);csz_weapons_accepted(0x80549844);
    options.haptics=75;ss22_out_poll(0); /* confirmed shot, not cabinet's cover level */
    assert(recoil_started&&haptic_active);uint64_t confirmed_time=recoil_started;
    triggers[RIGHT_HAND]=0;tick();assert(recoil_started==confirmed_time&&!csz_weapons_pending());
    triggers[RIGHT_HAND]=1;tick();assert(!csz_weapons_pending()); /* still in cooldown */
    for(int i=0;i<30;i++){tick();csz_weapons_begin(0x803b4e00);assert(!fire_output_down&&!csz_weapons_pending());}
    assert(csz_weapons_state()->ammo[1]==11);
    grips[RIGHT_HAND]=1;tick();grips[RIGHT_HAND]=0;tick();assert(pedal==0);
    stick_y=-1;tick();stick_y=0;tick();csz_weapons_begin(0x803b4e00);assert(options.machine_gun&&pedal==0);
    stick_y=1;tick();stick_y=0;tick();csz_weapons_begin(0x803b4e00);assert(!options.machine_gun&&csz_weapons_state()->ammo[1]==11&&pedal==0);
    grips[RIGHT_HAND]=1;tick();grips[RIGHT_HAND]=0;tick();assert(pedal==1&&!csz_weapons_pending());
    triggers[RIGHT_HAND]=0;tick();triggers[RIGHT_HAND]=1;tick();assert(csz_weapons_pending());
    focused=false;tick();assert(!csz_weapons_pending()&&!fire_output_down);focused=true;tick();assert(!csz_weapons_pending());
    triggers[RIGHT_HAND]=0;tick(); /* focus loss safely returned toggle-cover to hidden */
    assert(pedal==0);grips[RIGHT_HAND]=1;tick();assert(pedal==1);grips[RIGHT_HAND]=0;tick();
    triggers[RIGHT_HAND]=1;tick();assert(csz_weapons_pending());
    trigger_active[RIGHT_HAND]=false;tick();assert(!csz_weapons_pending());trigger_active[RIGHT_HAND]=true;tick();assert(!csz_weapons_pending());
    click(pause_action,2);assert(paused);bool old_hand=options.left_handed;click(hand_action,2);assert(options.left_handed!=old_hand&&paused&&!options.machine_gun);
    /* Start/menu panels use the original trigger, even with a stale live pointer. */
    paused=false;test_scene_panel=true;triggers[0]=triggers[1]=0;tick();
    triggers[weapon_hand()]=1;tick();assert(fire_output_down&&!csz_weapons_pending());
    test_scene_panel=false;paused=true;
    options_page=QOPTIONS_PAGE_HANDLING;menu_selected=3;adjust_horizontal(1);assert(!options.expanded_weapons);
    adjust_horizontal(1);qoptions_load(options_path,&saved);assert(saved.expanded_weapons);
    reset_page_setting();qoptions_load(options_path,&saved);assert(saved.expanded_weapons);
    menu_selected=5;assert(options.view_border==0);
    adjust_horizontal(1);assert(options.view_border==1);adjust_horizontal(1);assert(options.view_border==2);
    qoptions_load(options_path,&saved);assert(saved.view_border==2);adjust_horizontal(1);assert(options.view_border==0);
    adjust_horizontal(-1);assert(options.view_border==2);reset_page_setting();assert(options.view_border==0);
    /* Direction controls in the real host: each cardinal axis, diagonals,
     * no auto-repeat, menu/reconnect rearming, replay and disabled mode. */
    paused=false;focused=running=origin_set=true;test_scene_panel=false;
    connected[0]=connected[1]=tracked[0]=tracked[1]=true;stick_active=true;
    options.physical_crouch=false;options.two_hand=false;options.cover_toggle=true;
    triggers[0]=triggers[1]=grips[0]=grips[1]=0;options.machine_gun=true;
    options.expanded_weapons=true;select_weapon_hand(RIGHT_HAND);reset_input_edges();
    csz_weapons_configure(true,false);csz_weapons_init(0x80549844);TEST_W32(0x80549848,40);
    stick_x=stick_y=0;tick();csz_weapons_begin(0x803b4e00);tick();assert(weapon_select_visible);
    const float moves[][2]={{1,0},{-1,0},{0,1},{0,-1},{.9f,.7f}};
    for(unsigned k=0;k<sizeof moves/sizeof *moves;k++){
        bool was=options.machine_gun;float was_pedal=pedal;
        unsigned gx=g_ss22_gun_x,gy=g_ss22_gun_y;
        stick_x=moves[k][0];stick_y=moves[k][1];tick();csz_weapons_begin(0x803b4e00);
        assert(options.machine_gun!=was&&pedal==was_pedal&&g_ss22_gun_x==gx&&g_ss22_gun_y==gy);
        for(int n=0;n<40;n++){tick();assert(options.machine_gun!=was);}
        stick_x=-stick_x;stick_y=-stick_y;tick();assert(options.machine_gun!=was);
        stick_x=stick_y=0;tick();
    }
    bool selected_before=options.machine_gun;
    click(hand_action,2);assert(options.machine_gun==selected_before); /* no click needed */
    paused=true;stick_x=1;tick();paused=false;tick();assert(options.machine_gun==selected_before);
    stick_x=0;tick();stick_active=false;stick_x=1;tick();stick_active=true;tick();assert(options.machine_gun==selected_before);
    stick_x=0;tick();tracked[RIGHT_HAND]=false;stick_x=1;tick();tracked[RIGHT_HAND]=true;tick();assert(options.machine_gun==selected_before);
    stick_x=0;tick();focused=false;stick_x=1;tick();focused=true;tick();assert(options.machine_gun==selected_before);
    stick_x=0;tick();test_scene_panel=true;stick_x=1;tick();test_scene_panel=false;tick();assert(options.machine_gun==selected_before);
    stick_x=0;tick();replay_input=true;stick_x=1;tick();replay_input=false;tick();assert(options.machine_gun==selected_before);
    stick_x=0;tick();options.expanded_weapons=false;stick_x=1;tick();options.expanded_weapons=true;tick();assert(options.machine_gun==selected_before);
    stick_x=0;tick();stick_x=-1;tick();assert(options.machine_gun!=selected_before);
    /* A direction held outside cover cannot be consumed on re-entry, even
     * when both samples occur between original simulation ticks. */
    stick_x=0;tick();grips[RIGHT_HAND]=1;tick();grips[RIGHT_HAND]=0;tick();assert(pedal==1&&!weapon_select_visible);
    selected_before=options.machine_gun;stick_x=1;tick();assert(options.machine_gun==selected_before);
    grips[RIGHT_HAND]=1;tick();grips[RIGHT_HAND]=0;tick();assert(pedal==0&&weapon_select_visible&&options.machine_gun==selected_before);
    stick_x=0;tick();stick_y=1;tick();assert(options.machine_gun!=selected_before);
    stick_y=0;tick();selected_before=options.machine_gun;
    TEST_W32(0x803ba3f4,0);stick_x=1;tick();assert(!weapon_select_visible&&options.machine_gun==selected_before);
    TEST_W32(0x803ba3f4,0x80549844);tick();assert(options.machine_gun==selected_before);
    stick_x=0;tick();sync_input(102,false,1.7f);assert(!weapon_select_visible);
    stick_x=1;tick();assert(options.machine_gun==selected_before);
    /* Physical ducking uses the same selector context, not an extra shield. */
    options.physical_crouch=true;cover_calibration_pending=false;qcover_calibrate(&cover,1.7f);
    stick_x=stick_y=0;sync_input(103,true,1.7f);assert(pedal==1&&!weapon_select_visible);
    sync_input(104,true,1.0f);assert(pedal==0&&weapon_select_visible);
    stick_x=-1;sync_input(105,true,1.0f);assert(options.machine_gun!=selected_before);
    /* Full three-weapon integration uses the same production input path.
     * Earlier tests intentionally omit the optional mesh to test fallback. */
    test_shotgun_available=true;test_scene_panel=false;paused=false;focused=running=origin_set=true;
    connected[0]=connected[1]=tracked[0]=tracked[1]=trigger_active[0]=trigger_active[1]=true;
    options.expanded_weapons=true;options.physical_crouch=options.cover_toggle=false;
    options.two_hand=true;options.left_handed=false;choose_weapon(QWEAPON_MACHINE_GUN);
    stick_active=true;stick_x=stick_y=0;triggers[0]=triggers[1]=grips[0]=grips[1]=0;
    select_weapon_hand(RIGHT_HAND);reset_input_edges();
    TEST_W32(0x803ba3f4,0x80549844);TEST_W32(0x803ba3f8,0x805495a0);
    TEST_W32(0x80549844,3);TEST_W32(0x80549874,0);TEST_W32(0x80549848,40);
    csz_weapons_init(0x80549844);apply_weapon_model();tick();csz_weapons_begin(0x803b4e00);tick();
    assert(pedal==0&&weapon_select_visible&&chosen_weapon()==QWEAPON_MACHINE_GUN);
    /* Up selects handgun; right selects shotgun; held directions do not cycle. */
    stick_y=1;tick();csz_weapons_begin(0x803b4e00);
    assert(chosen_weapon()==QWEAPON_HANDGUN&&test_model==QGUN_PISTOL&&csz_weapons_state()->ammo[1]==12&&pedal==0);
    stick_y=0;tick();stick_x=1;tick();csz_weapons_begin(0x803b4e00);
    assert(chosen_weapon()==QWEAPON_SHOTGUN&&test_model==QGUN_SHOTGUN&&csz_weapons_state()->ammo[2]==3&&pedal==0);
    for(int i=0;i<30;i++)tick();assert(chosen_weapon()==QWEAPON_SHOTGUN&&pedal==0);
    /* Exposed stick movement cannot switch. The existing support grip remains
     * reserved, so the pistol/shotgun cannot accidentally change shield state. */
    stick_x=0;tick();grips[RIGHT_HAND]=1;tick();assert(pedal==1&&!weapon_select_visible);
    stick_y=-1;tick();assert(chosen_weapon()==QWEAPON_SHOTGUN);
    triggers[RIGHT_HAND]=1;tick();assert(csz_weapons_pending()&&(arcade_bits&0x10));
    TEST_W32(0x80549848,2);TEST_W32(0x803ba3fc,15);csz_weapons_accepted(0x80549844);
    assert(!csz_weapons_pending()&&csz_weapons_state()->ammo[2]==2&&!csz_weapons_state()->fault);
    assert(csz_weapons_feedback()&&!csz_weapons_feedback());
    for(int i=0;i<40;i++){tick();csz_weapons_begin(0x803b4e00);assert(!csz_weapons_pending());}
    grips[RIGHT_HAND]=0;tick();assert(pedal==0&&chosen_weapon()==QWEAPON_SHOTGUN);
    /* Returning to cover with stick already held requires neutral first. */
    tick();assert(chosen_weapon()==QWEAPON_SHOTGUN);
    stick_y=0;tick();stick_x=-1;tick();csz_weapons_begin(0x803b4e00);
    assert(chosen_weapon()==QWEAPON_HANDGUN&&pedal==0);
    stick_x=0;tick();stick_y=-1;tick();csz_weapons_begin(0x803b4e00);
    assert(chosen_weapon()==QWEAPON_MACHINE_GUN&&csz_weapons_state()->ammo[0]==40);
    stick_y=0;tick();stick_x=-1;tick();csz_weapons_begin(0x803b4e00);
    assert(chosen_weapon()==QWEAPON_SHOTGUN&&csz_weapons_state()->ammo[2]==2&&pedal==0);
    assert(qoptions_save(options_path,&options));qoptions_load(options_path,&saved);
    assert(saved.shotgun&&!saved.machine_gun&&saved.expanded_weapons);
    /* Saved explicit OFF remains OFF and the missing-asset path safely falls
     * back; it never exposes a working-looking but unavailable shotgun. */
    options.expanded_weapons=false;apply_weapon_model();csz_weapons_begin(0x803b4e00);
    assert(chosen_weapon()==QWEAPON_MACHINE_GUN&&!options.shotgun&&csz_weapons_state()->ammo[0]==40);
    assert(qoptions_save(options_path,&options));qoptions_load(options_path,&saved);assert(!saved.expanded_weapons);
    options.expanded_weapons=true;choose_weapon(QWEAPON_SHOTGUN);test_shotgun_available=false;apply_weapon_model();
    assert(chosen_weapon()==QWEAPON_MACHINE_GUN&&test_model==QGUN_MACHINE_GUN);
    #undef TEST_W32
    puts("PASS: original controls and feedback; three-shell shotgun/12-round handgun host integration, missing-mesh fallback, cover-only four-direction neutral-rearmed selection, HUD gating, unchanged cover/aim, no held-trigger shots, action/focus recovery and persistence");
    return 0;
}
