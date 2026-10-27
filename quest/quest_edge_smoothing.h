/* Original spatial edge filter, composed inside the existing gamma resolve.
 * No history buffer, camera jitter, temporal lag or extra draw pass. Text-marked
 * pixels and their neighbours are conservatively protected by the priority map.
 * This is edge smoothing, not MSAA/TAA and not a texture-detail reconstruction. */
#define QSCENE_EDGE_GLSL \
    "uniform highp sampler2D t_protect;uniform int u_smooth,u_protect;\n" \
    "float edge_luma(vec3 c){return dot(c,vec3(0.299,0.587,0.114));}\n" \
    "vec3 edge_smooth(vec2 uv,vec3 c){\n" \
    "  if(u_smooth==0)return c;\n" \
    "  ivec2 sz=textureSize(t_src,0);vec2 px=1.0/vec2(sz);\n" \
    "  vec3 n=texture(t_src,uv+vec2(0,px.y)).rgb,s=texture(t_src,uv-vec2(0,px.y)).rgb;\n" \
    "  vec3 e=texture(t_src,uv+vec2(px.x,0)).rgb,w=texture(t_src,uv-vec2(px.x,0)).rgb;\n" \
    "  float lc=edge_luma(c),ln=edge_luma(n),ls=edge_luma(s),le=edge_luma(e),lw=edge_luma(w);\n" \
    "  float lo=min(lc,min(min(ln,ls),min(le,lw))),hi=max(lc,max(max(ln,ls),max(le,lw)));\n" \
    "  if(hi-lo<max(0.06,hi*0.125))return c;\n" \
    "  vec2 along=vec2(ls-ln,le-lw);float span=max(abs(along.x),abs(along.y));\n" \
    "  if(span<0.0001)return c;\n" \
    "  if(u_protect!=0){\n" \
    "    ivec2 p=ivec2(uv*vec2(sz));\n" \
    "    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)\n" \
    "      if(texelFetch(t_protect,clamp(p+ivec2(x,y),ivec2(0),sz-1),0).r>0.0)return c;\n" \
    "  }\n" \
    "  vec2 offset=along*(0.5/span)*px;\n" \
    "  vec3 a=texture(t_src,uv+offset).rgb,b=texture(t_src,uv-offset).rgb;\n" \
    "  return mix(c,(a+b)*0.5,0.5);\n" \
    "}\n"
