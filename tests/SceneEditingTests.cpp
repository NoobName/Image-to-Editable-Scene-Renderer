#include "Scene/SceneEditing.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace isr;
using namespace DirectX;
namespace {
void Require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
bool Near(float a,float b,float epsilon=1e-4f){return std::abs(a-b)<epsilon;}
XMFLOAT3 Center(const Bounds& b){return {(b.minimum.x+b.maximum.x)*.5f,(b.minimum.y+b.maximum.y)*.5f,(b.minimum.z+b.maximum.z)*.5f};}
bool Same(XMFLOAT3 a,XMFLOAT3 b){return Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.z,b.z);}
Scene Fixture(){
    Scene scene;scene.camera.LookAt({0,0,0},{0,0,1});scene.camera.SetPerspective(XM_PIDIV2,1,.1f,20);
    scene.meshes={Mesh::Cube()};scene.materials.resize(2);
    Entity object;object.name="Object A";object.objectId="a";object.transform.position={0,0,4};
    Entity primitive;primitive.name="Primitive";primitive.parent=0;primitive.renderer=MeshRenderer{0,0};
    Entity back;back.name="Object B";back.objectId="b";back.transform.position={0,0,7};back.renderer=MeshRenderer{0,1};
    scene.entities={object,primitive,back};scene.UpdateWorldMatrices();return scene;
}
}
int main(){try{
    auto scene=Fixture();SceneEditState edit;edit.Capture(scene);
    Require(edit.Pick(scene,.5f,.5f)==0,"Picking must choose closest logical object root");
    Require(!edit.Pick(scene,.01f,.01f),"Background incorrectly selected an object");
    scene.entities[1].renderer->visible=false;
    Require(edit.Pick(scene,.5f,.5f)==2,"Hidden primitives must not intercept selection");
    scene.entities[1].renderer->visible=true;
    scene.entities[0].transform.scale={-2,.7f,1.3f};scene.UpdateWorldMatrices();
    Require(edit.Pick(scene,.5f,.5f)==0,"Mirrored nonuniform object was not selectable");
    scene.camera.SetPerspective(XM_PIDIV2,1,.1f,2);
    Require(!edit.Pick(scene,.5f,.5f),"Far-clipped geometry was picked");
    scene.camera.SetPerspective(XM_PIDIV2,1,10,20);
    Require(!edit.Pick(scene,.5f,.5f),"Near-clipped geometry was picked");
    scene=Fixture();edit.Capture(scene);
    scene.materials[0].baseColor={.2f,.3f,.4f,1};scene.materials[0].roughness=.12f;scene.materials[0].metallic=.82f;scene.materials[0].normalScale=.25f;
    scene.materials[0].overrideMetallicRoughness=true;
    Require(Near(edit.OriginalMaterial(0).roughness,.5f)&&Near(scene.materials[1].roughness,.5f),"Editing overwrote original or another object material");
    edit.RestoreMaterial(scene,0);
    Require(!scene.materials[0].overrideMetallicRoughness,"Material reset lost source-map mode");
    Require(Near(scene.materials[0].baseColor.x,1)&&Near(scene.materials[0].roughness,.5f)&&Near(scene.materials[0].metallic,0)&&Near(scene.materials[0].normalScale,1),"Material reset incomplete");
    // A reconstructed surface stores camera-space points in vertices, not a centered local mesh.
    for(auto& vertex:scene.meshes[0].vertices){vertex.position.x+=2;vertex.position.z+=3;}
    scene.entities[0].transform.position={0,0,0};scene.UpdateWorldMatrices();edit.Capture(scene);
    auto before=Center(*edit.SelectionBounds(scene,0));auto next=scene.entities[0].transform;
    next.rotation={.2f,.7f,.3f};next.scale={1.4f,.8f,1.2f};edit.EditTransform(scene,0,next,true);
    Require(Same(before,Center(*edit.SelectionBounds(scene,0))),"Rotate/scale moved reconstructed object's center");
    const auto other=scene.entities[2].transform.position;
    next=scene.entities[0].transform;next.position.x+=.7f;edit.EditTransform(scene,0,next,false);
    Require(Near(Center(*edit.SelectionBounds(scene,0)).x,before.x+.7f)&&Same(other,scene.entities[2].transform.position),"Move changed unrelated object or wrong space");
    edit.RestoreTransform(scene,0);Require(Same(Center(*edit.SelectionBounds(scene,0)),before),"Transform restore failed");
    // Preserve center even with the imported-matrix offset convention and nonuniform parent.
    scene.entities[0].transform.scale={2,1,3};XMFLOAT4X4 shear;XMStoreFloat4x4(&shear,XMMatrixIdentity());shear._21=.3f;
    scene.entities[1].transform.importedLocal=shear;scene.UpdateWorldMatrices();edit.Capture(scene);
    before=Center(*edit.SelectionBounds(scene,1));next=scene.entities[1].transform;next.rotation.y=.4f;next.scale={1.2f,.9f,.7f};
    edit.EditTransform(scene,1,next,true);Require(Same(before,Center(*edit.SelectionBounds(scene,1))),"Pivot compensation failed for shear/parent transform");
    const auto d=DragSunDirection({.4f,-.8f,.5f},scene.camera,25,-17);
    Require(Near(std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z),1),"Sun direction was not normalized");
    const auto fixed=DragSunDirection(d,scene.camera,0,0);Require(Same(d,fixed),"Zero drag changed sun direction");
    for(auto direction:{XMFLOAT3{0,1,0},XMFLOAT3{0,-1,0},XMFLOAT3{0,0,1}}){
        auto rotated=DragSunDirection(direction,scene.camera,100,-50);Require(std::isfinite(rotated.x+rotated.y+rotated.z),"Sun pole drag produced nonfinite direction");}
    bool rejected=false;try{DragSunDirection({0,0,0},scene.camera,1,1);}catch(const std::invalid_argument&){rejected=true;}
    Require(rejected,"Zero light direction should be rejected");
    scene.materials[0].roughness=.9f;edit.Reset();edit.Capture(scene);Require(Near(edit.OriginalMaterial(0).roughness,.9f),"New scene retained stale original material");
    std::cout<<"Picking, visibility, clipping, mirror, pivot, imported shear, isolated edits, reset and sun direction: OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
