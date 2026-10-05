# Image-to-Editable-Scene Renderer

Windows / C++20 / DirectX 12 项目。已实现 Prompt 00–19：清屏、资源系统、相机与基础 Scene、glTF 2.0 导入、PBR、方向光阴影、HDR 环境 IBL、三栏 Look Development 工具、统一参数驱动的后处理管线、ScenePackage 磁盘交换格式，以及独立 Python Reconstruction Pipeline（MoGe-2 几何、逐像素 2.5D 网格、SAM 2 分割、Marigold intrinsic 材质估计、独立物体编辑与原尺寸 Appearance Anchor）。可从 File 菜单重建图像并自动加载结果，在 Viewport 选取物体、拖动太阳方向并实时编辑场景；可切换到独立二维原图、数值分析图和原始照明拟合诊断。

Renderer 与 AI Pipeline 解耦。C++ 不依赖 Python；Python 代码仅位于 `tools/reconstruction/`，通过 JSON 和磁盘文件交换数据。模型只在 Python Adapter 内运行，网格构建与离线重建不依赖 PyTorch。

## 阶段 19：原始照明拟合基线

Image Mode 中点击 **Fit saved observations**，复用已保存的 geometry/material/segmentation，后台运行 `robust-directional-ambient` 并导入新包。也可离线执行：

```powershell
conda run -n image-scene-renderer python tools/reconstruction/estimate_lighting.py generated/scene18 --output generated/my-lighting
.\build\Debug\ImageSceneRenderer.exe --package generated/my-lighting --work-mode image --image-view old-shading
```

新增 Shading Proxy、Old Shading、Residual、Fit Mask 和 source/target 参数检查。Source 校准必须显式 Apply；旧拟合缓存随后失效，Export calibrated source 可离线重建证据。Target 只修改独立参数，暂不生成新 RGB。使用固定曝光和 albedo 尺度，强度为相对单位；不能将默认导出灯或 neutral albedo 称为准确估计。真实多光源/高反射照片可能明确返回退化结果。进度协议 v2 增加 Lighting 行，兼容旧四阶段协议。详见 [照明契约与操作](tools/reconstruction/LightingBaseline.md)。

## 阶段 18：AnalysisMaps 到图像域 GPU

Image Relighting 的 `Image View` 下拉菜单增加 Depth、Geometry Normal、Position、Validity、Region、Estimated Albedo、Roughness、Metallic 和三种来源 confidence。Inspector 显示单位、坐标、来源和真实可用性；不计算新照明，Source 与3D Final保留。几何 normal 与平坦 tangent normal 分开显示。

```powershell
conda activate image-scene-renderer
# 从已有真实 scene13 数值升级到新目录，不重新推理；本机已有 generated/scene18 可直接打开
python tools/reconstruction/export_analysis.py generated/scene13 --output generated/scene18-reader
.\build\Debug\ImageSceneRenderer.exe --package generated/scene18-reader --work-mode image --image-view geometry-normal
```

独立严格 `analysis/analysis.json` 与受限 DX10 DDS 保留 float32 / uint32 数值；EXR/NPZ继续保留，PBR纹理槽不增加分析图。`SourceObservation`、后台GPU准备和fence退休复用阶段17。缺少分析图显示 unavailable，损坏分析包拒绝发布。新重建和重新导出自动写入该契约。详见 [AnalysisMaps.md](tools/reconstruction/AnalysisMaps.md)，验证入口为 `tools/Validate-Analysis.ps1`、`tools/reconstruction/verify_analysis.py` 和 `AnalysisGpuTests`。

## 阶段 17：3D Scene / Image Relighting 工作模式（历史基础）

Viewport 顶部 `Mode` 切换工作模式。`3D Scene` 保留原有编辑与 Final；阶段17的 `Image Relighting` 提供规范化原图 `Original View`、`Pixel Grid` 和来源信息，不计算新照明。阶段18将原图/网格入口合入 Image View 下拉菜单并增加分析图。原图自动等比例 Fit，不受 mesh、可见性、材质、自由相机、曝光或 Look 影响。Pixel Grid 每64个源像素划线，可查看鼠标对应源像素。

```powershell
# 先按下方阶段16流程生成带 anchor 的包，或使用本机已验证的 generated/prompt16/analysis512
.\build\Debug\ImageSceneRenderer.exe --package "generated/my-anchor16" --work-mode image
.\build\Debug\ImageSceneRenderer.exe --package "generated/my-anchor16" --work-mode image --image-view grid
.\build\Debug\ImageSceneRenderer.exe --package "assets/ScenePackage" --work-mode scene
```

旧 `--render-mode original` 仍是3D中的 **Original Image on Geometry**，RenderMode数值不变。没有已验证anchor的旧包继续显示3D；升级的legacy processed anchor可查看，但明确标为低分辨率。原图采用一次sRGB decode/encode，绕过默认ACES/Bloom/Look。背景加载成功时Scene与只读SourceObservation一起发布；失败或取消保留上一份文档，并沿用fence退休旧GPU资源。

验证入口为 `tools/Validate-ImageMode.ps1` 和 `python tools/reconstruction/verify_image_mode.py`，使用阶段16 fixtures及修改前基线；详情、CLI与复现条件见 [ImageMode.md](tools/reconstruction/ImageMode.md)。本机实际19组窗口与像素验证通过；原尺寸1500×1000在Debug/Release/WARP下最大误差0 LSB（要求≤1），ImGui原尺寸512×341也为0。当前只支持Auto Fit、单mip线性过滤，无pan/zoom、新照明或HDR10输出。

## 阶段 16：保留原尺寸 Appearance Anchor

新包在独立的 `relighting/relighting.json` 中记录原文件、定向/色彩规范化后的原尺寸图、分析图、SHA-256、像素坐标映射及来源相机。`scene.json` 仍是 v1；`textures/original_image.png` 仍是分析尺寸的处理后图像，高分辨率图单独保存在 `textures/source_anchor.png`。模型和网格继续使用 `--max-size`，Renderer 的 Final 和 Original Image 模式保持原有含义。

```powershell
conda activate image-scene-renderer
python tools/reconstruction/reconstruct.py "input.jpg" --output generated/my-anchor16 --max-size 512 --geometry-backend dummy --segmentation-backend dummy --material-backend neutral
python tools/reconstruction/inspect_anchor.py generated/my-anchor16
.\build\Debug\ImageSceneRenderer.exe --package generated/my-anchor16
# 可选：创建含中文路径、细字、ICC、EXIF、旧包及损坏副本的学习输入；目录必须为新目录
python tools/reconstruction/anchor_examples.py generated/my-anchor-examples
```

