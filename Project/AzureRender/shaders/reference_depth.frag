#version 450
layout(location=0) out float surfaceDepth;
void main(){surfaceDepth=gl_FragCoord.z;}
