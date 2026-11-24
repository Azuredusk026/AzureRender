#ifndef AZURE_TEXTURE_FOOTPRINT_GLSL
#define AZURE_TEXTURE_FOOTPRINT_GLSL
// Explicit isotropic LOD from the major axis of the UV derivative ellipse.
// Inputs are finite texel derivatives. Scaling bounds the quadratic terms.
float azureEllipseMajorAxisLod(vec2 dx,vec2 dy){
    const float minimumScale=1.17549435e-38;
    float scale=max(max(abs(dx.x),abs(dx.y)),max(abs(dy.x),abs(dy.y)));
    precise vec2 x=dx/max(scale,minimumScale),y=dy/max(scale,minimumScale);
    precise float a=dot(x,x),b=dot(x,y),c=dot(y,y);
    precise float eigenvalue=0.5*(a+c+sqrt((a-c)*(a-c)+4.0*b*b));
    return max(-126.0,log2(max(scale,minimumScale))+0.5*log2(max(eigenvalue,minimumScale)));
}

float azurePointMipAxisLength(vec2 derivative){
    vec2 magnitude=abs(derivative);
    return max(magnitude.x,magnitude.y)+(11.0/32.0)*min(magnitude.x,magnitude.y);
}

// Axial and diagonal screen directions estimate the largest texel footprint.
float azureAxisDiagonalMipLod(vec2 dx,vec2 dy){
    float cardinal=max(azurePointMipAxisLength(dx),azurePointMipAxisLength(dy));
    float diagonal=(11.0/16.0)*max(azurePointMipAxisLength(dx+dy),azurePointMipAxisLength(dx-dy));
    return log2(clamp(max(cardinal,diagonal),1.17549435e-38,3.40282347e38));
}
#endif