查看输出包的 `debug/source-analysis.png` 和 `.json`，比较源图/分析图及有效映射。限制为单帧 JPEG/PNG、128 MiB、40 MP、单边 16384 像素，并在规范化前检查保守工作内存预算；超限明确失败，不偷偷缩小 anchor。RGB8 不保留高位深数值，原始文件字节另存以保留来源。

remesh / segment / estimate_material 保留已有 anchor 和 sidecar 字节；旧包升级必须输出新目录，只能标为 legacy processed anchor，来源相机不足时要求校准。扩展缺失可继续加载 3D，存在但损坏会明确拒绝导入，保留当前场景。此阶段只增加 CPU 元数据，没有重打光、独立原图视图或新 GPU 纹理。字段、公式与公开验证命令见 [AppearanceAnchor.md](tools/reconstruction/AppearanceAnchor.md)。详细中文教材、实验和 Q&A 仍只在本地 `docs/`。

## 阶段 15：编辑重建场景

```powershell
# 本机已有的具名分割 + 真实材质示例，无须重新运行模型
.\build\Debug\ImageSceneRenderer.exe --package "generated/scene13" --render-mode final
```

1. 左键点击 Viewport 中的物体，或在左侧 Objects 中选择 Chair 等节点。黄色包围框表示当前选择；点到子网格会自动选中对应的重建对象。
2. 右侧 Object → Transform 可拖动 Position / Rotation / Scale，双击数值可直接输入。旋转单位为度；默认围绕当前物体中心旋转和缩放，避免相机空间网格绕相机原点公转。Reset transform 恢复载入时变换。
3. Material 中实时修改 Base Color、Roughness、Metallic、Normal Strength。展开 **Original Material** 查看载入时的参数，**Restore original material** 恢复它们；这里的 Original 指载入的估计材质，不是原始照片。
4. 勾选 **Override roughness / metallic maps** 后，粗糙度和金属度滑块直接指定常量。未勾选时按 glTF 规则乘贴图值；黑色金属度贴图乘任何因子仍是 0。Base Color 始终作为贴图颜色乘数。
5. 在 Viewport 右上角 **SUN / drag LMB** 内按住左键拖动，实时改变方向光。控件会选中该灯，右侧可继续修改 Direction、Color、Intensity；拖动控件不会带动相机。多方向光时控制当前选中的方向光，否则控制第一盏。
6. 选择左侧 Environment 调整 HDRI / Intensity / Rotation；右侧 Look 调整 Exposure。光照效果请在 **Final** 中观察。

Estimated 视图保留原始估计贴图，不显示编辑因子或常量覆盖；Albedo / Roughness / Metallic / Normal 显示当前材质属性，但绕过光照和曝光。当前重建 Normal 是平坦 fallback，调 Normal Strength 不会凭空产生细节；可用 MaterialLab 的真实法线贴图验证。

每个重建对象拥有独立材质，贴图仍共享。普通 glTF 的共享材质保持共享，Inspector 会提示影响的 primitive 数量。Viewport 拾取使用几何相交，尚未按 MASK/BLEND 纹理透明度过滤；必要时在左侧层级选择。

编辑保存在本次运行的内存中，尚无保存编辑/Undo 功能。移动物体暴露的背面与遮挡空洞不会补全。学习文档及 Q&A 只保存在本地 docs/。

回归入口：`tools/Validate-Editing.ps1`；随后运行 `python tools/reconstruction/check_editing_captures.py generated/scene13` 比较 GPU 输出。场景数学和真实 ImGui 鼠标事件测试为 `SceneEditingTests`、`ViewportToolsTests`。

## 阶段 14：在编辑器内重建图像

```powershell
.\build\Debug\ImageSceneRenderer.exe
```

选择 **File → Reconstruct Image...**，选中 JPG / PNG 后自动开始。默认使用本机已配置的 MoGe + SAM 2 + Marigold，512 像素、离线缓存模式。处理时可以继续查看当前场景；进度窗口显示 Geometry、Segmentation、Materials、Scene Export。完成后经过 Loading，自动载入新场景并进入 Ready。

输出位于 `generated/<图片名>-<唯一后缀>/`，日志位于 `generated/.reconstruction-jobs/<job-id>/python.log`，具体路径显示在进度窗口中。Ready 后可以关闭进度窗口，继续编辑对象、材质和灯光。

**File → Reconstruction settings...** 可以选择 Python 解释器、各后端和图像尺寸。本机会自动发现 `D:\miniconda\envs\image-scene-renderer\python.exe`，无需先激活 Conda；解释器路径保存在被 Git 忽略的 generated/ 中。选择 `dummy / dummy / neutral` 可以快速测试连接。安装模型仍使用已有 setup 脚本。

重建、CPU 场景解析、GPU 资源准备均在后台进行。失败保留当前场景，提供 Retry / Settings 和错误日志；Cancel 会停止 Python 进程树。没有 Python runtime 链接、RPC 或 Agent。完整操作、进度协议和验证命令见 [DesktopIntegration.md](tools/reconstruction/DesktopIntegration.md)。

## 阶段 13：Intrinsic 材质重建

```powershell
# 本机已经生成的真实材质示例：
.\build\Debug\ImageSceneRenderer.exe --package "generated/scene13" --render-mode estimated-albedo

conda activate image-scene-renderer
# 首次配置材质后端：固定推理依赖 + 约 2.58 GB 官方 fp16 权重；本机已安装并缓存
.\tools\reconstruction\setup-material.ps1
# 复用阶段 12 的几何和物体划分，只估计材质，输出使用新目录
python tools/reconstruction/estimate_material.py generated/scene12 --output generated/my-material13 --device cuda --offline
# 新照片完整重建：
python tools/reconstruction/reconstruct.py "input.jpg" --output generated/my-scene13 --geometry-backend moge --segmentation-backend sam2 --material-backend marigold --device cuda --max-size 512 --offline
```

`MaterialEstimationBackend` 统一提供线性 Albedo、Roughness、Metallic、切线空间 Normal 和 Confidence。Marigold IID Appearance 实际估计前三项，Normal 使用明确标记的平坦 fallback；现有 MoGe 几何法线继续生效。Confidence 来自 3 次推理的差异，不是校准后的正确率。没有训练模型。

