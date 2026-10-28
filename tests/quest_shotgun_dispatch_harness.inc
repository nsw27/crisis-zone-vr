enum { GP_ADDR=0x803b4e00u,PLAYER_ADDR=0x80549844u,OWNER_ADDR=0x805495a0u };
static void invoke(void (*f)(uint32_t),uint32_t pc,uint32_t arg){
    assert(!depth);rr_budget=10000000;RS8(0xe0,GP_ADDR);RS8(0xe8,0x80f00000u);
    RS8(0xf8,0xdeadbefcu);RS8(0x20,arg);rr_call_push(0xdeadbefcu);f(pc);assert(!depth);
}
static void initialise(bool enabled,QWeapon weapon){
    memset(R,0,sizeof R);memset(tc2_ram,0,sizeof tc2_ram);hit_count=0;fixture_x=fixture_y=0;
    csz_weapons_select(enabled,weapon);invoke(L_80090930_at,0x80090930,PLAYER_ADDR);
    MWR4(GP_ADDR+0x55f4,PLAYER_ADDR);MWR4(GP_ADDR+0x55f8,OWNER_ADDR);
    MWR4(GP_ADDR+0x5d4c,PLAYER_ADDR);MWR4(GP_ADDR+0x56b0,PLAYER_ADDR);
    MWR4(PLAYER_ADDR,3);MWR4(PLAYER_ADDR+0x30,0);MWR4(GP_ADDR+0x5cdc,1);
    MWR4(OWNER_ADDR+0x78,3);MWR4(OWNER_ADDR+0x124,18);MWR4(OWNER_ADDR+0x114,17);
    /* Identity fixed-point camera and a nonzero focal plane. */
    for(int i=0;i<3;i++)MWR2(0x8026d60c+8*i,16384);
    float focal=512;uint32_t bits;memcpy(&bits,&focal,4);MWR4(0x803bad20,bits);
    csz_weapons_begin(GP_ADDR);
}
static void tick(float level){
    csz_weapons_sample(true,true,level);
    MWR1(0x803bad60,level>.55f?1:0);MWR2(GP_ADDR+0x5320,0x400);
    invoke(L_800947F0_at,0x800947f0,0);
    int32_t aimx=(int32_t)MRD4(GP_ADDR+0x532c),aimy=(int32_t)MRD4(GP_ADDR+0x5330);
    invoke(L_80095038_at,0x80095038,0);
    assert((int32_t)MRD4(GP_ADDR+0x532c)==aimx&&(int32_t)MRD4(GP_ADDR+0x5330)==aimy);
}
static void shell(void){tick(0);tick(1);for(int i=0;i<32;i++)tick(1);}
int main(int argc,char **argv){
    if(argc>1){
        assert(argc==3);FILE *f=fopen(argv[2],"wb");assert(f);initialise(false,QWEAPON_MACHINE_GUN);
        for(int i=0;i<128;i++){
            if(i%43==0)invoke(L_80091484_at,0x80091484,0);
            fixture_x=i-64;fixture_y=(i%40)-20;tick(i%9<5?1:0);
            fwrite(R,1,sizeof R,f);fwrite(tc2_ram+0x3b0000,1,0x10000,f);
            fwrite(tc2_ram+0x5495a0,1,0x400,f);fwrite(&rr_budget,1,sizeof rr_budget,f);
            hit_count=0;
        }assert(!fclose(f));return 0;
    }
    initialise(true,QWEAPON_SHOTGUN);assert(MRD4(PLAYER_ADDR+4)==3);
    tick(0);tick(1);
    fprintf(stderr,"FIRST shell ammo=%u hits=%d flags=%x tick=%llu\n",MRD4(PLAYER_ADDR+4),hit_count,MRD4(GP_ADDR+0x55fc),(unsigned long long)csz_weapons_state()->tick);
    assert(MRD4(PLAYER_ADDR+4)==2&&hit_count==1);
    assert(csz_weapons_feedback()&&!csz_weapons_feedback());
    /* Same update cannot dispatch a second pellet. */
    invoke(L_80095038_at,0x80095038,0);assert(hit_count==1);
    for(int i=0;i<32;i++){
        fixture_x=60;fixture_y=-35;tick(1);
        assert(MRD4(PLAYER_ADDR+4)==2&&!csz_weapons_state()->fault&&!csz_weapons_feedback());
    }
    assert(hit_count==3);
    assert(hits[0][0]==0&&hits[0][1]==10&&hits[0][2]==-512);
    assert(hits[1][0]==-9&&hits[1][1]==-5&&hits[1][2]==-512);
    assert(hits[2][0]==9&&hits[2][1]==-5&&hits[2][2]==-512);
    assert(hit_ticks[1]-hit_ticks[0]==2&&hit_ticks[2]-hit_ticks[1]==2);
    shell();shell();assert(MRD4(PLAYER_ADDR+4)==0&&hit_count==9);
    shell();assert(hit_count==9&&MRD4(PLAYER_ADDR+4)==0);
    invoke(L_80091484_at,0x80091484,0);assert(MRD4(PLAYER_ADDR+4)==3);
    tick(0);tick(1);int before=hit_count;assert(before==10);
    csz_weapons_cancel();
    for(int i=0;i<12;i++){csz_weapons_begin(GP_ADDR);invoke(L_80095038_at,0x80095038,0);}
    assert(hit_count==before);
    /* A missed/off-screen discharge is never converted into a damaging burst. */
    initialise(true,QWEAPON_SHOTGUN);tick(0);
    fixture_x=1000;tick(1);for(int i=0;i<10;i++)tick(1);assert(hit_count==0);
    /* At the edge only valid rays survive; they never wrap or pile up at x=-320. */
    initialise(true,QWEAPON_SHOTGUN);fixture_x=-318;tick(0);tick(1);for(int i=0;i<10;i++)tick(1);
    assert(hit_count==2&&hits[0][0]==-318&&hits[1][0]==-309);
    /* Cover cancels pending pellets without refunding the consumed shell. */
    initialise(true,QWEAPON_SHOTGUN);tick(0);tick(1);csz_weapons_sample(true,false,1);
    for(int i=0;i<8;i++){csz_weapons_begin(GP_ADDR);invoke(L_80095038_at,0x80095038,0);}
    assert(hit_count==1&&MRD4(PLAYER_ADDR+4)==2);
    /* Disabling/switching cancels, restores the saved MG magazine, and cannot hit later. */
    initialise(true,QWEAPON_SHOTGUN);tick(0);tick(1);csz_weapons_select(false,QWEAPON_MACHINE_GUN);
    csz_weapons_begin(GP_ADDR);assert(MRD4(PLAYER_ADDR+4)==40&&csz_weapons_state()->ammo[2]==2);
    MWR4(GP_ADDR+0x55fc,0);invoke(L_80095038_at,0x80095038,0);assert(hit_count==1);
    /* Damage-return flags are read locally, never written into the game's fire flags. */
    initialise(true,QWEAPON_SHOTGUN);tick(0);tick(1);
    MWR4(PLAYER_ADDR+0x30,4);for(int i=0;i<10;i++){csz_weapons_begin(GP_ADDR);invoke(L_80095038_at,0x80095038,0);}
    assert(hit_count==1);
    puts("PASS: real firing/ammo, fixed-point ray projection and hit dispatcher; 3 separated triangle rays per shell, 3 shells/9 calls, one feedback event, cooldown, no hold-to-repeat, reload, frozen aim offsets, no cursor writes, cancellation, edge rejection and original death gates");
}
