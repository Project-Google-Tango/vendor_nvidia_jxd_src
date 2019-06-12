attribute vec4 a_pos;
attribute vec4 a_tcoord;
varying vec4 v_tcoord;
void main()
{
   gl_Position = a_pos;
   v_tcoord = a_tcoord;
}