Viewport 增加 **Original Image / Estimated Albedo / Estimated Normal / Estimated Roughness**。原图独立保存，Estimated 视图读取原始贴图，绕过材质因子、照明和后处理；Final 使用估计材质及当前灯光。Estimated Normal 的平坦紫色表示当前没有细节法线估计，Inspector 会说明来源；原有 Normal 视图显示世界空间表面法线。

输出包含 `textures/object_albedo.png`、`object_normal.png`、`object_roughness.png`、`object_metallic.png`、`object_confidence.png` 和 `original_image.png`。每个对象拥有独立材质，共享完整图像 atlas。Albedo / 原图使用 sRGB，其他数据贴图使用 Linear。`debug/material.npz` 保留 float32 数值，JSON 记录模型版本和 fallback。

新重建默认 `--material-backend neutral`：使用中性材质并保留独立原图，避免把带光照的照片当成 Albedo。真实估计需显式选择 `marigold`；旧包仍可加载。材质更新保留包内已保存的 Camera、Light、Environment/HDRI、Look、Transform 和 Visible，不读取尚未保存的 UI 编辑。

接入、参数、文件规范和模型选择见 [Material.md](tools/reconstruction/Material.md)。`generated/scene13-inference` 已验证三个真实模型串联。Final 尚未恢复原图照明，估计贴图也可能残留光照、丢失细节；这一步不等于 Reference Matching。

## 阶段 12：分割与独立物体

```powershell
conda activate image-scene-renderer
# 首次安装 SAM 2：仅修改当前项目环境，下载固定版本官方源码和 Tiny 权重
.\tools\reconstruction\setup-sam2.ps1
# 新图片：MoGe 几何 + SAM 2 自动 mask，名称使用明确标记的几何启发式
python tools/reconstruction/reconstruct.py "input.jpg" --output generated/my-scene12 --geometry-backend moge --segmentation-backend sam2 --device cuda --max-size 512 --offline
# 复用已有室内预测，使用附带的点/框提示给物体命名；不重跑 MoGe
python tools/reconstruction/segment.py generated/scene11 --output generated/my-named-scene12 --segmentation-prompts tools/reconstruction/examples/indoor-prompts.json --device cuda --offline
.\build\Debug\ImageSceneRenderer.exe --package generated/my-named-scene12 --render-mode albedo
```

本机已经生成的示例为 `generated/scene12`（室内 9 区域）、`generated/scene12-traffic`（街景 8 区域，含 Building / Ground / Sky）和 `generated/scene12-auto-final`（自动分割）。它们及模型缓存不上传 GitHub。示例提示坐标只适用于对应 MoGe 官方照片，自己的照片需要重新提供点/框，或者省略提示文件。

左侧 **Objects** 选区域父节点即可修改 Transform、Visible 和独立 Material；Region 面板显示稳定字符串 ID、类别、mask 编号、像素包围盒与平均有效深度。Roughness 在 Final / Roughness 模式观察，Albedo 用于观察原图和 Base Color 修改。Sky 等没有可用三角形的区域仍显示为逻辑节点，不伪造几何。

每个区域写入 `objects/<id>/region.json`、`mask.png` 和可选 `mesh.glb`。各物体保持统一 point map 中的坐标，独立材质共享照片纹理。跨区域三角形会被删除，边缘可能有细缝；隐藏物体后露出的区域不会补全。SAM 2 不识别类别文字：有意义的名称来自点/框提示文件，自动名称是启发式结果。材质仍使用含原始光照的照片。

接口、环境安装、提示格式和文件规范见 [Segmentation.md](tools/reconstruction/Segmentation.md)。`remesh.py` 能保留已有对象 ID 和 mask，无须加载模型。运行 `tools/Validate-Segmentation.ps1` 可复现本机示例的 12 组 GPU 截图及独立修改检查。

## 阶段 11：逐像素 2.5D 网格

```powershell
conda activate image-scene-renderer
# 输入照片 → 几何估计 → 每个有效像素一个顶点 → 带内嵌照片纹理的 GLB
python tools/reconstruction/reconstruct.py "input.jpg" --output generated/my-scene11 --geometry-backend moge --device cuda --max-size 512 --offline
# 也可以直接复用上阶段保存的预测；不运行或加载模型
python tools/reconstruction/remesh.py generated/scene10-cuda --output generated/my-remesh
.\build\Debug\ImageSceneRenderer.exe --package generated/my-remesh --render-mode albedo
```

默认逐像素三角化；显式 `--grid-size 129` 可生成粗网格。深度断层、无效像素、过长三维边和退化面会被过滤。输出为 `meshes/scene_mesh.glb`，内嵌 Base Color PNG；`textures/base_color.png` 同时保留供检查。包内不再覆盖 GLB 材质，单独 `--model .../meshes/scene_mesh.glb` 也能显示贴图，但只加载模型时不会恢复预测相机。

Viewport 选择 **Wireframe / Depth / Normal** 检查几何，**Albedo** 检查照片对齐。从图像区域开始右键拖动旋转，短按 A / D 平移观察近远物体视差。Wireframe 使用真正的线框 PSO；所有数据视图绕过曝光、Bloom 和调色。Depth 为相机空间 Z 在 near/far 之间的线性灰度，包中 far 按深度范围设置，移动很远后可在 Camera Inspector 扩大它。

调参、离线 depth 反投影、过滤统计和复现验收见 [网格构建说明](tools/reconstruction/Geometry.md)。这仍是可见表面的 2.5D 网格；断层裂缝和移动后露出的空洞会保留，照片中已有的光照也尚未分离。

## 编译

需要 Windows 10/11 x64、VS 2022 或更新版本的 C++ 桌面工具链、CMake 3.25+、Ninja、Windows SDK（本机验证 10.0.26100.0，包含 DXC）、DX12 / Shader Model 6.0 显卡。`--warp` 可使用软件设备。Debug 需要 Windows Graphics Tools 提供的 D3D12 Debug Layer，缺失时会明确失败。

```powershell
# 普通 PowerShell，工程根目录
.\tools\Build.ps1
# VS 未被 vswhere 发现时
.\tools\Build.ps1 -VisualStudioPath 'D:\VisualStudio'
```

脚本加载 x64 MSVC 环境，构建 Debug / Release，并对两种配置执行 CTest。也可在 x64 Developer PowerShell 中执行：

