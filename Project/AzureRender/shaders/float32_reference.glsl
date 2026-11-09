#ifndef AZURE_FLOAT32_REFERENCE_GLSL
#define AZURE_FLOAT32_REFERENCE_GLSL
// Binary32 finite FMA with nearest-even rounding. Two low-first 32-bit limbs.
// Nonfinite operands and signed all-zero inputs retain the device FMA behavior.
int float32Width64(uvec2 a) { return a.y!=0u?findMSB(a.y)+33:(a.x!=0u?findMSB(a.x)+1:0); }
bool float32Below64(uvec2 a,int n) {
    if(n<=0)return false;
    if(n<32)return (a.x&((1u<<n)-1u))!=0u;
    if(n==32)return a.x!=0u;
    if(n<64)return a.x!=0u||(a.y&((1u<<(n-32))-1u))!=0u;
    return any(notEqual(a,uvec2(0)));
}
uvec2 float32Left64(uvec2 a,int n) {
    if(n<=0)return a;
    if(n<32)return uvec2(a.x<<n,(a.y<<n)|(a.x>>(32-n)));
    if(n<64)return uvec2(0,a.x<<(n-32));
    return uvec2(0);
}
uvec2 float32Right64(uvec2 a,int n) {
    if(n<=0)return a;
    if(n<32)return uvec2((a.x>>n)|(a.y<<(32-n)),a.y>>n);
    if(n<64)return uvec2(a.y>>(n-32),0);
    return uvec2(0);
}
uvec2 float32Align64(uvec2 a,int shift) {
    if(shift>=0)return float32Left64(a,shift);
    uvec2 q=float32Right64(a,-shift);q.x|=float32Below64(a,-shift)?1u:0u;return q;
}
uint float32Round64(uvec2 a,int shift) {
    if(shift<=0)return float32Left64(a,-shift).x;
    uint q=float32Right64(a,shift).x;
    bool halfwayBit=shift<=64&&((float32Right64(a,shift-1).x&1u)!=0u);
    return q+uint(halfwayBit&&(float32Below64(a,shift-1)||(q&1u)!=0u));
}
float float32ReferenceFma(float a,float b,float c) {
    uint ab=floatBitsToUint(a),bb=floatBitsToUint(b),cb=floatBitsToUint(c);
    uint ae=(ab>>23)&255u,be=(bb>>23)&255u,ce=(cb>>23)&255u;
    if(ae==255u||be==255u||ce==255u)return fma(a,b,c);
    uint am=(ab&0x7fffffu)|(ae!=0u?0x800000u:0u);
    uint bm=(bb&0x7fffffu)|(be!=0u?0x800000u:0u);
    uint cm=(cb&0x7fffffu)|(ce!=0u?0x800000u:0u);
    if(am==0u||bm==0u)return cm!=0u?c:fma(a,b,c);
    int ep=(ae!=0u?int(ae)-150:-149)+(be!=0u?int(be)-150:-149);
    int ec=ce!=0u?int(ce)-150:-149;
    uint hi,lo;umulExtended(am,bm,hi,lo);uvec2 p=uvec2(lo,hi),v=uvec2(cm,0);
    int top=ep+float32Width64(p);if(cm!=0u)top=max(top,ec+float32Width64(v));
    int alignedExponent=top-63;
    p=float32Align64(p,ep-alignedExponent);v=cm!=0u?float32Align64(v,ec-alignedExponent):uvec2(0);
    uint ps=(ab^bb)&0x80000000u,cs=cb&0x80000000u,sign=ps;
    uvec2 value;
    if(ps==cs){uint low=p.x+v.x;value=uvec2(low,p.y+v.y+uint(low<p.x));}
    else{
        bool larger=p.y>v.y||(p.y==v.y&&p.x>=v.x);
        uvec2 big=larger?p:v,small=larger?v:p;sign=larger?ps:cs;
        value=uvec2(big.x-small.x,big.y-small.y-uint(big.x<small.x));
    }
    int width=float32Width64(value);if(width==0)return 0.0;
    int power=alignedExponent+width-1;uint significand;
    if(power < -126){significand=float32Round64(value,-149-alignedExponent);return uintBitsToFloat(sign|significand);}
    significand=float32Round64(value,width-24);
    if(significand==0x1000000u){significand>>=1;power+=1;}
    if(power>127)return uintBitsToFloat(sign|0x7f800000u);
    return uintBitsToFloat(sign|(uint(power+127)<<23)|(significand&0x7fffffu));
}

// A normalized mantissa (0..2^23-1) and a validated ProfileV1 code (0..2).
uint float32ProfileReciprocalBits(uint index,uint code){
    uint rounded=0x3f800000u;
    if(index!=0u){
        uint divisor=0x800000u|index,remainder=0x800000u,quotient=0u;
        for(int i=0;i<24;++i){
            remainder<<=1u;quotient<<=1u;
            if(remainder>=divisor){remainder-=divisor;quotient|=1u;}
        }
        uint twice=remainder<<1u;
        quotient+=uint(twice>divisor||(twice==divisor&&(quotient&1u)!=0u));
        rounded=0x3f000000u|(quotient&0x7fffffu);
    }
    return uint(int(rounded)+int(code)-1);
}
#endif
