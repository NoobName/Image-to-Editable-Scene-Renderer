#pragma once
#include <string>
#include <string_view>
namespace isr {
// Translate only at the presentation boundary. Backend/progress keys, JSON and
// diagnostic logs retain their stable spelling; imported names are not rewritten.
inline std::string ChineseText(std::string_view text){
    struct Entry{std::string_view original,translated;};
    static constexpr Entry entries[]{
        {"Idle","空闲"},{"Processing","处理中"},{"Loading","加载中"},{"Ready","就绪"},{"Error","错误"},
        {"geometry","几何"},{"segmentation","区域分割"},{"materials","材质"},{"lighting","光照"},{"export","场景导出"},{"intrinsic","本征分解"},{"shadow","阴影"},
        {"pending","等待中"},{"running","处理中"},{"complete","已完成"},{"error","错误"},{"unavailable","不可用"},{"unknown","未知"},
        {"relative","相对尺度"},{"metric","公制尺度"},{"synthetic","合成数据"},{"estimated","估计结果"},{"fallback","回退结果"},{"manual","手工设置"},
        {"available","可用"},{"calibrated","已校准"},{"requires-calibration","需要校准"},{"accepted","已接受"},{"rejected","已拒绝"},{"degenerate","退化"},{"low-confidence","低置信度"},
        {"same-scene","同一场景"},{"different-content","不同内容"},
        {"moge","MoGe 几何预测"},{"sam2","SAM 2 区域分割"},{"marigold","Marigold 材质估计"},
        {"dummy","占位后端（测试）"},{"neutral","中性材质（回退）"},{"robust-directional-ambient","稳健方向光与环境光拟合"},
        {"manual-test","手工参数（测试）"},{"marigold-lighting","Marigold 本征分解"},{"proxy","代理估计（回退）"},{"intrinsic-assisted","本征辅助拟合"},
        {"Exposure (EV)","曝光（EV）"},{"Temperature","冷暖"},{"Tint","色偏"},{"Saturation","饱和度"},{"Contrast","对比度"},{"Gamma","伽马"},
        {"Intensity","强度"},{"Threshold","阈值"},{"Soft Knee","柔和过渡"},{"Radius","半径"},{"Softness","柔和度"},{"Fog Density (reserved)","雾密度（预留）"},
        {"Cube","立方体"},{"Sphere","球体"},{"Plane","平面"},{"Reconstructed Image Surface","重建图像表面"},
        {"Terracotta","陶土"},{"Jade","翡翠"},{"Slate","板岩"},
        {"Sky","天空"},{"Ground","地面"},{"Foreground","前景"},{"Background","背景"},{"Object","物体"},
        {"Save outside the source package. Export creates a new directory.","请保存到来源包之外。导出将创建新目录。"},
        {"Validating recipe and package on CPU...","正在 CPU 上校验配方和场景包…"},
        {"Native PNG/DDS export published.","原尺寸 PNG / DDS 已导出。"},
        {"Recipe published atomically. Source package unchanged.","配方已原子发布，来源包保持不变。"},
        {"Cancelled/stale load; previous document retained.","加载已取消或失效，保留上一份文档。"},
        {"Uploading recipe scene...","正在上传配方场景…"},
        {"Cancelled/stale upload; previous document retained.","上传已取消或失效，保留上一份文档。"},
        {"Recipe restored: explicit source baseline, target, response and protection.","配方已恢复：来源基准、目标、响应与保护参数均已加载。"},
        {"Optional offline candidate; never runs while dragging lights.","可选的离线候选生成，不会在拖动灯光时运行。"},
        {"Offline refinement running; physics remains interactive.","正在离线神经优化，物理结果仍可交互。"},
        {"Cancelled/stale candidate; current physics and previous comparison retained.","候选已取消或失效，保留当前物理结果和上次对比。"},
        {"Offline candidate ready. Inspect content changes in the comparison before using it.","离线候选已就绪，使用前请在对比中检查内容变化。"},
        {"Refinement failed/unavailable; physics retained. Inspect the process log or fallback comparison.","神经优化失败或不可用，已保留物理结果。请检查进程日志或回退对比。"},
        {"Analyze a reference or open a saved reference.json. No target is changed automatically.","请分析参考图或打开已保存的 reference.json。目标不会自动改变。"},
        {"Analyzing fixed reference observations in an offline Python process...","正在独立 Python 进程中离线分析固定参考观测…"},
        {"Cancelled/stale reference; previous target and observation retained.","参考分析已取消或失效，保留上一组目标和观测。"},
        {"Reference proposal ready. Inspect confidence, residual and parameter changes before Apply.","参考建议已就绪。应用前请检查置信度、残差和参数变化。"},
        {"Reference proposal applied; source and Exposure unchanged.","参考建议已应用，来源和曝光保持不变。"},
        {"Reference illumination unavailable; target retained.","参考照明不可用，目标保持不变。"},
        {"Reference target restored to pre-apply snapshot.","目标已恢复到应用参考建议前的状态。"},
        {"Choose File > Reconstruct Image","请选择“文件 → 从图像重建”。"},
        {"Starting Python pipeline","正在启动 Python 管线"},{"Cancelling reconstruction...","正在取消重建…"},
        {"Scene loaded. Materials and objects are editable.","场景已加载，可以编辑材质与物体。"},
        {"Loading ScenePackage and decoding textures...","正在加载场景包并解码纹理…"},{"Preparing GPU scene...","正在准备 GPU 场景…"},
        {"ScenePackage published","场景包已发布"},{"Building grid mesh and writing ScenePackage","正在构建网格并写入场景包"},
        {"Unavailable: legacy material has no estimated albedo","不可用：旧版材质没有估计反照率"},
        {"No corresponding backend output was retained","没有保留相应的后端输出"},
        {"nearest","最近邻"},{"nearest-valid","有效区最近邻"},{"linear","线性"},{"unitless","无量纲"},{"meters","米"},{"relative-units","相对单位"},
        {"camera-lh","相机左手系"},{"world-lh","世界左手系"},{"tangent","切线空间"},
        {"guarded-bilinear","受保护的双线性采样"},{"camera-z","相机 Z 深度"},{"lh-camera","相机左手系"},{"lh-world","世界左手系"},
        {"image","图像空间"},{"linear-rgb","线性 RGB"},{"unit-vector","单位向量"},{"binary","二值"},{"score","得分"},{"label-id","标签标识"},{"reflectance","反射率"},
        {"derived","派生数据"},{"synthetic-units","合成单位"},{"forward Z, not ray length","前向 Z 深度，不是视线长度"},{"observed camera XYZ","观测相机 XYZ 坐标"},
        {"signed geometric normal","带符号几何法线"},{"camera normal transformed by identity reconstruction cameraToWorld; unrelated to edited objects","由重建相机单位变换得到的世界法线，与编辑后的物体无关"},
        {"authoritative geometry validity","几何有效性的权威标记"},{"0=unassigned; IDs preserved exactly","0 表示未分配；标识值精确保留"},
        {"region proposal score; not calibrated semantic confidence","区域建议得分，不是校准后的语义置信度"},
        {"binary-validity-not-calibrated","二值有效性，未经置信度校准"},{"neutral-fallback","中性材质回退"},{"flat-tangent-fallback","平坦切线法线回退"},
        {"authored","人工制作"},{"unspecified quality","未指定的质量数据"},{"unspecified","未指定"},
        {"insufficient-valid-support","有效支持区域不足"},{"normal-diversity-rank-deficient","法线方向变化不足，无法确定光照"},
        {"directional-signal-too-small","方向光信号过弱"},{"ambiguous-direction-profile","光照方向存在歧义"},
        {"relative-coefficient-out-of-range","相对系数超出范围"},{"albedo-is-not-intrinsic","反照率并非本征分解结果"},
        {"Explicit manual/test calibration; not an automatic estimate","显式手工 / 测试校准，不是自动估计"},
        {"reference-fit-confidence-below-0.05","参考拟合置信度低于 0.05"},{"source-baseline-has-no-relative-energy","来源基准没有相对光照能量"},
        {"relative-intensity-clamped-to-64","相对强度已限制到 64"},
        {"unsupported-lighting-fields-preserve-source-baseline; calibrate manually","不支持的光照字段保留来源基准，请手工校准"},
        {"illumination-chromaticity-bounded-to-0.5..2-relative-luminance","照明色度已限制在相对亮度的 0.5 至 2 倍范围内"},
        {"synthetic-normal-fallback; no observed direction","回退为合成法线，没有观测光照方向"},
        {"neutral-albedo-fallback; object color is not illumination","回退为中性反照率；物体颜色不等于照明颜色"},
        {"cancelled","已取消"},{"no-improvement","没有改善"},{"improved","已改善"},
        {"cancelled-before-evaluation","评估开始前已取消"},{"reference-illumination-not-identifiable","参考照明不可辨识"},
        {"source-baseline-zero-energy","来源基准能量为零"},{"requested-direction-change-exceeds-90-degrees","请求的方向变化超过 90 度"},
        {"explicit-registered-pixel-alignment-required","需要显式确认像素配准对齐"},{"registered-resolution-mismatch","配准分辨率不一致"},
        {"registered-saved-observation-resolution-mismatch","已保存观测的配准分辨率不一致"},
        {"insufficient-unprotected-confident-support","未保护区域中有足够置信度的像素不足"},{"degenerate-source-normal-distribution","来源法线分布退化"},
        {"initial-parameters-outside-feasible-gauge","初始参数不满足可行尺度约束"},{"cancelled; original-target-retained","已取消，保留原目标"},
        {"no-significant-fixed-loss-improvement","固定目标损失没有明显改善"},
        {"registered-reference-not-explained-by-fixed-diffuse-observations","固定的漫反射观测无法解释配准后的参考图"},
        {"best-feasible-candidate; DX12 verification required before Apply","已找到最佳可行候选；应用前需要 DX12 验证"},
        {"zero: no material inference performed","零：没有进行材质推理"},
        {"zero: ensemble uncertainty unavailable; not a confidence estimate","零：集成不确定性不可用，不代表置信度估计"},
        {"1 - max ensemble standard deviation over albedo RGB / roughness / metallic; not calibrated accuracy; excludes fallback normal","1 减去反照率 RGB、粗糙度和金属度集成标准差的最大值；不是校准后的准确率，不含回退法线"},
        {"background","背景"},{"foreground","前景"},{"ground","地面"},{"sky","天空"},{"object","物体"},{"heuristic","启发式估计"}
    };
    for(const auto& entry:entries)if(text==entry.original)return std::string(entry.translated);
    static constexpr Entry prefixes[]{
        {"Recipe operation failed; previous document/files retained: ","配方操作失败，已保留原文档和文件。详细诊断："},
        {"Reference failed; current source/target retained: ","参考处理失败，已保留来源和目标。详细诊断："},
        {"Refinement stopped; physics retained: ","神经优化已停止，物理结果已保留。详细诊断："},
        {"File chooser failed: ","文件选择失败，错误码："},
        {"geometry-adapter-unavailable: ","几何适配器不可用，详细诊断："},{"material-adapter-unavailable: ","材质适配器不可用，详细诊断："},
        {"Optional debug/reconstruction.json ignored: ","已忽略可选的 debug/reconstruction.json，详细诊断："}
    };
    for(const auto& entry:prefixes)if(text.starts_with(entry.original))return std::string(entry.translated)+std::string(text.substr(entry.original.size()));
    return std::string(text); // Preserve unknown model diagnostics and user asset names verbatim.
}
}