```powershell
cmake --preset windows
cmake --build --preset debug
cmake --build --preset release
ctest --test-dir build -C Debug --output-on-failure
ctest --test-dir build -C Release --output-on-failure
```

DXC DLL、shaders 与内置 HDR 环境自动复制到 EXE 旁；HLSL 在启动时编译。修改 Shader 后重新构建。DXC 可通过 `-DDXC_RUNTIME_DIR='C:\path\to\dxc\bin\x64'` 指定。中文编译器依赖检测异常时可用 `Build.ps1 -Fresh` 重建 CMake 缓存。

第三方源码已随工程固定版本保存，无需构建时下载：cgltf v1.15、MikkTSpace、Dear ImGui v1.92.9b、nlohmann/json v3.12.0。来源、许可与校验值见 [external/README.md](external/README.md)。

## 运行

```powershell
.\build\Debug\ImageSceneRenderer.exe
.\build\Debug\ImageSceneRenderer.exe --model "assets/models/MaterialLab/MaterialLab.gltf"
.\build\Release\ImageSceneRenderer.exe --model "assets/models/MaterialLab/MaterialLab.glb"
.\build\Debug\ImageSceneRenderer.exe --demo clear
.\build\Debug\ImageSceneRenderer.exe --demo triangle
.\build\Debug\ImageSceneRenderer.exe --lighting-test
.\build\Debug\ImageSceneRenderer.exe --lighting-test --env "assets/environments/SunsetCourtyard.hdr"
```

默认 `--demo scene` 显示程序化 Cube / Sphere / Plane；`--model` 通过命令行加载模型，支持带空格和中文的本地路径。交互运行默认打开 ImGui，可用 `--no-ui` 关闭；有限帧测试默认隐藏 UI，`--ui` 可强制显示。

上面的 EXE 命令兼容 CMD 与 PowerShell，路径统一使用双引号。**CMD 不把单引号当作引号**，例如 `--model 'assets/...'` 会尝试读取名字里含单引号的文件。请先进入工程根目录；CMD 使用 `cd /d "D:\UCL\render\Image_to_Editable_Scene Renderer"`，PowerShell 使用 `Set-Location "D:\UCL\render\Image_to_Editable_Scene Renderer"`。交互运行遇到加载失败会显示错误对话框和日志位置；`--smoke` / `--frames` 自动测试保留日志并返回非零退出码，不弹框。

界面为固定三栏，随主窗口大小调整：左侧 **Scene**、中间 **Viewport**、右侧 **Inspector**。当前不支持拖拽停靠。

1. 在 Scene 的 Meshes 中选择 Cube / Sphere / Plane 或导入模型节点。Inspector → Object 修改局部 Position / Rotation（度）/ Scale；选择带网格的节点后可修改 Visible 和材质。父节点变换会影响子节点；glTF 多 primitive 的材质在各 primitive 子节点上。
2. Material 支持 Base Color、Metallic、Roughness、Normal Strength、AO、AO Map Strength、Emissive。参数是线性空间中的材质因子，与贴图相乘。共享同一材质的节点会一起更新；Emissive 支持大于 1 的 HDR 数值。
3. 选择 Directional / Point Light 编辑颜色、强度、方向或位置 / 范围；方向光下有 Shadow / PCF / Bias 控件。
4. 选择 Environment 切换 HDRI、修改强度 / 旋转、开关 IBL 和 Skybox。
5. Inspector → Look 集中提供 Color Management、Color Grading、Bloom 和 Vignette。**Gamma 默认 1，表示额外调节中性；最终 sRGB 编码始终执行一次。** Reset display 恢复 ACES / EV 0 / Gamma 1；Reset all look 重置所有 Look 参数。选中其他场景对象会回到 Object 标签。
6. Viewport 顶部切换 Render Mode；Frame all 取景整个可见场景，Frame selected 取景选中节点及其可见子节点。选择 Camera 可以编辑相机位置、Yaw / Pitch、FOV 和裁剪面。

导入矩阵能无损近似分解为 TRS 时显示实际局部参数；镜像缩放保留。含剪切等不可可靠分解的矩阵保留为基底，面板明确显示 Local offset；编辑的 TRS 作用于该基底之前。缩放不能为零。

当前编辑仅保存在内存中，关闭后丢失；未实现保存、撤销 / 重做、画面拾取和变换 Gizmo。模型通过启动时 `--model` 加载，组合场景通过 `--package` 加载。

Render Mode 支持 Final / Albedo / Normal / Roughness / Metallic / Depth / Wireframe；面板还提供曝光和两类灯光参数。

| 输入 | 行为 |
| --- | --- |
| 从 Viewport 图像内开始右键拖动 | 相机旋转；拖出区域后保持直到松开 |
| W / A / S / D，Q / E | 鼠标位于 Viewport 或正在相机拖动时，前后左右、上下移动 |
| Shift | 加速 |
| 滚轮 | 沿视线移动 |
| 右键 + 滚轮 | 调整移动速度 |
| Alt + 右键拖动 | 围绕最近一次 LookAt / Frame 取景目标环绕 |
| R | 重置为默认相机 |
| Esc / 关闭按钮 / Alt+F4 | 退出；Esc 被编辑框捕获时优先由 UI 处理，Alt+F4 保留原生关闭行为 |

Viewport 图像虽然属于 ImGui 窗口，仍能接收相机输入；面板拖动、弹出菜单与文本输入不会驱动相机。导入模型初始按包围盒取景；窄窗口可再按 Frame all。R 重置为基础场景默认相机，加载自定义模型时建议使用 Frame all / Frame selected。Resize 分别管理交换链与 Viewport 的 Depth / HDR / LDR 目标，相机宽高比始终跟随图像区域；最小化时暂停绘制。

窗口与 ImGui 共享同一个 Win32 鼠标捕获，避免重复 SetCapture 将按住状态清掉；快速拖动后松开仍处理最后一段位移。WindowInputTests 通过真实 WndProc / ImGui 后端验证嵌入图像、跨面板拖动、滚轮、弹出菜单 / 输入阻塞等路径。

## ScenePackage v1：文件交换

随工程提供可直接运行的 [assets/ScenePackage/scene.json](assets/ScenePackage/scene.json)。包含两组 MaterialLab、一张 HDR 环境、独立粗糙度/金属度贴图和一个示例分割标签 PNG。左侧保留 glTF 材质，右侧演示包内材质替换。

