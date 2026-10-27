#include "quest_menu_pointer.h"
#include <assert.h>
#include <stdio.h>
static V3 point(float x,float y){return v3((x/640.f-.5f)*1.3f,(.5f-y/480.f)*.975f,-1.6f);}
int main(void){
    int checks=0;
    for(int hand=0;hand<2;hand++)for(int turn=0;turn<5;turn++)for(int page=0;page<QOPTIONS_PAGE_COUNT;page++)
    for(int row=0;row<qoptions_row_count(page);row++)for(int zone=0;zone<3;zone++){
        Q4 rot={0,sinf(turn*.2f),0,cosf(turn*.2f)};V3 head=v3(.4f,1.6f,-.3f);
        float px=zone==0?200:zone==1?546:604,py=(float)qmenu_row_y(page,row)+8;
        V3 o=add(head,rotate(rot,v3(hand?.25f:-.25f,-.2f,-.4f)));
        V3 target=add(head,rotate(rot,point(px,py)));float x,y;
        assert(qmenu_project(head,rot,o,sub(target,o),&x,&y));assert(fabsf(x-px)<.01f&&fabsf(y-py)<.01f);
        int direction;assert(qmenu_row_at(page,x,y,&direction)==row);assert(direction==(zone==0?0:zone==1?-1:1));checks++;
    }
    float x,y;Q4 id={0,0,0,1};
    assert(!qmenu_project(v3(0,0,0),id,v3(0,0,0),v3(0,0,1),&x,&y));
    assert(!qmenu_project(v3(0,0,0),id,v3(0,0,0),v3(1,0,0),&x,&y));
    assert(!qmenu_project(v3(0,0,0),id,v3(NAN,0,0),v3(0,0,-1),&x,&y));
    assert(!qmenu_project(v3(0,0,0),(Q4){0,0,0,0},v3(0,0,0),v3(0,0,-1),&x,&y));
    assert(!qmenu_project(v3(0,0,0),id,v3(0,0,-2),v3(0,0,-1),&x,&y));
    assert(!qmenu_project(v3(0,0,0),id,v3(0,0,0),v3(1,0,-1),&x,&y));
    assert(qmenu_page_at(36,58)==-1&&qmenu_page_at(604,58)==1&&qmenu_page_at(320,58)==0);
    int direction;assert(qmenu_row_at(0,200,50,&direction)==-1&&qmenu_row_at(0,200,120,&direction)==-1);
    QMenuClick state={.hand=-1};
    assert(!qmenu_click(&state,true,0,1));assert(!qmenu_click(&state,true,0,0));assert(qmenu_click(&state,true,0,1));
    assert(!qmenu_click(&state,true,0,1));assert(!qmenu_click(&state,true,0,.4f));assert(!qmenu_click(&state,true,0,1));
    assert(!qmenu_click(&state,true,0,0));assert(qmenu_click(&state,true,0,1));assert(!qmenu_click(&state,false,0,0));
    assert(!qmenu_click(&state,true,0,1));assert(!qmenu_click(&state,true,0,0));assert(!qmenu_click(&state,true,1,1));
    assert(!qmenu_click(&state,true,1,0));assert(qmenu_click(&state,true,1,1));assert(!qmenu_click(&state,true,1,NAN));
    assert(!qmenu_click(&state,true,1,1));
    printf("PASS: %d pointer/button projections in both hands and rotated panels; fresh-press, off-panel, tracking and handoff safety\n",checks);
}
