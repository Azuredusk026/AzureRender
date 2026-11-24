// Win32 publishes these empty compatibility macros before public render APIs.
#define near
#define far
#include "render/ReferenceCameraCalibration.hpp"
int main(){azurerender::ReferenceCameraView v;azurerender::PreparedReferenceCameraView p(v);
    return p.pickRay(.5,.5).direction[2]==-1&&azurerender::relocateReferenceCamera(v,{},{}).schemaVersion==1?0:1;
}