```powershell
.\build\Debug\ImageSceneRenderer.exe --package "assets/ScenePackage"
.\build\Debug\ImageSceneRenderer.exe --package "assets/ScenePackage/scene.json" --exposure 1
.\build\Debug\ValidateScenePackage.exe "assets/ScenePackage"
.\tools\Run-Smoke.ps1 -Package "assets/ScenePackage" -UI -Capture -LogName package
```

`--package` 接受目录或 `scene.json`，与 `--model` 互斥。相机、灯光、环境和 Look 使用包内设置；显式 CLI Look / 环境参数覆盖包内值，与参数排列顺序无关。运行后 UI 修改同一份 Scene / LookParameters。当前没有从 UI 导出编辑结果或热重载功能。

```text
ScenePackage/
  scene.json
  objects/      # 阶段 12：每区域 ID / mask / metadata / 可选 mesh
  meshes/       # .gltf/.glb 及其本地依赖
  textures/     # PNG/JPEG 材质贴图、Radiance .hdr 环境
  masks/        # segmentation.png 等标签数据
  debug/        # depth.exr / normal.exr / pointmap.exr 等分析数据
```

格式入口为 [schemas/scene-package.schema.json](schemas/scene-package.schema.json)：`version=1`、左手 Y-up、米、角度使用度。顶层必需 `camera / environment / lights / objects`；可选 `look / auxiliary`。C++ 构建时嵌入该 schema，Python 直接读取同一文件；字段、范围和默认值没有另写一份 Python schema。修改 schema 后重建 C++。包内所有文件引用相对包根目录，允许中文和空格，使用 `/`；拒绝绝对路径、URL、`..` 和逃出包的引用。整体移动文件夹后仍可加载。

Object 的 `material` 缺省时保留 glTF 材质；存在时替换该 Object 下所有 primitive 的材质，使用 UV0。`baseColor / normal / roughness / metallic / emissive / occlusion` 是贴图路径；数值因子另用 `baseColorFactor / roughnessFactor / metallicFactor / emissiveFactor / normalStrength / ao / occlusionStrength`。BaseColor 和 Emissive 为 sRGB，其他为 Linear；独立 roughness / metallic 读取 R 通道，在 CPU 合成 G=roughness、B=metallic，二者都提供时必须同尺寸。默认金属度、粗糙度因子都是 1；普通非金属需设置 `metallicFactor: 0`。

`auxiliary` 支持 `depth / normal / pointmap / segmentation`。EXR 在此阶段只检查引用并保留文件，不解码、不上传 GPU，也不把分析法线当成材质 normal map。HDR 环境仍使用 `.hdr`。空 `environment: {}` 关闭 IBL / Skybox；空灯光列表不会自动插入默认灯。

Python 3.10+ 标准库工具，无需安装 PyTorch、jsonschema 或 DX12 Python 绑定：

```powershell
python tools/reconstruction/scene_package.py example "generated/MyScenePackage"
python tools/reconstruction/scene_package.py validate "generated/MyScenePackage"
python tools/reconstruction/scene_package.py normalize "generated/MyScenePackage"
.\build\Debug\ImageSceneRenderer.exe --package "generated/MyScenePackage"
python tools/reconstruction/test_scene_package.py --validator "build/Debug/ValidateScenePackage.exe"
```

示例命令要求目标目录为空，避免覆盖已有包。API 提供 `create_package / copy_asset / add_auxiliary / write_package / load_package`，写 manifest 前校验，通过临时文件替换保存。Python 校验结构、语义和文件引用；C++ 校验器还实际解码网格与材质纹理。两者使用本项目 schema 所需的 JSON Schema 关键字子集，未知 schema 关键字明确报错，不声称实现完整 Draft 2020-12。

CMake 若发现 Python 3.10+ 会增加 `ScenePackageInterop`，未安装 Python 仍可构建/运行 Renderer 和 C++ 测试。C++ `ScenePackageTests` 检查层级重映射、模型缓存、纹理复用、材质通道和 schema/Look 默认值；互通测试覆盖中文/空格路径、搬移包、错误输入和失败写入保持原文件。

## 单图几何估计（MoGe-2 / Dummy）

阶段 10 通过 `GeometryEstimationBackend` 统一输出深度、法线、点图、相机内参和 confidence。官方 MoGe-2 被包装在可选 Adapter 内，Renderer 仍只读取 ScenePackage，不需要 Python / PyTorch。

```powershell
# 当前电脑已创建此专用 Conda 环境；在 Miniconda Prompt 中激活。
conda activate image-scene-renderer
python tools/reconstruction/reconstruct.py "input.jpg" --output "generated/my-cuda-scene" --geometry-backend moge --device cuda --max-size 512 --grid-size 129
.\build\Debug\ImageSceneRenderer.exe --package "generated/my-cuda-scene"
```

新机器可用 `conda env create -f tools/reconstruction/environment.yml` 创建相同的 Python 3.13 / PyTorch 2.8 CUDA 12.9 环境，再运行 `python tools/reconstruction/fetch_moge_sources.py`。已有环境无需重复创建。旧 `.venv` 的 CPU 路径继续可用，详见 [环境说明](tools/reconstruction/MoGe.md)。

首次 MoGe 推理下载固定版本的官方 small normal 权重（约 141 MB）；源码和权重缓存在 `tools/reconstruction/` 的 Git 忽略目录。几何 confidence 是 **0/1 有效区域**，不是预测精度概率。网格只表示照片中可见表面；分割默认 Dummy、可选 SAM 2，材质默认中性 fallback、可选 Marigold。不训练模型。

`debug/depth.png`、`normal.png`、`confidence.png` 为预览，完整数值见 EXR / `geometry.npz`，内参与 FOV 见 `debug/camera.json`。`--geometry-backend dummy` 保留无模型路径；`--offline` 使用已缓存权重。

完整说明见 [MoGe 接入文档](tools/reconstruction/MoGe.md)。本地学习内容见 `docs/learning/10-geometry-estimation.md` 和 `docs/interview/10-qa.md`，均不上传 GitHub。

## Python Dummy Reconstruction Pipeline

阶段 09 的默认 Dummy 路径打通 `输入 JPEG/PNG → Python Dummy Backend → ScenePackage → DX12 Renderer`。Dummy 输出是一张带输入图像材质的平面网格，固定深度、默认法线、单对象标签和默认非金属材质；这条路径不运行 AI 推理。

