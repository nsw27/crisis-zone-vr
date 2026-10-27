/* Runs the actual game's init, firing decision, reload and ammo-decrement code.
 * Inputs below are synthetic cabinet/player state, not a real gameplay capture. */
enum { GP_ADDR=0x803b4e00u, PLAYER_ADDR=0x80549844u, OWNER_ADDR=0x805495a0u };
static void invoke(void (*f)(uint32_t),uint32_t ep,uint32_t a0){
    assert(!depth);rr_budget=10000000;RS8(0xE0,GP_ADDR);RS8(0xE8,0x80f00000u);
    RS8(0xF8,0xdeadbefcu);RS8(0x20,a0);rr_call_push(0xdeadbefcu);f(ep);assert(!depth);
}
static void init_state(bool expanded,bool handgun){
    memset(R,0,sizeof R);memset(tc2_ram,0,sizeof tc2_ram);
    csz_weapons_configure(expanded,handgun);
    invoke(L_80090930_at,0x80090930,PLAYER_ADDR);
    assert(MRD4(PLAYER_ADDR+4)==40);
    MWR4(GP_ADDR+0x55f4,PLAYER_ADDR);MWR4(GP_ADDR+0x55f8,OWNER_ADDR);
    MWR4(GP_ADDR+0x5d4c,PLAYER_ADDR);MWR4(GP_ADDR+0x56b0,PLAYER_ADDR);
    MWR4(PLAYER_ADDR,3);MWR4(PLAYER_ADDR+0x30,0);MWR4(GP_ADDR+0x5cdc,1);
    /* Exposed owner at the end of its animation: original eligibility accepts fire. */
    MWR4(OWNER_ADDR+0x78,3);MWR4(OWNER_ADDR+0x124,18);MWR4(OWNER_ADDR+0x114,17);
    csz_weapons_begin(GP_ADDR);
}
static void frame(bool trigger_down){
    /* The actual gun-input layer's held-trigger flag is bit 0 of this byte. */
    MWR1(0x803bad60,trigger_down?1:0);
    MWR2(GP_ADDR+0x5320,0); /* normal aim path, no out-of-screen/special calibration */
    invoke(L_800947F0_at,0x800947f0,0);
}
#ifndef QTEST_COMPARE
int main(void){
    init_state(false,false);unsigned before=MRD4(PLAYER_ADDR+4);
    frame(false);assert(MRD4(PLAYER_ADDR+4)==before);
    for(int i=0;i<100;i++)frame(true);
    fprintf(stderr,"BASELINE held fire ammo=%u\n",MRD4(PLAYER_ADDR+4));
    assert(MRD4(PLAYER_ADDR+4)<40); /* real firing path is reached */
    init_state(true,true);assert(MRD4(PLAYER_ADDR+4)==12);
    csz_weapons_sample(true,true,1);frame(true);assert(MRD4(PLAYER_ADDR+4)==12);
    csz_weapons_sample(true,true,0);frame(false);csz_weapons_sample(true,true,1);
    for(int i=0;i<100;i++)frame(true);
    fprintf(stderr,"HANDGUN held fire ammo=%u shadow=%d fault=%d\n",MRD4(PLAYER_ADDR+4),csz_weapons_state()->ammo[1],csz_weapons_state()->fault);
    assert(MRD4(PLAYER_ADDR+4)==11&&csz_weapons_state()->ammo[1]==11&&!csz_weapons_state()->fault);
    assert(csz_weapons_feedback()&&!csz_weapons_feedback());
    for(int shot=1;shot<12;shot++){
        csz_weapons_sample(true,true,0);frame(false);csz_weapons_sample(true,true,1);
        for(int i=0;i<12;i++)frame(true);
        assert(MRD4(PLAYER_ADDR+4)==11u-shot&&!csz_weapons_state()->fault);
    }
    csz_weapons_sample(true,true,0);frame(false);csz_weapons_sample(true,true,1);frame(true);
    assert(MRD4(PLAYER_ADDR+4)==0&&!csz_weapons_state()->fault);
    /* Actual reload function restores 12, not 40, without a custom timer. */
    invoke(L_80091484_at,0x80091484,0);assert(MRD4(PLAYER_ADDR+4)==12);
    csz_weapons_configure(true,false);frame(false);assert(MRD4(PLAYER_ADDR+4)==40);
    for(int i=0;i<20;i++)frame(true);
    unsigned mg=MRD4(PLAYER_ADDR+4);assert(mg<40&&!csz_weapons_state()->fault);
    csz_weapons_configure(true,true);frame(false);assert(MRD4(PLAYER_ADDR+4)==12);
    csz_weapons_configure(true,false);frame(false);assert(MRD4(PLAYER_ADDR+4)==mg);
    csz_weapons_configure(true,true);frame(false);invoke(L_800913F0_at,0x800913f0,0);
    assert(MRD4(PLAYER_ADDR+4)==12&&csz_weapons_state()->ammo[0]==40);
    assert(MRD4(PLAYER_ADDR)==3); /* weapon rules do not increase player life */
    puts("PASS: actual lifted init/update/fire/reload/reset functions; held handgun produces one accepted shot, 12-shot exhaustion, original HUD ammo word, independent magazines; synthetic inputs, graphics/audio mocked");
    return 0;
}

#else
int main(int argc,char **argv){
    assert(argc==2);FILE *f=fopen(argv[1],"wb");assert(f);
    for(int mode=0;mode<2;mode++){
        init_state(false,false);
        for(int i=0;i<320;i++){
            /* Original machine gun, empty magazine, shielding and actual reload
             * boundary, plus continuation/reset. Record registers and game RAM. */
            if(i%79==0)invoke(L_800913F0_at,0x800913f0,0);
            if(i%43==0){MWR4(OWNER_ADDR+0x78,1);MWR4(OWNER_ADDR+0x114,8);}
            else{MWR4(OWNER_ADDR+0x78,3);MWR4(OWNER_ADDR+0x114,17);}
            MWR4(PLAYER_ADDR+0x30,mode?0x2:0);
            frame(i%17<12);
            assert(fwrite(R,1,sizeof R,f)==sizeof R);
            assert(fwrite(tc2_ram+0x3b0000,1,0x10000,f)==0x10000);
            assert(fwrite(tc2_ram+0x5495a0,1,0x400,f)==0x400);
            assert(fwrite(&rr_budget,1,sizeof rr_budget,f)==sizeof rr_budget);
        }
    }
    assert(!fclose(f));puts("PASS: 640 default-mode game updates recorded for original/modified comparison");
}
#endif
