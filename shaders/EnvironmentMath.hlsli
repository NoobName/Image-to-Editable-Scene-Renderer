#ifndef ISR_ENVIRONMENT_MATH
#define ISR_ENVIRONMENT_MATH
float3 RotateEnvironment(float3 direction,float radians){
    float s,c;sincos(radians,s,c);return float3(c*direction.x-s*direction.z,direction.y,s*direction.x+c*direction.z);
}
float3 CubeDirection(uint face,float2 uv){
    float2 p=uv*2-1;float3 d;
    if(face==0)d=float3(1,-p.y,-p.x);
    else if(face==1)d=float3(-1,-p.y,p.x);
    else if(face==2)d=float3(p.x,1,p.y);
    else if(face==3)d=float3(p.x,-1,-p.y);
    else if(face==4)d=float3(p.x,-p.y,1);
    else d=float3(-p.x,-p.y,-1);
    return normalize(d);
}
float2 DirectionUv(float3 d){return float2(atan2(d.z,d.x)/6.28318530718+0.5,acos(clamp(d.y,-1,1))/3.14159265359);}
float2 Hammersley(uint i,uint count){return float2(float(i)/count,float(reversebits(i))*2.3283064365386963e-10);}
float3 TangentToWorld(float3 local,float3 n){
    float3 up=abs(n.z)<0.999?float3(0,0,1):float3(1,0,0);
    float3 t=normalize(cross(up,n)),b=cross(n,t);return normalize(t*local.x+b*local.y+n*local.z);
}
float3 SampleGGX(float2 xi,float roughness,float3 n){
    float alpha=max(roughness,0.045)*max(roughness,0.045),a2=alpha*alpha;
    float phi=6.28318530718*xi.x,cosTheta=sqrt((1-xi.y)/(1+(a2-1)*xi.y));
    float sinTheta=sqrt(max(0,1-cosTheta*cosTheta));
    return TangentToWorld(float3(cos(phi)*sinTheta,sin(phi)*sinTheta,cosTheta),n);
}
#endif
