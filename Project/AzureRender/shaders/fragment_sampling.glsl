#ifndef AZURE_FRAGMENT_SAMPLING_GLSL
#define AZURE_FRAGMENT_SAMPLING_GLSL
#if defined(AZURE_GEOMETRY_SAMPLES)
#define AZURE_SAMPLE sample
#define AZURE_FRAGMENT_COORD vec4(floor(gl_FragCoord.xy)+gl_SamplePosition,gl_FragCoord.zw)
#else
#define AZURE_SAMPLE
#define AZURE_FRAGMENT_COORD gl_FragCoord
#endif
#endif
