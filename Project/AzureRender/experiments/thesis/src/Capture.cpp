#include "Capture.hpp"
#include <stdexcept>
#if defined(THESIS_RENDERDOC)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <renderdoc_app.h>
#endif
namespace thesis {
Capture::Capture(const std::string& library,const std::string& prefix) {
    if(prefix.empty()) return;
#if defined(THESIS_RENDERDOC)
    module=LoadLibraryA(library.c_str());
    if(!module) throw std::runtime_error("RenderDoc library load failed");
    auto get=reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(static_cast<HMODULE>(module),"RENDERDOC_GetAPI"));
    if(!get || get(eRENDERDOC_API_Version_1_6_0,&api)!=1) throw std::runtime_error("RenderDoc API unavailable");
    static_cast<RENDERDOC_API_1_6_0*>(api)->SetCaptureFilePathTemplate(prefix.c_str());
#else
    (void)library;
    throw std::runtime_error("RenderDoc capture support was not configured");
#endif
}
Capture::~Capture() {
    // The capture library stays loaded until process exit so Vulkan hooks remain valid.
}
void Capture::begin() {
#if defined(THESIS_RENDERDOC)
    if(api) { static_cast<RENDERDOC_API_1_6_0*>(api)->StartFrameCapture(nullptr,nullptr); active=true; }
#endif
}
void Capture::end() {
#if defined(THESIS_RENDERDOC)
    if(api && active) {
        active=false;
        if(!static_cast<RENDERDOC_API_1_6_0*>(api)->EndFrameCapture(nullptr,nullptr)) throw std::runtime_error("RenderDoc capture failed");
    }
#endif
}
}
