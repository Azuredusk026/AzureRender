#version 450
layout(constant_id=0) const bool enabled=false;
layout(constant_id=7) const uint integerValue=3;
layout(constant_id=15) const float realValue=0.5;
layout(location=0) flat in float value;
layout(location=0) out vec4 outputValue;
void main(){outputValue=vec4(enabled?1:0,float(integerValue),realValue,value);}
