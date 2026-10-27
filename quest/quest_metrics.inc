/* Small head-relative diagnostic panel. Separate from the options texture;
 * text updates at most twice a second, never once per eye. All numbers are
 * measured counters/timers, not a requested refresh rate presented as FPS. */
static GLuint metrics_texture;
static unsigned char metrics_pixels[512*80*4];
static char metrics_lines[4][64];
static bool metrics_dirty=true;
void qui_metrics_update(const char *a,const char *b,const char *c,const char *d){
    const char *rows[]={a,b,c,d};
    for(int i=0;i<4;i++)if(strcmp(metrics_lines[i],rows[i])){snprintf(metrics_lines[i],64,"%s",rows[i]);metrics_dirty=true;}
}
void qui_draw_metrics(const float view[16],const float projection[16],V3 head,Q4 rotation){
    if(!program)return;
    GLint unit,bound,unpack;glGetIntegerv(GL_ACTIVE_TEXTURE,&unit);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);
    if(!metrics_texture)glGenTextures(1,&metrics_texture);glBindTexture(GL_TEXTURE_2D,metrics_texture);
    if(metrics_dirty){
        for(int i=0;i<512*80;i++){metrics_pixels[4*i]=8;metrics_pixels[4*i+1]=15;metrics_pixels[4*i+2]=21;metrics_pixels[4*i+3]=230;}
        for(int line=0;line<4;line++)for(int k=0;k<62&&metrics_lines[line][k];k++){
            unsigned c=(unsigned char)metrics_lines[line][k];if(c<32||c>127)continue;
            const uint8_t *glyph=builtin_font+(c-32)*128;
            for(int y=0;y<16;y+=2)for(int x=0;x<16;x+=2){
                unsigned shade=glyph[y*8+x/2]>>4;if(shade==15)continue;
                unsigned char *p=metrics_pixels+4*((8+line*17+y/2)*512+8+k*8+x/2);
                int a=15-shade;p[0]=(210*a+p[0]*shade)/15;p[1]=(240*a+p[1]*shade)/15;p[2]=(240*a+p[2]*shade)/15;p[3]=255;
            }
        }
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,&unpack);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,512,80,0,GL_RGBA,GL_UNSIGNED_BYTE,metrics_pixels);glPixelStorei(GL_UNPACK_ROW_LENGTH,unpack);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);metrics_dirty=false;
    }
    const float xy[4][2]={{-.46f,.38f},{.18f,.38f},{.18f,.28f},{-.46f,.28f}},uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
    const int order[6]={0,1,2,0,2,3};float verts[6][5];
    for(int i=0;i<6;i++){int k=order[i];V3 v=add(head,rotate(rotation,v3(xy[k][0],xy[k][1],-1.25f)));verts[i][0]=v.x;verts[i][1]=v.y;verts[i][2]=v.z;verts[i][3]=uv[k][0];verts[i][4]=uv[k][1];}
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);glUniformMatrix4fv(u_view,1,GL_FALSE,view);glUniformMatrix4fv(u_projection,1,GL_FALSE,projection);glUniform1i(u_texture,0);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof verts,verts,GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),0);glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
    glDrawArrays(GL_TRIANGLES,0,6);glDisable(GL_BLEND);glBindTexture(GL_TEXTURE_2D,bound);glActiveTexture(unit);
}
