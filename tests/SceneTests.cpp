#include "Scene/Scene.h"
#include "App/CameraController.h"
#include <windows.h>
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace isr;
using namespace DirectX;
namespace {
int checks = 0;
void Require(bool condition, const char* message) { ++checks; if (!condition) throw std::runtime_error(message); }
bool Near(float a, float b, float epsilon = 1e-4f) { return std::abs(a-b) < epsilon; }
template<class Function> void MustThrow(Function f) {
    bool caught = false; try { f(); } catch (const std::exception&) { caught = true; }
    Require(caught,"Invalid input was accepted");
}
void CheckMesh(const Mesh& mesh) {
    Require(!mesh.vertices.empty() && mesh.indices.size()%3 == 0,"Invalid mesh size");
    for (const auto& v : mesh.vertices) {
        const auto n = XMLoadFloat3(&v.normal), t = XMLoadFloat4(&v.tangent);
        Require(Near(XMVectorGetX(XMVector3Length(n)),1),"Normal is not unit length");
        Require(Near(XMVectorGetX(XMVector3Length(t)),1),"Tangent is not unit length");
        Require(Near(XMVectorGetX(XMVector3Dot(n,t)),0),"Tangent is not perpendicular to normal");
        Require(v.texcoord.x >= 0 && v.texcoord.x <= 1 && v.texcoord.y >= 0 && v.texcoord.y <= 1,"UV out of range");
    }
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        for (size_t j = 0; j < 3; ++j) Require(mesh.indices[i+j] < mesh.vertices.size(),"Index out of bounds");
        const auto& a = mesh.vertices[mesh.indices[i]], &b = mesh.vertices[mesh.indices[i+1]], &c = mesh.vertices[mesh.indices[i+2]];
        const auto cross = XMVector3Cross(XMLoadFloat3(&b.position)-XMLoadFloat3(&a.position),XMLoadFloat3(&c.position)-XMLoadFloat3(&a.position));
        Require(XMVectorGetX(XMVector3LengthSq(cross)) > 1e-12f,"Degenerate triangle");
        Require(XMVectorGetX(XMVector3Dot(cross,XMLoadFloat3(&a.normal)+XMLoadFloat3(&b.normal)+XMLoadFloat3(&c.normal))) > 0,"Inward winding");
        const float du1 = b.texcoord.x-a.texcoord.x, dv1 = b.texcoord.y-a.texcoord.y;
        const float du2 = c.texcoord.x-a.texcoord.x, dv2 = c.texcoord.y-a.texcoord.y;
        const float determinant = du1*dv2-du2*dv1;
        Require(std::abs(determinant) > 1e-8f,"Degenerate UV triangle");
        const auto bitangent = ((XMLoadFloat3(&c.position)-XMLoadFloat3(&a.position))*du1
            -(XMLoadFloat3(&b.position)-XMLoadFloat3(&a.position))*du2)/determinant;
        const auto reconstructed = XMVector3Cross(XMLoadFloat3(&a.normal),XMLoadFloat4(&a.tangent))*a.tangent.w;
        Require(XMVectorGetX(XMVector3Dot(bitangent,reconstructed)) > 0,"Tangent handedness disagrees with UV +V");
    }
}
}
int main() {
    try {
        CheckMesh(Mesh::Cube()); CheckMesh(Mesh::Sphere()); CheckMesh(Mesh::Sphere(3,2)); CheckMesh(Mesh::Plane());
        MustThrow([] { (void)Mesh::Sphere(2,2); });
        Transform transform; transform.scale = {2,3,4}; transform.position = {10,20,30}; transform.UpdateWorld();
        XMFLOAT3 point; XMStoreFloat3(&point,XMVector3TransformCoord(XMVectorSet(1,1,1,1),transform.WorldMatrix()));
        Require(Near(point.x,12) && Near(point.y,23) && Near(point.z,34),"Transform S*R*T order");
        transform.scale.y = 0; MustThrow([&] { (void)transform.LocalMatrix(); });
        // Editing an imported node must preserve the image before any edit,
        // including mirrored transforms and Euler singularities.
        for(float pitch:{-XM_PIDIV2,-0.7f,0.0f,0.4f,XM_PIDIV2})for(float yaw:{-2.0f,0.0f,1.5f})for(float roll:{-1.0f,0.0f,2.0f})for(float sign:{-1.0f,1.0f}){
            const auto original=XMMatrixScaling(2*sign,3,4)*XMMatrixRotationRollPitchYaw(pitch,yaw,roll)*XMMatrixTranslation(3,5,-7);
            Transform imported;imported.importedLocal.emplace();XMStoreFloat4x4(&*imported.importedLocal,original);
            Require(imported.TryDecomposeImported(),"SRT matrix decomposition failed");
            XMFLOAT4X4 a,b;XMStoreFloat4x4(&a,original);XMStoreFloat4x4(&b,imported.LocalMatrix());
            for(int row=0;row<4;++row)for(int col=0;col<4;++col)Require(Near(a.m[row][col],b.m[row][col],0.001f),"Imported matrix changed during editing conversion");
            imported.position.x+=2;XMStoreFloat4x4(&b,imported.LocalMatrix());Require(Near(b._41,5),"Imported position cannot be edited");
        }
        Transform shear;shear.importedLocal=XMFLOAT4X4(1,0.4f,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
        Require(!shear.TryDecomposeImported()&&shear.importedLocal.has_value(),"Shear was lost in TRS conversion");
        shear.position.x=2;XMFLOAT4X4 sheared;XMStoreFloat4x4(&sheared,shear.LocalMatrix());
        Require(Near(sheared._12,0.4f)&&Near(sheared._41,2)&&Near(sheared._42,0.8f),"Matrix-base offset composition failed");
        Scene hierarchy; Entity parent; parent.transform.position = {2,0,0};
        Entity child; child.parent = 0; child.transform.position = {0,3,0}; hierarchy.entities = {parent,child}; hierarchy.UpdateWorldMatrices();
        XMStoreFloat3(&point,XMVector3TransformCoord(XMVectorZero(),hierarchy.entities[1].transform.WorldMatrix()));
        Require(Near(point.x,2) && Near(point.y,3),"Parent world composition");
        hierarchy.entities[0].parent = 1; MustThrow([&] { hierarchy.UpdateWorldMatrices(); });
        {
            Scene objects;objects.meshes={Mesh::Cube()};objects.materials={{"one"},{"two"}};
            Entity a;a.objectId="building";Entity aNode;aNode.parent=0;Entity aMesh;aMesh.parent=1;aMesh.renderer=MeshRenderer{0,0};
            Entity b;b.objectId="ground";Entity bMesh;bMesh.parent=3;bMesh.renderer=MeshRenderer{0,1};Entity sky;sky.objectId="sky";
            objects.entities={a,aNode,aMesh,b,bMesh,sky};objects.UpdateWorldMatrices();
            Require(objects.RenderableSubtree(0)==std::vector<size_t>{2},"Object selection leaked across roots");
            Require(objects.RenderableSubtree(3)==std::vector<size_t>{4},"Second object subtree lost");
            Require(objects.RenderableSubtree(5).empty(),"Mask-only sky gained invented geometry");
            for(auto index:objects.RenderableSubtree(0)){auto& r=*objects.entities[index].renderer;r.visible=false;objects.materials[r.materialIndex].roughness=.15f;}
            Require(!objects.entities[2].renderer->visible&&objects.entities[4].renderer->visible,"Visibility edit leaked to another object");
            Require(Near(objects.materials[1].roughness,.5f)&&Near(objects.materials[0].roughness,.15f),"Independent material edit leaked");
        }
        Camera camera; camera.LookAt({0,0,-5},{0,0,0}); camera.SetPerspective(XM_PIDIV4,1,0.1f,100);
        auto position = camera.Position();
        Require(XMVectorGetX(XMVector3Length(XMVector3TransformCoord(XMLoadFloat3(&position),camera.View()))) < 1e-4f,"View matrix must map eye to origin");
        const float nearDepth = XMVectorGetZ(XMVector3TransformCoord(XMVectorSet(0,0,0.1f,1),camera.Projection()));
        const float farDepth = XMVectorGetZ(XMVector3TransformCoord(XMVectorSet(0,0,100,1),camera.Projection()));
        Require(Near(nearDepth,0) && Near(farDepth,1),"D3D depth range is [0,1]");
        MustThrow([&] { camera.SetPerspective(XM_PI,1,0.1f,100); });
        MustThrow([&] { camera.SetPerspective(1,0,0.1f,100); });
        MustThrow([&] { camera.SetPerspective(1,1,1,0.1f); });
        camera.Orbit({0,0,0},0.5f,0.2f); position = camera.Position();
        Require(Near(XMVectorGetX(XMVector3Length(XMLoadFloat3(&position))),5),"Orbit radius changed");
        camera.Rotate(1,100); Require(std::isfinite(XMVectorGetX(camera.View().r[0])),"Pitch clamp failed");
        camera.LookAt({0,0,-5},{0,0,0}); CameraController controller; InputState input;
        input.keys['W'] = true; input.keys['D'] = true; controller.Update(camera,input,0.1f);
        position = camera.Position(); const float distance = std::sqrt(position.x*position.x+(position.z+5)*(position.z+5));
        Require(Near(distance,0.3f),"Diagonal movement must be normalized and time based");
        input = {}; input.rightMouse = true; input.mouseX = 40; input.mouseY = -10;
        const auto beforeView = camera.View(); controller.Update(camera,input,0.1f);
        Require(!Near(XMVectorGetX(beforeView.r[0]),XMVectorGetX(camera.View().r[0])),"RMB rotation did not affect view");
        input = {}; input.wheel = 1; position = camera.Position(); controller.Update(camera,input,0.1f);
        Require(!Near(position.z,camera.Position().z),"Wheel did not dolly camera");
        input.rightMouse = true; const float oldSpeed = controller.Speed(); controller.Update(camera,input,0.1f);
        Require(controller.Speed() > oldSpeed,"RMB wheel did not change movement speed");
        input = {}; input.active = false; input.keys['W'] = true; position = camera.Position(); controller.Update(camera,input,1);
        Require(Near(position.z,camera.Position().z),"Inactive window moved camera");
        auto demo = Scene::CreateDemo(); Require(demo.entities.size() == 3,"Demo scene composition");
        std::cout << "PASS: " << checks << " geometry, transform, camera and input checks\n"; return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
