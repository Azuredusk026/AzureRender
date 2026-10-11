#ifdef _MSC_VER
#pragma warning(push, 0)
#pragma warning(disable: 4701)
#endif
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <stb_vorbis.c>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