需要 Python 3.12+（已验证 3.13.2 x64），只在局部 `.venv` 安装 NumPy / Pillow / OpenEXR，无模型权重、PyTorch 或 CUDA。C++ 无需这个 Python 环境，也没有新增 Python 运行时依赖。

```powershell
# 工程根目录，首次安装；也可加 -Python "D:\miniconda\python.exe"
.\tools\reconstruction\setup.ps1

# 不必激活环境，直接使用隔离环境里的解释器
.\tools\reconstruction\.venv\Scripts\python.exe tools/reconstruction/reconstruct.py "input.jpg" --output "generated/scene01"
.\build\Debug\ImageSceneRenderer.exe --package "generated/scene01"
```

在 `tools/reconstruction/` 内激活环境后，也可使用 `python reconstruct.py input.jpg --output generated/scene01`。输入和输出路径相对当前工作目录；目标必须为新目录或空目录。局部环境及生成文件已加入 Git 忽略。

模块化接口位于 `pipeline/depth_backend.py`、`normal_backend.py`、`segmentation_backend.py`、`material_backend.py`；各自实现 `predict(image)`。`geometry_builder.py` 负责反投影/网格，`scene_exporter.py` 复用已有 ScenePackage schema。PNG/JPEG 先统一方向、色彩和尺寸；输出包含真实 float32 `depth.exr / normal.exr / pointmap.exr`、标签 PNG、GLB 与 RGB 材质。EXR 由 Python 写入并在测试中回读，Renderer 仍只通过 GLB/PNG 绘制场景。

三栏 Viewport 比输入图像窄时可点 **Frame all**。选择 Image Surface 父节点即可编辑材质；默认是单面平面，从背面观察会被剔除。阶段 13 起新 CLI 默认使用中性材质，可在 Original Image 模式检查输入照片；历史阶段 09 包仍使用照片占位 Base Color，需在 Albedo 模式检查。Final 不能视为 Reference Matching。

```powershell
# CPU 数值/文件与 C++ 互通，再进行有限帧窗口验收
.\tools\reconstruction\validate.ps1
# 只运行 CPU 与 C++ 加载测试
.\tools\reconstruction\validate.ps1 -SkipWindows
```

选项、输出布局、无输入时的测试图生成和后端替换方法见 [tools/reconstruction/README.md](tools/reconstruction/README.md)。本地详细教程为 `docs/learning/09-dummy-reconstruction.md`，附面试 Q&A 和验证记录。

## glTF 支持与边界

- `.gltf` + 外部 buffer / PNG / JPEG，以及 `.glb` 内嵌 bufferView 图像、base64 图像。
- POSITION / NORMAL / TANGENT / TEXCOORD_0 / TEXCOORD_1 / COLOR_0；8/16/32 位无符号索引、无索引三角形、strip、fan；交错、normalized 和 sparse accessor。
- 节点 TRS / matrix、父子层级、多 primitive、同网格多实例、负行列式变换。统一转换到 Renderer 左手坐标；不自动翻转 UV。
- 缺失 NORMAL 时生成平面法线；缺失 TANGENT 时使用 MikkTSpace，按角点保留 UV 接缝。
- Base Color / Emissive 使用 sRGB SRV；Normal / Metallic-Roughness / AO 使用线性 SRV。ORM 使用 R=AO、G=Roughness、B=Metallic。
- CPU WIC 解码去重、GPU 按图像与色彩用途缓存、采样器表去重。CPU 生成完整 mip 链，颜色 mip 在线性空间滤波，alpha 不做 gamma 转换。
- 支持 OPAQUE / MASK / 简单排序 BLEND、double-sided、KHR_texture_transform、KHR_materials_unlit、KHR_materials_emissive_strength、KHR_mesh_quantization。
- 当前不支持动画、蒙皮、morph、Draco、Meshopt、KTX2/BasisU、glTF camera / light 导入。未知 required extension 明确拒绝；其他材质扩展暂未实现。透明物体按实体原点距离排序，不保证相交透明物体正确。
- 为早期实现设置了容量边界：每帧 4 MiB 常量（同时执行阴影和主 Pass 时约 4095 个物体）、2048 个 sampler 描述符（复用相同表）、普通图像最大 16384 边长 / 解码后 512 MiB、单文件最大 1 GiB。尚无通用流式加载、压缩或资源回收系统。

测试场景是本项目生成的 MaterialLab，包含共享 ORM / Base Color、缺失切线、层级变换、镜像球体与四张纹理。生成源为 [tools/GenerateAssets.cpp](tools/GenerateAssets.cpp)，可重新生成：

```powershell
.\build\Debug\GenerateAssets.exe assets/models/MaterialLab
```

## PBR 与色彩管线

Cook–Torrance 使用 GGX NDF、Smith GGX 几何项、Schlick Fresnel，支持金属/粗糙度、切线空间法线、AO 与自发光。支持最多 8 个方向/点光源，点光按距离平方衰减并在 range 附近平滑归零。

```text
sRGB 颜色贴图 → 硬件解码 → 线性材质与光照
                          ↓
                   RGBA32_FLOAT HDR
                          ↓
                 Bloom 提取 / 降采样 / 上采样 / HDR 合成
                          ↓
                 曝光 2^EV → Temperature / Tint → Saturation → Contrast → Vignette
                          ↓
                 None / Reinhard / ACES
                          ↓
                 额外 Gamma（默认 1）→ 精确分段 sRGB 编码
                          ↓
                 SDR Viewport 纹理 → ImGui → SDR Swap Chain
```

None 将超范围值截断；Reinhard 使用 `x/(1+x)`；ACES 使用 filmic 拟合曲线，不是完整 ACES 色彩管理。UI 在后处理之后合成，不受 Bloom、调色或暗角影响。关闭 UI 时直接输出到交换链。输出仍是 SDR 窗口，浮点中间缓冲不代表 HDR10 显示输出。

| 视图 | 输出含义 |
| --- | --- |
| Final | PBR + HDR 后处理 + tone mapping + Gamma 调节 + sRGB |
| Albedo | 贴图 × 因子 × 顶点色，sRGB 显示，无曝光 |
| Normal | 法线映射后的世界法线 `N*0.5+0.5`，原始数值显示 |
| Roughness / Metallic | 贴图通道 × 因子，灰度数值显示 |
| Depth | 线性视空间深度映射到 near–far 的 0–1；背景为 1 |

