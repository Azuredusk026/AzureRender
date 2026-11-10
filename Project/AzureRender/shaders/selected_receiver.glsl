struct AzureSelectedShadowReceiver {vec3 currentPosition,nextPosition;vec2 currentGradient,nextGradient;};
AzureSelectedShadowReceiver azureEmptyShadowReceiver(){return AzureSelectedShadowReceiver(vec3(0),vec3(0),vec2(0),vec2(0));}
