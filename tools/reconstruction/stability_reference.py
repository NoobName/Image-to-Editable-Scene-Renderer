"""Float64 oracle for guarded source-domain sampling and bounded log edits (not runtime)."""
import numpy as np
from verify_image_ratio import decode

LUMA = np.array([.2126,.7152,.0722])


def smooth(a,b,x):
    t=np.clip((x-a)/(b-a),0,1)
    return t*t*(3-2*t)


def sample(old,new,qa,qb,depth,normal,labels,h,w,p):
    ah,aw=old.shape[:2]
    u=(np.arange(w)+.5)*aw/w;v=(np.arange(h)+.5)*ah/h
    cx,cy=np.meshgrid(u.astype(int),v.astype(int))
    values=[old,new,qa,qb];nearest=[a[cy,cx].astype(float) for a in values]
    good=(nearest[0][...,3]*nearest[1][...,3])!=0
    if not p['stability'] or (h,w)==(ah,aw):return nearest,np.zeros((h,w),bool)
    px=u-.5;py=v-.5
    px=np.where(abs(px-np.rint(px))<1e-4,np.rint(px),px);py=np.where(abs(py-np.rint(py))<1e-4,np.rint(py),py)
    bx,by=np.meshgrid(np.floor(px).astype(int),np.floor(py).astype(int))
    fx,fy=np.meshgrid(px%1,py%1)
    norm=normal[cy,cx,:3];norm=norm/np.maximum(np.linalg.norm(norm,axis=-1,keepdims=True),1e-6)
    z=depth[cy,cx];label=labels[cy,cx];sums=[np.zeros_like(a) for a in nearest]
    for oy in range(2):
        for ox in range(2):
            x=np.clip(bx+ox,0,aw-1);y=np.clip(by+oy,0,ah-1)
            n=normal[y,x,:3];n=n/np.maximum(np.linalg.norm(n,axis=-1,keepdims=True),1e-6)
            good&=(old[y,x,3]*new[y,x,3]!=0)&(labels[y,x]==label)&(np.sum(norm*n,axis=-1)>=p['normalCosineEdge'])
            good&=np.abs(z-depth[y,x])<=p['relativeDepthEdge']*np.maximum(np.minimum(z,depth[y,x]),1e-6)
            weight=(fx if ox else 1-fx)*(fy if oy else 1-fy)
            for total,a in zip(sums,values):total+=a[y,x]*weight[...,None]
    return [np.where(good[...,None],s,n) for s,n in zip(sums,nearest)],good


def response(old,new,qa,qb,source,p,protection=0):
    valid=old[...,3]*new[...,3]!=0;a=old[...,:3];b=new[...,:3]
    qa=np.clip(qa,0,1).copy();shadow=np.clip(qb[...,0],0,1)
    if not p['fitCacheValid']:qa[...,3]=.5;shadow=np.full_like(shadow,.8)
    encoded=source/255;linear=decode(encoded);y=linear@LUMA
    signal=smooth(.002,.04,y)*(1-smooth(.92,.98,encoded.max(axis=-1)))
    combined=np.minimum(np.minimum(qa.min(axis=-1),shadow),signal)
    confidence=valid*(1-protection)*(combined if p['stability'] else 1)
    if p.get('intrinsicAssisted') and p.get('intrinsicProtection',True):confidence*=np.clip(qb[...,1],0,1)
    eps=p['epsilon'];raw_y=np.log((b@LUMA+eps)/(a@LUMA+eps))
    chroma=np.log((b+eps)/(a+eps))-raw_y[...,None]
    if p['colorMode']=='luminance':chroma*=0
    raw=raw_y[...,None]+chroma;lo,hi=p['ratioClamp']
    if p['stability']:
        support=np.minimum(smooth(.01,.12,a@LUMA),smooth(.002,.04,y))
        brighten=min(1.15,hi)+(hi-min(1.15,hi))*support
        limit=np.clip(raw_y,np.log(lo),np.log(brighten))[...,None]+np.clip(chroma,-np.log(p['chromaLimit']),np.log(p['chromaLimit']))
        limit=np.clip(limit,np.log(lo),np.log(hi))
    else:limit=np.log(np.clip(np.exp(raw_y)[...,None]*np.clip(np.exp(chroma),.5,2),lo,hi))
    effective=limit*(p['strength']*confidence)[...,None]
    unchanged=(~valid)|np.all(a==b,axis=-1)|(confidence==0)|(p['strength']==0)
    effective=np.where(unchanged[...,None],0,effective)
    return np.exp(effective),confidence,linear*np.exp(effective),raw,effective
