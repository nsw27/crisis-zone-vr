/* Real production host against the original deterministic OpenXR mock. */
#define main legacy_handedness_main
#include "handedness_fixture.c"
#undef main
static QGunKind test_model;
bool qgun_available(QGunKind k){return k==QGUN_PISTOL||k==QGUN_MACHINE_GUN;}
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
    assert(!options.left_handed);instance=(XrInstance)(uintptr_t)1;session=(XrSession)(uintptr_t)1;
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
    for(int page=0;page<3;page++){
        assert(options_page==page);
        for(int row=0;row<qoptions_row_count(page);row++){
            assert(menu_selected==row);distance_y=-1;tick();tick();
            assert(menu_selected==(row+1)%qoptions_row_count(page));distance_y=0;tick();
        }
        distance_x=1;tick();assert(options_page==(page+1)%3);tick();assert(options_page==(page+1)%3);
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
    /* Illegal inputs cannot generate undefined float/integer conversions. */
    nx=NAN;ny=INFINITY;aim_valid=true;ss22_input_update();assert(g_ss22_gun_off);
    nx=-1000;ny=1000;ss22_input_update();assert(g_ss22_gun_x==64&&g_ss22_gun_y==255&&g_ss22_gun_off);
    nx=1;ny=0;ss22_input_update();assert(g_ss22_gun_x==831&&g_ss22_gun_y==32);
    /* Trigger NaN cancels; reconnect/finite recovery needs a new release. */
    paused=false;options.physical_crouch=false;reset_input_edges();triggers[0]=triggers[1]=0;tick();
    triggers[weapon_hand()]=1;tick();assert(trigger==1);triggers[weapon_hand()]=NAN;tick();assert(trigger==0);
    triggers[weapon_hand()]=1;tick();assert(trigger==0);triggers[weapon_hand()]=0;tick();triggers[weapon_hand()]=1;tick();assert(trigger==1);
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
    puts("PASS: original handoff/cover/tracking controls; new menu rows/pages/edit/reset/persistence; invalid input; output feedback lifecycle");
    return 0;
}