直接光与 IBL 都在线性 HDR 空间累加。AO 影响环境项，不压暗直接光或自发光；对环境镜面项目前使用简单 AO 乘数。Ambient diffuse 保留为可选常量近似，默认 0。尚无 GI、环境遮挡追踪、SSR、抗锯齿或多重散射补偿。

## 后处理与统一 Look 参数

`src/Scene/LookParameters.h` 是外观参数的唯一数据源，存放在 `RenderSettings::look`。UI、CLI 和运行时修改都写这一份数据；Renderer 每帧从它生成 GPU 常量。字段名称、范围、有限值校验集中在 `LookParameterSchema`，供后续 Reference Matching 使用，没有引入 AI filter。

| 参数 | 范围 / 默认 | 含义 |
| --- | --- | --- |
| Exposure | -16–16 EV / 0 | 线性 HDR 亮度乘 `2^EV` |
| Temperature / Tint | -1–1 / 0 | 正值分别偏暖、偏洋红；负值偏冷、偏绿；艺术调节，不是 Kelvin 标定 |
| Saturation | 0–2 / 1 | 0 为 Rec.709 亮度灰阶，1 为原饱和度 |
| Contrast | 0.25–2 / 1 | 围绕线性 18% 灰的幂曲线，在 Tone Mapping 前应用 |
| Bloom | 默认关闭 | 启用后，Intensity 0–2，默认 0.08 |
| Bloom Threshold / Soft Knee | 0–20 / 1；0–1 / 0.5 | 曝光前 HDR 阈值与柔和过渡，避免亮度经过阈值时突变 |
| Bloom Radius | 0.5–2 / 1 | 上采样 tent 核的扩散半径，单位为源层 texel |
| Vignette Intensity | 0–1 / 0 | 0 关闭，中心保持，边缘渐暗 |
| Vignette Radius / Softness | 0–1 / 0.35；0.01–1 / 0.65 | 按 Viewport 宽高比校正的渐暗起点与过渡宽度 |
| Gamma | 0.1–4 / 1 | Tone Mapping 后的额外调整，之后仍执行准确 sRGB 编码 |

Bloom 使用最多 5 层、从半分辨率开始的 HDR 金字塔。降采样先按样本提取高亮，之后用归一化 tent 上采样逐级合成；不同尺寸不会因为层数变化而让恒定输入被重复累加放大。初版没有时间滤波或镜头污渍；高频亮点仍可能随运动闪烁。Color Grading 使用 RGB 平衡和解析公式，未实现 LUT 或完整色域管理。

新的效果默认中性，默认画面保持阶段 06 的结果。Albedo / Normal / Roughness / Metallic / Depth 都绕过 HDR 后处理。`fogDensity` 已预留但尚不参与渲染，也不提供虚假的雾控件。外观编辑仍只保存在内存中。

```powershell
.\build\Debug\ImageSceneRenderer.exe --bloom --bloom-intensity 0.4 --temperature 0.5 --vignette 0.5
.\build\Debug\ImageSceneRenderer.exe --saturation 0
.\build\Debug\ImageSceneRenderer.exe --tone-mapping reinhard --exposure 1
```

扩展入口是 `PostProcessingPipeline`：HDR 阶段消费 / 返回 `PostProcessImage`，独立 Pass 使用 `PostProcessTarget`，最后统一交给 ToneMapPass。读取和写入使用不同目标；Resize 在等待 GPU 后重建纹理，复用固定 descriptor 槽位。当前有显式 Pass 顺序，尚未实现通用 Render Graph 或临时资源别名系统。

## 阴影与 HDR 环境

默认启用 2048×2048 方向光 Shadow Map 与 3×3 PCF。选择方向光后的 `Shadow` 面板可启停阴影、选择 Hard / PCF 3×3 / PCF 5×5，调整 Shadow Bias 与 Normal Bias。接收平面斜率校正防止 PCF 在斜面产生条纹。Shadow Bias 使用归一化光空间深度；Normal Bias 使用世界单位（glTF 按米理解）。过大的 bias 会让阴影脱离物体。

目前为**第一个启用的方向光**生成单张正交 Shadow Map，范围按可见场景包围盒拟合；其他方向光和点光不投影。支持 OPAQUE / MASK 投影、镜像节点和双面材质；BLEND 不投射阴影。阴影只影响对应的直接光，环境光仍能照亮阴影区域。尚无 CSM、面积光软阴影或点光阴影，超大场景会降低局部阴影精度。

`Environment` 面板可选择 HDRI，分别开关 IBL 与 Skybox，调整强度和 Y 轴旋转角；输入本地 `.hdr` 路径后点击 `Load HDR` 可加载其他文件。初次加载同步生成 IBL，可能短暂停顿；最多缓存 3 份 GPU 环境。切换前等待旧帧完成，失败时保留原环境并显示错误。默认启动环境为 SoftStudio，编辑暂不保存。

环境系统生成并实际用于着色的资源：

| 资源 | 配置 | 用途 |
| --- | --- | --- |
| Sky Cubemap | 256² × 6，9 mips | 天空显示、卷积输入 |
| Diffuse Irradiance | 32² × 6 | 漫反射半球积分，存储 E，着色时除 π |
| Specular Prefilter | 128² × 6，8 mips | GGX 重要性采样，roughness 选择 mip |
| BRDF LUT | 128²，环境间共享 | 按 N·V / roughness 查 split-sum A、B |

预计算使用 DX12 Compute Shader，每个卷积像素 256 个样本；HDR 资源均为 RGBA32_FLOAT。强度/旋转在采样阶段生效，无须重新卷积。HDRI 是方向光场，旋转同时影响天空、漫反射与镜面反射；无局部探针视差校正。

导入支持 Radiance RGBE `.hdr`、Y-major ±Y / ±X、现代 scanline RLE 和原始 RGBE；不支持 EXR、XYZE、旧式 RGBE repeat 编码。上限 16384×8192 且总像素不超过 32M；大图的 CPU mip / upload 还会占用额外内存。输入按线性 RGB 辐射亮度处理，不进行 sRGB 解码，也不自动应用 Radiance EXPOSURE / COLORCORR / PRIMARIES 元数据；建议使用标准线性 RGB 的等距柱状全景图。

附带两张本项目程序化生成的 HDR 测试环境：SoftStudio（大面积灯板）和 SunsetCourtyard（暖色太阳/地平线）。它们是原创测试数据，不是实拍 HDRI；来源及生成方式见 [assets/environments/README.md](assets/environments/README.md)。重新生成后需重新构建以复制运行资产：

