#include "Fullscreen.hlsli"
Texture2D<float4> positionMap:register(t0);
Texture2D<float4> normalMap:register(t1);
Texture2D<uint> validityMap:register(t2);
Texture2D<float4> evidenceMap:register(t3);
Texture2D<float> depthMap:register(t4);
cbuffer VisibilityData:register(b0){row_major float4x4 lightVP;float3 travel;float normalBias;float depthBias;uint radius;uint available;float pad;};
float4 PSEvaluate(float4 screen:SV_Position):SV_TARGET {
    int2 p=int2(screen.xy);float3 n=normalMap.Load(int3(p,0)).xyz;
    if(!available||!validityMap.Load(int3(p,0))||evidenceMap.Load(int3(p,0)).w==0||dot(n,n)<.5)return float4(1,0,0,1);
    n=normalize(n);float facing=dot(n,-normalize(travel));
    if(facing<=.1)return float4(1,0,0,1); // Unobserved backsides cannot certify a receiver.
    float3 pos=positionMap.Load(int3(p,0)).xyz+n*normalBias*(1-saturate(facing));
    float4 clip=mul(float4(pos,1),lightVP);float3 q=clip.xyz/clip.w;
    float2 uv=q.xy*float2(.5,-.5)+.5;
    uint w,h;depthMap.GetDimensions(w,h);
    if(any(uv<=0)||any(uv>=1)||q.z<=0||q.z>=1)return float4(1,0,q.z,1);
    float2 dx=ddx(uv),dy=ddy(uv);float det=dx.x*dy.y-dx.y*dy.x;
    float2 slope=abs(det)>1e-12?float2(dy.y*ddx(q.z)-dx.y*ddy(q.z),dx.x*ddy(q.z)-dy.x*ddx(q.z))/det:0;
    float visible=0,count=0;int2 center=int2(uv*float2(w,h));float closest=1;
    for(int y=-int(radius);y<=int(radius);++y)for(int x=-int(radius);x<=int(radius);++x){
        int2 tap=center+int2(x,y);if(any(tap<0)||tap.x>=int(w)||tap.y>=int(h))return float4(1,0,q.z,1);
        float z=depthMap.Load(int3(tap,0));float2 offset=(float2(tap)+.5)/float2(w,h)-uv;
        // Clamp receiver-plane extrapolation near depth discontinuities; never extrapolate geometry there.
        float bias=depthBias+min(abs(dot(slope,offset)),.002);
        visible+=(q.z-bias<=z);count++;closest=min(closest,z);
    }
    return float4(visible/count,1,q.z,closest);
}
