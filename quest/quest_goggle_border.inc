/* Optional head-relative peripheral border. No sampling of scene colour/depth,
 * no missing-geometry inference, no aim/FOV changes. Draw before the UI.
 * Each eye's asymmetric projection and orientation map into one head frame. */
static GLuint border_program,border_vao;
static GLint border_optics,border_rotation,border_mode;
static bool border_attempted;
static bool border_init(void){
    if(border_program)return true;
    if(border_attempted)return false;
    border_attempted=true;
    GLuint vs=shader(GL_VERTEX_SHADER,
        "#version 300 es\nprecision highp float;out vec2 ndc;void main(){"
        "vec2 p=gl_VertexID==0?vec2(-1,-1):gl_VertexID==1?vec2(3,-1):vec2(-1,3);"
        "ndc=p;gl_Position=vec4(p,0,1);}");
    GLuint fs=shader(GL_FRAGMENT_SHADER,
        "#version 300 es\nprecision highp float;in vec2 ndc;out vec4 frag;"
        "uniform vec4 optics;uniform mat3 eyeToHead;uniform int mode;"
        "void main(){vec3 d=eyeToHead*vec3((ndc+optics.zw)/optics.xy,-1);"
        "vec2 t=d.xy/max(-d.z,.001);float a;vec3 c;"
        "if(mode==1){float r=length(t/vec2(1.38,1.12));"
        "a=.82*smoothstep(.78,1.18,r);c=vec3(.015,.021,.025);}"
        "else{float lower=.99-.11*exp(-t.x*t.x/.045);"
        "vec2 q=abs(t)/vec2(1.40,t.y<0.?lower:1.10);vec2 s=q*q;"
        "float r=sqrt(sqrt(dot(s,s)));"
        "a=smoothstep(.84,.98,r);"
        "float seam=smoothstep(.965,.98,r)*(1.-smoothstep(.99,1.02,r));"
        "float lip=smoothstep(.90,.945,r)*(1.-smoothstep(.965,.99,r));"
        "c=vec3(.019,.025,.027)+seam*vec3(.035,.043,.047)+lip*vec3(.009); }"
        "if(d.z>=-.001)a=1.;if(a<=0.)discard;frag=vec4(c,a);}");
    if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return false;}
    GLuint p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);
    glDeleteShader(vs);glDeleteShader(fs);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){glDeleteProgram(p);fprintf(stderr,"[VIEW] optional border unavailable; keeping original view\n");return false;}
    border_program=p;glGenVertexArrays(1,&border_vao);
    border_optics=glGetUniformLocation(p,"optics");border_rotation=glGetUniformLocation(p,"eyeToHead");border_mode=glGetUniformLocation(p,"mode");
    return true;
}
void qui_draw_border(const float p[16],Q4 eye_to_head,int mode){
    if(mode<1||mode>2||!p)return;
    for(int i=0;i<16;i++)if(!isfinite(p[i]))return;
    if(p[0]<=.01f||p[5]<=.01f||fabsf(p[11]+1)>.001f)return;
    float n=eye_to_head.x*eye_to_head.x+eye_to_head.y*eye_to_head.y+eye_to_head.z*eye_to_head.z+eye_to_head.w*eye_to_head.w;
    if(!isfinite(n)||n<.99f||n>1.01f)return;
    if(!border_init())return;
    V3 x=rotate(eye_to_head,v3(1,0,0)),y=rotate(eye_to_head,v3(0,1,0)),z=rotate(eye_to_head,v3(0,0,1));
    float r[9]={x.x,x.y,x.z,y.x,y.y,y.z,z.x,z.y,z.z};
    GLint old_program,old_vao,src_rgb,dst_rgb,src_alpha,dst_alpha,eq_rgb,eq_alpha;
    GLboolean mask[4];bool depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE),scissor=glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_CURRENT_PROGRAM,&old_program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&old_vao);glGetBooleanv(GL_COLOR_WRITEMASK,mask);
    glGetIntegerv(GL_BLEND_SRC_RGB,&src_rgb);glGetIntegerv(GL_BLEND_DST_RGB,&dst_rgb);glGetIntegerv(GL_BLEND_SRC_ALPHA,&src_alpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dst_alpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&eq_rgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eq_alpha);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);
    glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE); /* preserve target alpha */
    glUseProgram(border_program);glBindVertexArray(border_vao);
    glUniform4f(border_optics,p[0],p[5],p[8],p[9]);glUniformMatrix3fv(border_rotation,1,GL_FALSE,r);glUniform1i(border_mode,mode);
    glDrawArrays(GL_TRIANGLES,0,3);
    glUseProgram(old_program);glBindVertexArray(old_vao);glColorMask(mask[0],mask[1],mask[2],mask[3]);
    glBlendFuncSeparate(src_rgb,dst_rgb,src_alpha,dst_alpha);glBlendEquationSeparate(eq_rgb,eq_alpha);
    if(depth)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
    if(blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);
    if(cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
    if(scissor)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
}
static void border_shutdown(void){
    glDeleteProgram(border_program);glDeleteVertexArrays(1,&border_vao);border_program=border_vao=0;border_attempted=false;
}
