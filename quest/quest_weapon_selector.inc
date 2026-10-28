#include "quest_weapon_colours.h"
/* Cover-only informational loadout strip. Uses the existing UI shader/font,
 * one small texture updated only when the confirmed state or request changes.
 * No game writes, new aiming marker, shield replacement or firing side effects. */
enum { WEAPON_STRIP_W=640, WEAPON_STRIP_H=128 };
static GLuint weapon_selector_texture;
static uint8_t weapon_selector_pixels[WEAPON_STRIP_W*WEAPON_STRIP_H*4];
static bool weapon_selector_visible,weapon_selector_dirty=true;
static int weapon_selector_selected=-1,weapon_selector_requested=-1;
static int weapon_selector_ammo[QWEAPON_COUNT];
void qui_weapons_update(const QWeapons *s,bool visible){
    bool valid=visible&&s&&s->enabled&&s->ready&&!s->fault&&
        s->selected>=0&&s->selected<QWEAPON_COUNT&&
        s->requested>=0&&s->requested<QWEAPON_COUNT;
    if(valid)for(int i=0;i<QWEAPON_COUNT;i++)
        if(s->ammo[i]<0||s->ammo[i]>qweapons_capacity((QWeapon)i))valid=false;
    weapon_selector_visible=valid;
    if(!valid)return;
    if(weapon_selector_selected!=(int)s->selected||weapon_selector_requested!=(int)s->requested)
        weapon_selector_dirty=true;
    weapon_selector_selected=s->selected;weapon_selector_requested=s->requested;
    for(int i=0;i<QWEAPON_COUNT;i++)if(weapon_selector_ammo[i]!=s->ammo[i]){
        weapon_selector_ammo[i]=s->ammo[i];weapon_selector_dirty=true;
    }
}
static void weapon_strip_rect(int x,int y,int w,int h,uint8_t r,uint8_t g,uint8_t b){
    for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++){
        if(xx<0||xx>=WEAPON_STRIP_W||yy<0||yy>=WEAPON_STRIP_H)continue;
        uint8_t *p=weapon_selector_pixels+4*(yy*WEAPON_STRIP_W+xx);
        p[0]=r;p[1]=g;p[2]=b;p[3]=245;
    }
}
static void weapon_strip_text(int x,int y,const char *s,bool small,uint8_t r,uint8_t g,uint8_t b){
    int stride=small?2:1,size=16/stride;
    for(;*s;s++,x+=size){
        unsigned c=(unsigned char)*s;if(c<32||c>127)continue;
        const uint8_t *glyph=builtin_font+(c-32)*128;
        for(int row=0;row<size;row++)for(int col=0;col<size;col++){
            int xx=x+col,yy=y+row;if(xx<0||xx>=WEAPON_STRIP_W||yy<0||yy>=WEAPON_STRIP_H)continue;
            int gx=col*stride,gy=row*stride;uint8_t packed=glyph[gy*8+gx/2];
            int shade=(gx&1)?packed&15:packed>>4;if(shade==15)continue;
            uint8_t *p=weapon_selector_pixels+4*(yy*WEAPON_STRIP_W+xx);int a=15-shade;
            p[0]=(r*a+p[0]*shade)/15;p[1]=(g*a+p[1]*shade)/15;p[2]=(b*a+p[2]*shade)/15;p[3]=255;
        }
    }
}
static void weapon_selector_upload(void){
    weapon_strip_rect(0,0,640,128,10,18,25);
    weapon_strip_text(112,7,"IN COVER - SELECT WEAPON",false,220,230,240);
    /* Only implemented weapons are advertised. A later shotgun needs real
     * damage/asset integration before it becomes a selectable card. */
    for(int i=0;i<QWEAPON_COUNT;i++){
        int x=16+i*312;bool chosen=i==weapon_selector_requested;
        const uint8_t *colour=q_arcade_weapon_colours[i];
        weapon_strip_rect(x,30,296,74,colour[0],colour[1],colour[2]);
        weapon_strip_rect(x+2,32,292,70,chosen?28:14,chosen?37:25,chosen?48:32);
        if(chosen)weapon_strip_rect(x+7,36,4,60,255,235,150);
        weapon_strip_text(x+12,39,i==QWEAPON_MACHINE_GUN?"MACHINE GUN":"HANDGUN",false,colour[0],colour[1],colour[2]);
        char rounds[32];snprintf(rounds,sizeof rounds,"%d / %d",weapon_selector_ammo[i],qweapons_capacity((QWeapon)i));
        weapon_strip_text(x+12,62,rounds,false,220,230,240);
        const char *status=chosen&&weapon_selector_selected!=weapon_selector_requested?"SWITCHING...":
            weapon_selector_ammo[i]==0?"EMPTY - COVER RELOAD REQUIRED":
            i==QWEAPON_MACHINE_GUN?"HOLD TRIGGER: AUTOMATIC":"FRESH TRIGGER: SINGLE SHOT";
        weapon_strip_text(x+12,86,status,true,chosen?255:181,chosen?222:207,chosen?106:220);
    }
    weapon_strip_text(56,112,"RIGHT STICK: UP/RIGHT NEXT - DOWN/LEFT PREVIOUS",true,181,207,220);
    GLint row_length;glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row_length);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    if(!weapon_selector_texture){
        glGenTextures(1,&weapon_selector_texture);glBindTexture(GL_TEXTURE_2D,weapon_selector_texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,WEAPON_STRIP_W,WEAPON_STRIP_H,0,GL_RGBA,GL_UNSIGNED_BYTE,weapon_selector_pixels);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }else{
        glBindTexture(GL_TEXTURE_2D,weapon_selector_texture);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,WEAPON_STRIP_W,WEAPON_STRIP_H,GL_RGBA,GL_UNSIGNED_BYTE,weapon_selector_pixels);
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH,row_length);weapon_selector_dirty=false;
}
void qui_draw_weapons(const float view[16],const float projection[16],V3 head,Q4 rotation){
    if(!program||!weapon_selector_visible)return;
    GLint unit,bound;glGetIntegerv(GL_ACTIVE_TEXTURE,&unit);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);
    if(weapon_selector_dirty||!weapon_selector_texture)weapon_selector_upload();
    else glBindTexture(GL_TEXTURE_2D,weapon_selector_texture);
    /* Low in the view, below the unchanged central aiming dot. The same plane
     * is projected independently through each eye, not stamped onto both eyes. */
    const float xy[4][2]={{-.52f,-.28f},{.52f,-.28f},{.52f,-.488f},{-.52f,-.488f}};
    const float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};const int order[6]={0,1,2,0,2,3};float verts[6][5];
    for(int i=0;i<6;i++){int k=order[i];V3 pos=add(head,rotate(rotation,v3(xy[k][0],xy[k][1],-1.6f)));
        verts[i][0]=pos.x;verts[i][1]=pos.y;verts[i][2]=pos.z;verts[i][3]=uv[k][0];verts[i][4]=uv[k][1];}
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);glUniformMatrix4fv(u_view,1,GL_FALSE,view);glUniformMatrix4fv(u_projection,1,GL_FALSE,projection);
    glUniform1i(u_texture,0);glUniform2f(u_cursor,-10,-10); /* no extra reticle */
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof verts,verts,GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),0);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
    glDrawArrays(GL_TRIANGLES,0,6);glDisable(GL_BLEND);glBindTexture(GL_TEXTURE_2D,bound);glActiveTexture(unit);
}
static void weapon_selector_shutdown(void){
    glDeleteTextures(1,&weapon_selector_texture);weapon_selector_texture=0;
    weapon_selector_visible=false;weapon_selector_dirty=true;weapon_selector_selected=weapon_selector_requested=-1;
}
