#version 450
layout(push_constant) uniform Parameters {float depth;} parameters;
layout(location=0) out vec4 color;
void main(){color=parameters.depth<.5?vec4(1,0,0,1):vec4(0,0,1,1);}
