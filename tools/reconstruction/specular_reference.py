"""Float64 directional GGX oracle; relative Lambert coefficient times pi, fixed source camera."""
import numpy as np

def unit(a):return a/np.maximum(np.linalg.norm(a,axis=-1,keepdims=True),1e-6)
def ggx(normal,position,albedo,roughness,metallic,direction,direct):
    n,v,l=unit(normal),unit(-position),unit(-np.asarray(direction,dtype=float));h=unit(v+l)
    nl=np.clip(n@l,0,1);nv=np.clip(np.sum(n*v,-1),0,1);nh=np.clip(np.sum(n*h,-1),0,1);vh=np.clip(np.sum(v*h,-1),0,1)
    alpha=np.maximum(roughness,.045)**2;a2=alpha**2;den=nh**2*(a2-1)+1
    distribution=a2/(np.pi*den**2)
    def g(x):return 2*x/np.maximum(x+np.sqrt(a2+(1-a2)*x*x),1e-7)
    f0=.04*(1-metallic[...,None])+albedo*metallic[...,None]
    fresnel=f0+(1-f0)*(1-vh[...,None])**5
    result=fresnel*(distribution*g(nl)*g(nv)/np.maximum(4*nl*nv,1e-7)*nl*np.pi)[...,None]*direct
    result[(nl<=0)|(nv<=0)]=0;return result

def surface(kind,w=129,h=97):
    fy=.5/np.tan(np.deg2rad(45)/2);k=np.array([[fy*h/w,0,.5],[0,fy,.5],[0,0,1]],np.float32)
    u,v=np.meshgrid((np.arange(w)+.5)/w,(np.arange(h)+.5)/h)
    ray=np.stack(((u-.5)/k[0,0],-(v-.5)/k[1,1],np.ones_like(u)),-1)
    valid=np.ones((h,w),bool);z=np.full((h,w),2.)
    if kind=='sphere':
        aa=np.sum(ray*ray,-1);disc=36-32*aa;valid=disc>0;z=(6-np.sqrt(np.maximum(disc,0)))/(2*aa)
    point=ray*z[...,None];normal=unit(point-[0,0,3]) if kind=='sphere' else np.broadcast_to([0.,0.,-1.],point.shape).copy()
    point[~valid]=0;normal[~valid]=0;z[~valid]=0
    return point.astype(np.float32),normal.astype(np.float32),z.astype(np.float32),valid,k
