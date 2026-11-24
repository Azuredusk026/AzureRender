#version 450
layout(push_constant) uniform Input {vec4 color;vec4 parameters;} inputData;
layout(location=0) out vec4 color;
layout(location=1) out vec4 normal;
layout(location=2) out vec4 mask;
void main(){color=inputData.color;normal=vec4(.25,.5,.75,1);mask=vec4(1);}
