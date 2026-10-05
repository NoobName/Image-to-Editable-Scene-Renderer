#include "Assets/HdrImage.h"
#include "Scene/Bounds.h"
#include "Renderer/EnvironmentBaker.h"
#include "GpuImageReadback.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <limits>
using namespace isr;
using namespace DirectX;
namespace {
size_t checks = 0;
void Require(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
template<class Fn> void Reject(Fn fn, const char* message) {
    bool rejected = false; try { fn(); } catch (const std::exception&) { rejected = true; }
    Require(rejected, message);
}
bool Near(float a, float b, float tolerance = 0.001f) {
    return std::isfinite(a) && std::abs(a - b) < tolerance * std::max(1.0f, std::abs(b));
}
struct Fixture {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("ImageSceneRendererLighting-" + std::to_string(GetCurrentProcessId()) + ".hdr");
    ~Fixture() { std::error_code ec; std::filesystem::remove(path, ec); }
    void Write(const std::string& header, std::initializer_list<unsigned char> bytes) {
        std::ofstream out(path, std::ios::binary); out << header;
        for (auto b : bytes) out.put(static_cast<char>(b));
    }
};
void CpuTests() {
    Fixture file;
    HdrImage input{16, 3, std::vector<XMFLOAT4>(48, {100000, 50000, 25000, 1})};
    SaveHdr(file.path, input); auto loaded = LoadHdr(file.path);
    Require(loaded.width == 16 && loaded.height == 3, "HDR dimensions");
    for (const auto& p : loaded.pixels)
        Require(std::abs(p.x-100000)<512 && std::abs(p.y-50000)<512 && std::abs(p.z-25000)<512,
            "RGBE round trip stays within one shared-exponent quantization step and preserves HDR");
    file.Write("#?RGBE\nFORMAT=32-bit_rle_rgbe\n\n+Y 2 -X 2\n",
        {128,0,0,129, 0,128,0,129, 0,0,128,129, 128,128,128,129});
    loaded = LoadHdr(file.path);
    Require(loaded.pixels[0].x == 1 && loaded.pixels[0].y == 1 && loaded.pixels[0].z == 1 &&
        loaded.pixels[1].z == 1 && loaded.pixels[2].y == 1 && loaded.pixels[3].x == 1, "HDR axis flips");
    const std::string header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 8\n";
    file.Write(header, {2,2,0,8, 136,128, 136,64, 136,32, 136,129});
    loaded = LoadHdr(file.path);
    for (const auto& p : loaded.pixels) Require(p.x == 1 && p.y == .5f && p.z == .25f, "RLE repeated packet");
    file.Write(header, {2,2,0,8, 0}); Reject([&]{ LoadHdr(file.path); }, "Reject zero RLE packet");
    file.Write(header, {2,2,0,8, 137,128}); Reject([&]{ LoadHdr(file.path); }, "Reject RLE row overflow");
    file.Write(header, {2,2,0,8, 8,128}); Reject([&]{ LoadHdr(file.path); }, "Reject truncated RLE literal");
    file.Write("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 8192 +X 16384\n", {});
    Reject([&]{ LoadHdr(file.path); }, "Reject excessive HDR allocation");
    input.pixels[0].y = std::numeric_limits<float>::quiet_NaN();
    Reject([&]{ SaveHdr(file.path, input); }, "Reject nonfinite RGB channel");
    Reject([]{ HdrMipChain({}); }, "Reject invalid mip source");
    HdrImage odd{3, 5, std::vector<XMFLOAT4>(15, {2, 3, 4, 1})};
    auto mips = HdrMipChain(odd);
    Require(mips.size() == 3 && mips.back().width == 1 && mips.back().height == 1, "Odd mip dimensions");
    for (const auto& mip : mips) for (const auto& p : mip.pixels)
        Require(Near(p.x, 2) && Near(p.y, 3) && Near(p.z, 4), "Mips preserve constant radiance");
    odd.pixels[14].x = 17;
    Require(Near(HdrMipChain(odd).back().pixels[0].x, 3), "Odd mip includes last row and column");
    Scene scene = Scene::CreateDemo();
    XMFLOAT4X4 mirrored;
    XMStoreFloat4x4(&mirrored, XMMatrixScaling(-2, .5f, 3) * XMMatrixRotationRollPitchYaw(.3f, .7f, 0));
    scene.entities[1].transform.importedLocal = mirrored;
    scene.UpdateWorldMatrices(); std::vector<Bounds> local;
    for (const auto& mesh : scene.meshes) local.push_back(MeshBounds(mesh));
    auto bounds = WorldBounds(scene, local);
    for (const auto& e : scene.entities) if (e.renderer) for (const auto& vertex : scene.meshes[e.renderer->meshIndex].vertices) {
        XMFLOAT3 p; XMStoreFloat3(&p, XMVector3TransformCoord(XMLoadFloat3(&vertex.position), e.transform.WorldMatrix()));
        Require(p.x >= bounds.minimum.x - .001f && p.x <= bounds.maximum.x + .001f &&
            p.y >= bounds.minimum.y - .001f && p.y <= bounds.maximum.y + .001f &&
            p.z >= bounds.minimum.z - .001f && p.z <= bounds.maximum.z + .001f, "World bounds contain transformed geometry");
    }
    for (XMFLOAT3 direction : {XMFLOAT3{0,-1,0}, XMFLOAT3{0,1,0}, XMFLOAT3{.3f,-1,.7f}}) {
        auto vp = FitDirectionalShadow(bounds, direction, 2048);
        for (unsigned i = 0; i < 8; ++i) {
            auto p = XMVectorSet(i&1?bounds.maximum.x:bounds.minimum.x, i&2?bounds.maximum.y:bounds.minimum.y,
                i&4?bounds.maximum.z:bounds.minimum.z, 1);
            XMFLOAT3 clip; XMStoreFloat3(&clip, XMVector3TransformCoord(p, vp));
            Require(std::abs(clip.x) < 1 && std::abs(clip.y) < 1 && clip.z > 0 && clip.z < 1, "Shadow frustum encloses every bounds corner");
        }
    }
    Reject([&]{ FitDirectionalShadow(bounds, {0,0,0}, 2048); }, "Reject zero light direction");
}
void GpuTests() {
    DeviceContext context(true); // WARP makes the numeric checks independent of the installed GPU.
    EnvironmentBaker baker(context);
    const XMFLOAT4 color{.25f, .5f, 100000, 1};
    auto maps = baker.Bake({16,8,std::vector<XMFLOAT4>(128, color)});
    for (auto* resource : {maps.sky.Get(), maps.prefilter.Get(), maps.irradiance.Get()}) {
        float scale = resource == maps.irradiance.Get() ? XM_PI : 1;
        for (const auto& sub : ReadGpuImage(context, resource)) for (const auto& p : sub)
            Require(Near(p.x, color.x * scale) && Near(p.y, color.y * scale) && Near(p.z, color.z * scale),
                "Constant environment: sky=L, irradiance=pi*L, all prefilter mips=L");
    }
    auto lut = ReadGpuImage(context, baker.BrdfLut())[0];
    for (const auto& p : lut)
        Require(std::isfinite(p.x + p.y) && p.x >= 0 && p.y >= 0 && p.x + p.y < 1.08f, "BRDF LUT finite nonnegative energy");
    Require(lut[127].x > .99f && lut[127].y < .001f, "Smooth normal-incidence BRDF limit");
    // Encode the world direction as color. This exposes flipped/swapped cube faces.
    HdrImage gradient{256,128,{}}; gradient.pixels.resize(256*128);
    for (unsigned y = 0; y < 128; ++y) for (unsigned x = 0; x < 256; ++x) {
        float theta = (y+.5f)/128*XM_PI, phi = ((x+.5f)/256-.5f)*XM_2PI;
        gradient.pixels[y*256+x] = {.5f+.5f*std::sin(theta)*std::cos(phi), .5f+.5f*std::cos(theta),
            .5f+.5f*std::sin(theta)*std::sin(phi), 1};
    }
    auto directional = baker.Bake(gradient);
    auto sky = ReadGpuImage(context, directional.sky.Get());
    const XMFLOAT3 expected[] = {{1,.5f,.5f},{0,.5f,.5f},{.5f,1,.5f},{.5f,0,.5f},{.5f,.5f,1},{.5f,.5f,0}};
    for (unsigned face = 0; face < 6; ++face) {
        auto p = sky[face*EnvironmentBaker::SkyMips][128*256+128];
        Require(Near(p.x, expected[face].x, .006f) && Near(p.y, expected[face].y, .006f) && Near(p.z, expected[face].z, .006f), "Cubemap axes match world directions");
    }
    auto prefilter = ReadGpuImage(context, directional.prefilter.Get());
    float sharp = prefilter[0][64*128+64].x;
    float rough = prefilter[7][0].x;
    Require(sharp > .99f && rough < sharp - .06f && rough > .55f, "Roughness broadens the environment reflection");
    context.CheckMessages();
    std::cout << "GPU reference: irradiance B=" << ReadGpuImage(context, maps.irradiance.Get())[0][0].z
        << ", directional prefilter sharp=" << sharp << ", rough=" << rough << '\n';
}
}
int main() {
    try { CpuTests(); GpuTests(); std::cout << "PASS: " << checks << " HDR/shadow/IBL checks\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
