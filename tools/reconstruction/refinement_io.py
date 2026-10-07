"""Read immutable, matching Core recipe/export; publish diagnostics, never edit source."""
import copy
import hashlib
import json
import struct
from pathlib import Path
import numpy as np
from PIL import Image, ImageFilter
from appearance_contract import load_extension
from analysis_contract import load_analysis
from package_schema import read_json, validate
from scene_package import load_package, asset_path
from runtime_dds import read_dds
from pipeline.neural_refinement_backend import RefinementInput, validate_input
from pipeline.adapters.pixl_refinement import decode, encode


def digest(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def nearest(array, shape):
    h,w=shape; ah,aw=array.shape[:2]
    return array[np.minimum(((np.arange(h)+.5)*ah/h).astype(int),ah-1)[:,None],
                 np.minimum(((np.arange(w)+.5)*aw/w).astype(int),aw-1)[None,:]]


def native_confidence(path, width, height):
    # Same restricted DDS layout as runtime_dds, but this export has native dimensions.
    # Do not widen the analysis-map reader's 2048 limit for every other pipeline caller.
    path=Path(path)
    if path.stat().st_size != 148+width*height*4:raise ValueError('Native confidence DDS byte count mismatch')
    data=path.read_bytes()
    words=[124,0x100F,height,width,width*4,0,1]+[0]*11
    words += [32,4,0x30315844,0,0,0,0,0,0x1000,0,0,0,0]
    expected=b'DDS '+struct.pack('<31I',*words)+struct.pack('<5I',41,3,0,1,0)
    if data[:148]!=expected:raise ValueError('Native confidence must be restricted R32_FLOAT DDS')
    result=np.frombuffer(data,dtype='<f4',offset=148).reshape(height,width).copy()
    if not np.isfinite(result).all() or np.any(result<0) or np.any(result>1):raise ValueError('Native confidence values invalid')
    return result


def load_inputs(recipe_path, export_path, mask_path=None):
    recipe_path=Path(recipe_path).resolve(strict=True); export_path=Path(export_path).resolve(strict=True)
    recipe=read_json(recipe_path)
    schema=read_json(Path(__file__).resolve().parents[2]/'schemas/relighting-recipe.schema.json')
    validate(recipe,schema,schema=schema)
    relative=recipe['sourcePackage']
    if '\\' in relative or ':' in relative or Path(relative).is_absolute():raise ValueError('Recipe package must be a relative path')
    package=(recipe_path.parent/relative).resolve(strict=True); load_package(package)
    appearance=load_extension(package); analysis=load_analysis(package,appearance)
    if not appearance or not analysis:raise ValueError('Refinement requires validated source/analysis observations')
    if recipe['identity']['analysisRevision']!=analysis:raise ValueError('Analysis revision mismatch')
    for name,sha in recipe['identity']['manifests'].items():
        path=asset_path(package,name,name.split('/')[0] if '/' in name else '',('.json',)) if '/' in name else package/name
        if digest(path)!=sha:raise ValueError('Recipe analysis/backend fingerprint mismatch: '+name)
    exported=read_json(export_path/'export.json')
    if exported['state']!=recipe['state'] or exported['anchorSha256']!=recipe['identity']['anchorSha256'] or exported['sourceId']!=recipe['identity']['sourceId']:
        raise ValueError('Physics export is stale or belongs to a different recipe/source')
    anchor=asset_path(package,appearance['sourceImage']['path'],'textures',('.png',))
    if digest(anchor)!=recipe['identity']['anchorSha256']:raise ValueError('Wrong canonical source hash')
    with Image.open(anchor) as image:
        if max(image.size)>8192 or image.width*image.height>8*1024*1024:raise ValueError('Native refinement budget exceeded')
        original=np.asarray(image.convert('RGB')).copy()
    def native_rgb(path):
        with Image.open(path) as image:
            # Reject a forged export header before allocating/decompressing a large image.
            if image.size != (original.shape[1],original.shape[0]):raise ValueError('Export must match native source dimensions')
            return np.asarray(image.convert('RGB')).copy()
    if not np.array_equal(original,native_rgb(export_path/'original.png')):raise ValueError('Export original pixels differ from anchor')
    physics=native_rgb(export_path/'result.png')
    if physics.shape!=original.shape:raise ValueError('Physics must be native size')
    state=recipe['state']; mapping={}
    if state.get('imagePointLights'):
        raise ValueError('Neural refinement currently supports directional guidance only; keep the physics result with point lights')
    def number(key):
        path=asset_path(package,analysis['maps'][key]['path'],'analysis',('.dds',)); mapping[key]=digest(path)
        return read_dds(path)[1]
    validity=number('validity')>0; normal=number('normal')[...,:3]; depth=number('depth')
    guidance={'validity':validity,'normal':normal.copy(),'depth':depth,'confidence':number('geometryConfidence')}
    guidance['relightingConfidence']=native_confidence(export_path/'confidence.dds',original.shape[1],original.shape[0])
    mapping['relightingConfidence']=digest(export_path/'confidence.dds')
    for key,name in [('oldShading','old-shading'),('newShading','new-shading')]:
        values=read_dds(export_path/(name+'.dds'))[1][...,:3]
        if values.shape!=normal.shape:raise ValueError('Shading and geometry analysis grids must match')
        guidance[key]=values.copy(); mapping[name]=digest(export_path/(name+'.dds'))
    h,w=original.shape[:2]; protection=np.zeros((h,w),np.float32)
    labels=nearest(number('region'),(h,w))
    for region in state['protectedRegions']:protection[labels==region['label']]=region['weight']
    mask_paths=[]
    if recipe['importedMask']:
        record=recipe['importedMask']; path=(recipe_path.parent/record['path']).resolve(strict=True)
        if not path.is_relative_to(recipe_path.parent) or digest(path)!=record['sha256']:raise ValueError('Imported mask boundary/hash mismatch')
        mask_paths.append(path)
    if mask_path:mask_paths.append(Path(mask_path).resolve(strict=True))
    for path in mask_paths:
        with Image.open(path) as image:
            if max(image.size)>8192 or image.width*image.height>8*1024*1024:raise ValueError('Protection mask budget exceeded')
            mask=np.asarray(image.convert('L'),np.float32)/255
        protection=np.maximum(protection,nearest(mask,(h,w)))
    target=copy.deepcopy(state['target'])
    observation=RefinementInput(original,decode(physics),copy.deepcopy(state['sourceCalibration']['light']),target,
        guidance,protection,digest(anchor),digest(export_path/'result.png'),analysis['camera']['scaleType'],state['targetGlobalGain'])
    validate_input(observation)
    for array in (observation.original_rgb,observation.physics_rgb,observation.protection,*guidance.values()):array.setflags(write=False)
    # Additional guidance is retained/fingerprinted even though PIXL's network does not take
    # depth/normals/shadows directly. The external acceptance policy must not invent support.
    semantic_recipe={'identity':recipe['identity'],'state':state,'maskSha256':recipe['importedMask']['sha256'] if recipe['importedMask'] else None}
    recipe_hash=hashlib.sha256(json.dumps(semantic_recipe,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    refs={'recipe':recipe_hash,'export':digest(export_path/'export.json'),'source':digest(anchor),
          'physics':digest(export_path/'result.png'),'maps':mapping,'masks':[digest(p) for p in mask_paths]}
    return observation,recipe,refs,package


def luminance(a):return a@np.array([.2126,.7152,.0722],np.float32)


def gradient(a):
    y=luminance(a); dx=np.zeros_like(y); dy=np.zeros_like(y)
    dx[:,1:]=np.diff(y,axis=1); dy[1:]=np.diff(y,axis=0)
    return np.sqrt(dx*dx+dy*dy)


def accept_candidate(observation, candidate, strength):
    """Declared conservative content gate; heuristic, not an accuracy certificate."""
    physics=observation.physics_rgb; shape=physics.shape[:2]
    valid=nearest(observation.guidance['validity'],shape)
    confidence=nearest(observation.guidance['confidence'],shape)
    # Strong source edges include text strokes. Dilating them prevents a color correction
    # leaking across silhouettes; explicit user masks remain authoritative.
    edges=gradient(decode(observation.original_rgb))>.10
    edges=np.asarray(Image.fromarray(edges.astype(np.uint8)*255).filter(ImageFilter.MaxFilter(5)))>0
    difference=np.max(np.abs(candidate-physics),axis=-1)
    signal=decode(observation.original_rgb)
    lost_signal=(signal.max(axis=-1)<.015)|(signal.max(axis=-1)>.98)
    rejected=(difference>.20)|(np.abs(gradient(candidate)-gradient(physics))>.08)|(~valid)|edges|(confidence<.25)|lost_signal
    rejected=np.asarray(Image.fromarray(rejected.astype(np.uint8)*255).filter(ImageFilter.MaxFilter(3)))>0
    # The Core's native confidence already includes its signal/shadow/intrinsic support.
    # Refinement must never revive a pixel where the physics edit was deliberately disabled.
    confidence=np.minimum(confidence,observation.guidance['relightingConfidence'])
    weight=strength*(1-observation.protection)*confidence*(~rejected)
    refined=np.where(weight[...,None]==0,physics,physics+weight[...,None]*(candidate-physics)).astype(np.float32)
    protected=(observation.protection==1)|rejected
    metrics={'candidateLinearMae':float(np.mean(np.abs(candidate-physics))),
        'candidateEdgeMae':float(np.mean(np.abs(gradient(candidate)-gradient(physics)))),
        'refinedLinearMae':float(np.mean(np.abs(refined-physics))),
        'refinedEdgeMae':float(np.mean(np.abs(gradient(refined)-gradient(physics)))),
        'acceptedFraction':float(np.mean(weight>0)), 'autoEdgeProtectedFraction':float(edges.mean()),
        'lostSignalFraction':float(lost_signal.mean()),
        'protectedMaximumError':float(np.max(np.abs(refined[protected]-physics[protected]))) if np.any(protected) else 0.,
        'thresholds':{'linearMaxDelta':.20,'gradientDelta':.08,'sourceEdge':.10,'minimumGeometryConfidence':.25},
        'assessment':'Heuristic content-drift indicators; not OCR correctness or physical relighting ground truth'}
    return refined,weight,rejected,edges,metrics


def comparison_html(path, status, metrics):
    import html
    cards=''.join(f'<figure><img src="{name}.png"><figcaption>{label}</figcaption></figure>' for name,label in
        [('original','原图 / 固定来源'),('physics','物理结果 / 回退'),('raw-candidate','原始神经候选'),
         ('refined','优化结果 / 经过保护'),('difference','优化与物理结果之差 × 4'),('protected','保护区域'),
         ('accepted','接受权重'),('rejected','拒绝区域'),('guidance','目标反照率 / 明暗 / 残差引导')])
    labels={'candidateLinearMae':'候选线性平均绝对误差','candidateEdgeMae':'候选边缘平均绝对误差',
        'refinedLinearMae':'优化后线性平均绝对误差','refinedEdgeMae':'优化后边缘平均绝对误差',
        'acceptedFraction':'接受比例','autoEdgeProtectedFraction':'自动边缘保护比例','lostSignalFraction':'信号缺失比例',
        'protectedMaximumError':'保护区最大误差','thresholds':'阈值','linearMaxDelta':'线性最大变化',
        'gradientDelta':'梯度变化','sourceEdge':'原图边缘','minimumGeometryConfidence':'最低几何置信度','assessment':'评估说明'}
    def display(value):
        if isinstance(value,dict):return {labels.get(key,key):display(item) for key,item in value.items()}
        if value=='Heuristic content-drift indicators; not OCR correctness or physical relighting ground truth':
            return '启发式内容漂移指标，不代表文字识别正确性或物理重光照真值'
        return value
    status={'ready':'已就绪','candidate':'候选已生成','bypassed':'已跳过','failed':'失败','cancelled':'已取消','accepted':'已接受','rejected':'已拒绝'}.get(status,status)
    text=f'''<!doctype html><html lang="zh"><meta charset="utf-8"><title>离线神经优化对比</title>
<style>body{{background:#171c22;color:#e2e8ee;font:16px system-ui;margin:24px}}main{{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:14px}}figure{{margin:0}}img{{width:100%;background:#000}}figcaption{{padding:8px}}pre{{white-space:pre-wrap}}button{{padding:10px;margin:5px}}</style>
<h1>原图 · 物理结果 · 可选神经优化候选</h1><p>{html.escape(status)} — 原图和核心场景不会被覆盖。</p>
<button onclick="document.querySelector('main').style.gridTemplateColumns='repeat(3,minmax(0,1fr))'">并排对比</button>
<button onclick="document.querySelector('main').style.gridTemplateColumns='1fr'">细节查看 / 单列</button>
<main>{cards}</main><h2>实测内容变化</h2><pre>{html.escape(json.dumps(display(metrics),indent=2,ensure_ascii=False))}</pre>
<p>数值调试视图不经过艺术画面调整。生成的反射与阴影仅是候选，不是实测几何。</p></html>'''
    Path(path).write_text(text,encoding='utf-8')
