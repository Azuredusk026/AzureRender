#ifndef AZURE_MATERIAL_SPECIALIZATION_GLSL
#define AZURE_MATERIAL_SPECIALIZATION_GLSL
// Keep emission/neutral surface bits beside the submitted view's two bits.
uint azureSpecializedMaterialFeatures(bool defaults,uint submitted){
    return defaults?(submitted&0xc0000028U):submitted;
}
#endif
