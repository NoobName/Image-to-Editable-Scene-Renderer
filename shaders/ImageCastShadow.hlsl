#include "Fullscreen.hlsli"
#include "ColorManagement.hlsli"
Texture2D<float4> sourceMap:register(t0);
Texture2D<float4> baselineMap:register(t1);
Texture2D<float4> oldVisibility:register(t2);
Texture2D<float4> newVisibility:register(t3);
Texture2D<float4> normalMap:register(t4);
Texture2D<float4> evidenceMap:register(t5);
Texture2D<float4> residualMap:register(t6);
Texture2D<float> protectionMap:register(t7);
Texture2D<float> oldDepth:register(t8);
Texture2D<float> newDepth:register(t9);
Texture2D<float4> finalMap:register(t10);
cbuffer CastData:register(b0){
    float4 rect;uint sw,sh,aw,ah;
    float3 sourceDirection;uint enabled;
    float3 sourceDirect;float epsilon;
    float3 sourceAmbient;float strength;
    float3 targetDirection;float maxDelta;
    float3 targetDirect;float confidenceScale;
    uint view;uint same;uint maskWidth;uint maskHeight;
};
float3 Decode(float3 c){return select(c<=.04045,c/12.92,pow((c+.055)/1.055,2.4));}
float Y(float3 c){return dot(c,float3(.2126,.7152,.0722));}
struct Change {float3 delta;float confidence;};
Change Evaluate(uint2 p){
    Change c=(Change)0;float2 uv=(float2(p)+.5)/float2(sw,sh);int2 a=min(int2(uv*float2(aw,ah)),int2(aw,ah)-1);
    float4 old=oldVisibility.Load(int3(a,0)),next=newVisibility.Load(int3(a,0)),e=evidenceMap.Load(int3(a,0));
    float3 n=normalMap.Load(int3(a,0)).xyz;
    if(!enabled||same||strength==0||confidenceScale==0||old.y==0||next.y==0||dot(n,n)<.5)return c;
    n=normalize(n);float cs=saturate(dot(n,-normalize(sourceDirection))),ct=saturate(dot(n,-normalize(targetDirection)));
    float3 ds=sourceDirect*cs,dt=targetDirect*ct;
    float change=next.x-old.x;float support=e.z;
    // Removing baked darkness requires agreement with the photographed old shadow.
    // Adding darkness requires an observed new blocker and an observed previously lit receiver.
    if(change>0){support*=e.y;change*=1-saturate(e.x);}
    else {support*=step(.9,old.x)*step(.85,e.x);} // Do not stack a new mask onto unexplained baked darkness.
    float3 encoded=sourceMap.Load(int3(p,0)).rgb,I=Decode(encoded);
    float signal=smoothstep(.004,.04,Y(I))*(1-smoothstep(.92,.98,max(encoded.r,max(encoded.g,encoded.b))));
    int2 m=min(int2(uv*float2(maskWidth,maskHeight)),int2(maskWidth,maskHeight)-1);
    c.confidence=saturate(support*confidenceScale)*signal*(1-saturate(protectionMap.Load(int3(m,0))));
    if(c.confidence==0||change==0)return c;
    // R is not claimed to be emission alone: all positive non-diffuse evidence is protected.
    // Restore reflectance from the source observation, not from AI albedo texture detail.
    float3 diffuse=max(I-max(residualMap.Load(int3(a,0)).rgb,0),0);
    float observed=(e.y>0&&old.x<.5)?saturate(e.x):1;
    float3 rho=diffuse/max(sourceAmbient+ds*observed,epsilon);
    // Visibility occurs ONLY in the direct term. This additive transport is not a second ratio.
    float3 delta=rho*dt*change;
    float3 bound=maxDelta*I;
    c.delta=clamp(delta,-min(bound,diffuse),bound)*(strength*c.confidence);
    return c;
}
float4 PSComposite(float4 screen:SV_Position):SV_TARGET {
    uint2 p=uint2(screen.xy);float4 baseline=baselineMap.Load(int3(p,0));Change c=Evaluate(p);
    return float4(max(baseline.rgb+c.delta,0),baseline.a);
}
float4 PSPreview(float4 screen:SV_Position):SV_TARGET {
    float2 uv=(screen.xy-rect.xy)/rect.zw;if(any(uv<0)||any(uv>=1))return float4(.012,.016,.022,1);
    if(!enabled&&view<8)return float4(.45,.03,.45,1);
    uint2 p=min(uint2(uv*float2(sw,sh)),uint2(sw-1,sh-1));int2 a=min(int2(uv*float2(aw,ah)),int2(aw,ah)-1);
    float3 color=0;
    if(view<2){uint w,h;oldDepth.GetDimensions(w,h);int2 q=min(int2(uv*float2(w,h)),int2(w,h)-1);color=(view==0?oldDepth.Load(int3(q,0)):newDepth.Load(int3(q,0))).xxx;}
    else if(view<4){float4 v=view==2?oldVisibility.Load(int3(a,0)):newVisibility.Load(int3(a,0));color=v.y==0?float3(.45,.03,.45):v.xxx;}
    else if(view==4)color=evidenceMap.Load(int3(a,0)).xxx;
    else if(view==5)color=Evaluate(p).confidence.xxx;
    else if(view==6)color=saturate(.5+Evaluate(p).delta*2);
    else if(view==7)color=LinearToSrgb(abs(finalMap.Load(int3(p,0)).rgb-baselineMap.Load(int3(p,0)).rgb)*4);
    else if(view==8)color=LinearToSrgb(finalMap.Load(int3(p,0)).rgb);
    else if(view==9)color=LinearToSrgb(baselineMap.Load(int3(p,0)).rgb);
    return float4(color,1);
}
