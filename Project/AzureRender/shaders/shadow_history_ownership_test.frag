#version 450
layout(early_fragment_tests) in;
layout(location=0) flat in uint key;
layout(set=0,binding=0,std430) coherent buffer History { uvec2 entries[]; } history;
void main(){
    uint pixel=uint(gl_FragCoord.y)*32u+uint(gl_FragCoord.x);
    if(atomicCompSwap(history.entries[pixel].x,0xffffffffu,key)==0xffffffffu)
        history.entries[pixel].y=key^0xa5a5a5a5u;
}
