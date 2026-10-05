#ifndef ISR_SHADOW_SAMPLING
#define ISR_SHADOW_SAMPLING
Texture2D<float> shadowMap:register(t6);
float DirectionalVisibility(float3 position,float3 geometricNormal,float3 lightDirection){
    float facing=saturate(dot(geometricNormal,lightDirection));
    float3 receiver=position+geometricNormal*shadowParams.y*(1-facing);
    float4 clipPosition=mul(float4(receiver,1),lightViewProjection);
    float3 projected=clipPosition.xyz/clipPosition.w;
    float2 uv=projected.xy*float2(0.5,-0.5)+0.5;
    // Reconstruct the receiver plane's depth slope in shadow UV space. Comparing
    // every PCF tap with the center depth incorrectly shadows a sloping plane.
    // Evaluate derivatives before the bounds branch so a pixel quad stays valid.
    float2 dx=ddx(uv),dy=ddy(uv);
    float dzdx=ddx(projected.z),dzdy=ddy(projected.z);
    float determinant=dx.x*dy.y-dx.y*dy.x;
    float2 slope=abs(determinant)>1e-10
        ?float2(dzdx*dy.y-dzdy*dx.y,dzdy*dx.x-dzdx*dy.x)/determinant:0;
    if(any(uv<0)||any(uv>1)||projected.z<=0||projected.z>=1)return 1;
    int width=(int)shadowInfo.y;int2 pixel=int2(uv*width);
    float visibility=0;int radius=(int)shadowParams.w;
    // Compare each depth sample before averaging: PCF is not a blur of raw depth.
    for(int y=-radius;y<=radius;++y)for(int x=-radius;x<=radius;++x){
        int2 tap=pixel+int2(x,y);
        float depth=any(tap<0)||any(tap>=width)?1:shadowMap.Load(int3(tap,0));
        float2 offset=(float2(tap)+0.5)*shadowParams.z-uv;
        float receiverDepth=projected.z+dot(slope,offset)-shadowParams.x;
        visibility+=receiverDepth<=depth?1:0;
    }
    return visibility/((radius*2+1)*(radius*2+1));
}
#endif
