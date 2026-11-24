// Linear HDR sky, fixed to world directions. Translation has no effect.
vec3 azureProceduralSky(vec3 direction) {
    // A broad east/west haze cue remains visible when the sun is off screen.
    vec3 horizon=vec3(.54,.66,.78)*(.88+.12*direction.x);
    vec3 zenith=vec3(.055,.17,.38);
    vec3 sky=mix(horizon,zenith,pow(max(direction.y,0.0),.45));
    float alignment=max(dot(direction,normalize(vec3(.48,.72,.5))),0.0);
    sky+=vec3(1.0,.66,.3)*pow(alignment,24.0)*.35;
    sky+=vec3(8.0,6.4,4.5)*smoothstep(.9995,.99985,alignment);
    vec3 ground=mix(vec3(.12,.14,.17),horizon,exp(min(direction.y,0.0)*18.0));
    return mix(ground,sky,smoothstep(-.025,.025,direction.y));
}
