precision highp float;
uniform lowp sampler2D s_tex_uv;
varying vec4 v_tcoord;
void main()
{
   vec4 src = texture2D(s_tex_uv, v_tcoord.xy);
   gl_FragColor = vec4(1.0) - src;
}
