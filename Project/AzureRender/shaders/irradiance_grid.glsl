#ifndef AZURE_IRRADIANCE_GRID_GLSL
#define AZURE_IRRADIANCE_GRID_GLSL
// The caller owns the irradianceCells storage buffer and its lifetime.
vec4 evaluateIrradianceGrid(vec3 position,vec3 normal,vec4 originSpacing,
    ivec3 dims,float intensity,bool quadratic) {
    vec3 q=(position-originSpacing.xyz)/originSpacing.w;
    if(any(lessThan(q,vec3(0)))||any(greaterThan(q,vec3(dims-ivec3(1)))))return vec4(0);
    ivec3 lo=ivec3(floor(q)),hi=min(lo+ivec3(1),dims-ivec3(1));
    vec3 f=q-vec3(lo);
    vec4 red=vec4(0),green=vec4(0),blue=vec4(0);
    vec4 redQuadratic=vec4(0),greenQuadratic=vec4(0),blueQuadratic=vec4(0);
    vec3 tail=vec3(0);
    float weight=0;
    int stride=quadratic?8:4;
    for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x){
        ivec3 p=ivec3(x!=0?hi.x:lo.x,y!=0?hi.y:lo.y,z!=0?hi.z:lo.z);
        int base=stride*(p.x+dims.x*(p.y+dims.y*p.z));
        vec4 validity=irradianceCells[base+(quadratic?6:3)];
        float w=(x!=0?f.x:1-f.x)*(y!=0?f.y:1-f.y)*(z!=0?f.z:1-f.z)*(quadratic?validity.w:validity.x);
        red+=irradianceCells[base]*w;
        green+=irradianceCells[base+(quadratic?2:1)]*w;
        blue+=irradianceCells[base+(quadratic?4:2)]*w;
        if(quadratic){
            redQuadratic+=irradianceCells[base+1]*w;
            greenQuadratic+=irradianceCells[base+3]*w;
            blueQuadratic+=irradianceCells[base+5]*w;
            tail+=validity.xyz*w;
        }
        weight+=w;
    }
    vec4 basis=vec4(normal,1);
    vec3 value=vec3(dot(red,basis),dot(green,basis),dot(blue,basis));
    if(quadratic){
        vec4 terms=vec4(normal.x*normal.y,normal.y*normal.z,normal.z*normal.z,normal.x*normal.z);
        value+=vec3(dot(redQuadratic,terms),dot(greenQuadratic,terms),dot(blueQuadratic,terms))
            +tail*(normal.x*normal.x-normal.y*normal.y);
    }
    return vec4(max(value,vec3(0))*intensity,clamp(weight,0,1));
}
#endif
