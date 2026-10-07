"""Float64 CPU oracle for bounded direct-only visibility transport (not a model)."""
import numpy as np


def smooth(a,b,x):
    t=np.clip((x-a)/(b-a),0,1)
    return t*t*(3-2*t)


def decode(x):
    return np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4)


def change(encoded,normal,old,new,evidence,residual,source,target,*,epsilon=.02,strength=1,
           confidence=1,max_delta=2,protection=0,enabled=True):
    encoded=np.asarray(encoded,np.float64);normal=np.asarray(normal,np.float64)
    I=decode(encoded)
    if not enabled or strength==0 or confidence==0 or source==target:
        return np.zeros_like(I),np.zeros(I.shape[:-1])
    directions=np.array([source['direction'],target['direction']],np.float64)
    if np.any(np.linalg.norm(directions,axis=1)<1e-5):return np.zeros_like(I),np.zeros(I.shape[:-1])
    normal=normal/np.maximum(np.linalg.norm(normal,axis=-1,keepdims=True),1e-12)
    cs=np.clip(normal@(-directions[0]/np.linalg.norm(directions[0])),0,1)
    ct=np.clip(normal@(-directions[1]/np.linalg.norm(directions[1])),0,1)
    ds=cs[...,None]*source['directIntensity']*np.array(source['directColor'])
    dt=ct[...,None]*target['directIntensity']*np.array(target['directColor'])
    ambient=source['ambientIntensity']*np.array(source['ambientColor'])
    delta_v=new[...,0]-old[...,0]
    positive=delta_v>0
    support=evidence[...,2]*np.where(positive,evidence[...,1],(old[...,0]>=.9)&(evidence[...,0]>=.85))
    delta_v*=np.where(positive,1-np.clip(evidence[...,0],0,1),1)
    signal=smooth(.004,.04,I@np.array([.2126,.7152,.0722]))*(1-smooth(.92,.98,encoded.max(-1)))
    weight=np.clip(support*confidence,0,1)*signal*(1-np.clip(protection,0,1))*(old[...,1]!=0)*(new[...,1]!=0)
    diffuse=np.maximum(I-np.maximum(residual[...,:3],0),0)
    observed=np.where((evidence[...,1]>0)&(old[...,0]<.5),np.clip(evidence[...,0],0,1),1)
    rho=diffuse/np.maximum(ambient+ds*observed[...,None],epsilon)
    raw=rho*dt*delta_v[...,None]
    delta=np.clip(raw,-np.minimum(max_delta*I,diffuse),max_delta*I)*(strength*weight[...,None])
    return delta,weight