```powershell
.\build\Debug\GenerateEnvironments.exe assets/environments
.\build\Debug\ImageSceneRenderer.exe --lighting-test --lights none --no-sky
# 此时切换 HDRI，只观察球体与平面的变化，背景保持不变
```

`--lighting-test` 将球体设置为金属度 1、粗糙度 0.12 的中性金属，便于观察灯板/地平线反射。标准场景仍保留原来的材质。

## 验证

```powershell
.\tools\Validate.ps1       # 00–02 的 9 项窗口回归
.\tools\Validate-PBR.ps1   # 03–04 的 17 项窗口验收与图像比较
.\tools\Validate-Lighting.ps1 # 05 的 21 项阴影 / IBL / 环境切换验收
.\tools\Validate-Startup.cmd # CMD 模型路径、错误提示与退出码回归
.\tools\Validate-LookDev.ps1 # 06 的 15 项 Viewport / 色彩管理 / 模型验收
.\tools\Validate-PostProcessing.ps1 # 07 的 26 项效果 / 参数一致性 / 数据绕过验收
.\tools\Validate-ScenePackage.ps1 # 08 的包加载 / UI / 相机 / CLI 覆盖 / Release / WARP
.\tools\Validate-Segmentation.ps1 # 12 的独立对象隐藏 / 恢复 / 材质修改
.\tools\Validate-Material.ps1 # 13 的材质通道 / 原图对照 / 原始贴图视图 / IBL 与 Shadow 回归
.\tools\Run-Smoke.ps1 -Model "assets/models/MaterialLab/MaterialLab.gltf" -Capture -LogName my-pbr
.\tools\Run-Smoke.ps1 -RenderMode roughness -Capture -LogName roughness
.\tools\Run-Smoke.ps1 -MaterialSmoke -Exposure 2 -UI -Capture -LogName edit
```

CLI 还支持 `--render-mode final|albedo|normal|roughness|metallic|depth|wireframe|original|estimated-albedo|estimated-normal|estimated-roughness`、`--exposure EV`、`--ambient 0..1`、`--lights all|directional|point|none`、`--frames N`、`--smoke`、`--material-smoke`、`--camera-smoke`、`--reverse-order`、`--capture path.bmp`、`--warp`、`--log path.log`。

阶段 06 增加 `--tone-mapping none|reinhard|aces` 与 `--gamma 0.1..4`。例如 `ImageSceneRenderer.exe --tone-mapping reinhard --exposure 1 --gamma 1`。无 UI 时也可以使用，便于可重复的图像比较。

阶段 07 CLI：`--temperature`、`--tint`、`--saturation`、`--contrast`、`--bloom` / `--no-bloom`、`--bloom-intensity`、`--bloom-threshold`、`--bloom-knee`、`--bloom-radius`、`--vignette`、`--vignette-radius`、`--vignette-softness`。范围见上表；有限值、越界与带多余字符的数值会被拒绝。`--look-smoke` 在 30 / 50 / 70 帧修改 / 重置 / 再次修改同一份参数，以验证实时更新与关闭效果后的资源复用。

阶段 05 CLI：`--env path.hdr`、`--env-intensity 0..100`、`--env-rotation degrees`、`--no-ibl`、`--no-sky`、`--no-shadows`、`--shadow-pcf 0|1|2`（对应 1 / 9 / 25 taps）、`--shadow-bias 0..0.05`、`--normal-bias 0..1`、`--lighting-test`。`--environment-smoke` 在第 30 / 50 / 70 帧请求 Sunset / 无效路径 / Studio，验证失败恢复与缓存复用。

Smoke 会缩放、最小化并恢复窗口。BMP 由 GPU Readback 保存；捕获时额外回读 HDR 并统计峰值与非有限像素。正常交互不会逐帧读回。自动比较覆盖六种模式差异、glTF/GLB 等价、深度顺序不变、两类灯光贡献、曝光、运行时材质修改和相机变化。

CTest 包含 SceneTests、ResourceTests、AssetTests、PbrTests、LightingTests、WindowInputTests、PostProcessingTests、ScenePackageTests，以及发现 Python 时加入的 ScenePackageInterop。新增 PostProcessingTests 在 WARP 上实际运行 HDR Pass 并回读：39,777 项检查涵盖 Bloom 高亮扩散 / 常量响应 / 阈值、灰阶、冷暖与 Tint、18% 灰对比度、暗角、1×1 / 奇数尺寸、descriptor 复用和参数校验。PbrTests 保留 256 个 BRDF 样本与 72 个色彩管理样本。其他测试继续覆盖场景、资产、HDR / IBL / Shadow 与 Win32 输入。

Validate-PostProcessing 的 26 组窗口测试覆盖全部独立效果、实时 Look 修改与 CLI 输出完全一致、五种数据视图不受效果影响、灰阶像素、glTF/GLB、Debug/Release/WARP/UI，以及 resize / 最小化恢复。

Validate-Lighting 在关闭天空和直接光时分别比较金属球、漫反射物体、背景的像素，避免把“背景切换”误认成 IBL；还检查 PCF 自阴影条纹、两类 bias、点光不受方向阴影影响、环境失败恢复、缓存往返、Release、WARP 与 UI。

日志输出到文件和 OutputDebugString。Debug 启用验证层，并记录 errors / warnings；Smoke 脚本要求两者都为 0。Release 不启用验证层，其正常运行不能代替 Debug 检查。

## 代码与本地学习资料

`src/Assets/` 只负责 CPU 解析；`src/Scene/` 保存可编辑数据；`src/Renderer/` 管理 DX12 资源和 Pass；`src/UI/` 的 LookDevelopmentUI 管理后端，Layout / SceneHierarchy / InspectorPanels / EnvironmentPanel 管理各面板，ViewportInput 管理输入归属。GpuScene 复用主 Pass 与 Shadow Pass 的网格/材质；EnvironmentManager 管理 HDR 选择与缓存，EnvironmentBaker 执行 GPU 卷积。Shader 按功能拆分，ColorManagement.hlsli 供 ToneMap 和 GPU 数值测试共用。

`docs/` 内有各阶段中文入门教程、面试 Q&A、排错与实测记录。**整个 `/docs/` 已被 `.gitignore` 排除，不上传 GitHub。** `build/`、`generated/` 也被忽略。当前尚未实现隐藏几何补全、编辑保存、RPC、网络服务或参数优化。
