#include "UI/ChineseText.h"
#include "UI/SourceImagePanel.h"
#include "Assets/AssetIO.h"
namespace isr {
bool DrawWorkModeControls(RelightingSession& session){
    const auto previous=session.Mode();ImGui::TextUnformatted("工作模式");ImGui::SameLine();
    if(ImGui::RadioButton("三维场景###3D Scene",previous==WorkMode::Scene3D))session.SetMode(WorkMode::Scene3D);
    if(ImGui::GetContentRegionAvail().x>150)ImGui::SameLine();ImGui::BeginDisabled(!session.CanDisplayImage());
    if(ImGui::RadioButton("图像重光照###Image Relighting",previous==WorkMode::ImageRelighting))session.SetMode(WorkMode::ImageRelighting);
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip(session.CanDisplayImage()?
        "查看原图、分析数据和相对漫反射重光照；来源相机保持固定。":"此包没有经过验证的原图资产，仍可使用三维场景。");
    return session.Mode()!=previous;
}
void DrawSourceInformation(const RelightingSession& session,uint32_t vw,uint32_t vh){
    ImGui::TextUnformatted("原图 / 分析观测数据");
    if(!session.CanDisplayImage()){ImGui::TextWrapped("没有经过验证的原图资产。请打开由重建管线创建或升级的场景包。");return;}
    const auto& source=*session.Source();const auto& anchor=*source.anchor;const auto& metadata=anchor.metadata;
    ImGui::TextWrapped("%s",metadata["sourceKind"]=="canonical"?"规范原图：RGB8、sRGB，保留方向校正后的原始尺寸。":"旧版处理图，无法提供原始分辨率。");
    ImGui::TextWrapped("场景包：%s",PathUtf8(source.packageRoot).c_str());
    ImGui::Text("原图：%u × %u",anchor.sourceSize[0],anchor.sourceSize[1]);
    ImGui::Text("分析图：%u × %u",anchor.analysisSize[0],anchor.analysisSize[1]);
    if(int(session.imageView)>=74){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        ImGui::TextWrapped("视线距离为相机空间点的长度，不是相机 Z 深度。距离预览除以受支持的最大值；紫色表示无效。透射率为实际生效值，保护区为 1；置信度和空气光贡献为原始线性值，不经过画面调整与曝光。尺度未知或缺少贴图时不加雾；单位和覆盖率见捕获报告。");
    }else if(int(session.imageView)>=64){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(!source.shadow||!source.intrinsic||!source.analysisMaps)ImGui::TextWrapped("不可用：成对投射阴影需要旧阴影、本征分解和来源几何数据。");
        else if(!session.lighting.cacheValid)ImGui::TextWrapped("不可用：来源光照校准使旧阴影数据失效。请重新拟合、分析并导出新包。");
        else ImGui::TextWrapped("旧、新阴影使用独立的 2048 贴图与固定的左手系观测几何。可见性：白色为未遮挡、黑色为遮挡、紫色为不受支持的接收面。仅对直射光叠加有界变化，不遮蔽环境光或自发光。读回报告记录实际覆盖率和资源预算。");
    }else if(int(session.imageView)>=55){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(!source.shadow)ImGui::TextWrapped("不可用：请先分析已保存数据中的旧阴影。");
        else if(!session.lighting.cacheValid)ImGui::TextWrapped("不可用：来源校准已改变，请重新拟合、分析并导出新包。");
        else{const auto& d=source.shadow->metadata;ImGui::TextWrapped("%s | 仅分析，最终效果不变。",ChineseText(d["diagnostics"]["backend"].get<std::string>()).c_str());
            ImGui::Text("候选像素 %u | 未知像素 %u",d["diagnostics"]["candidatePixels"].get<unsigned>(),d["diagnostics"]["unknownPixels"].get<unsigned>());
            ImGui::Text("固定明暗尺度 %.6g",d["parameters"]["shadingScale"].get<double>());
            ImGui::TextWrapped("未知不等于确定受光。仅支持可见、跨区域的深度几何遮挡。手工 PNG 图层采用归一化原图坐标和最近邻映射，不改变原图 RGB。");}
    }else if(int(session.imageView)>=46){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        ImGui::TextWrapped("采用保守的非金属方向光 GGX 模型与固定来源视角。残差不会自动视为高光。紫色表示依据不足或来源校准已失效。此视图不重建玻璃、环境反射或投射阴影。");
    }else if(int(session.imageView)>=44){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        ImGui::TextWrapped(source.intrinsic?"本征漫反射支持度 / 受保护残差比例。这是启发式指标，不是高光识别；仅在本征拟合被接受后生效。":"不可用：缺少本征分解观测。");
    }else if(int(session.imageView)>=38){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(!source.intrinsic)ImGui::TextWrapped("不可用：缺少 intrinsic/intrinsic.json。请先对已保存图像进行本征分解。");
        else{const auto& d=source.intrinsic->metadata;ImGui::TextWrapped("后端：%s",ChineseText(d["provenance"]["backend"].get<std::string>()).c_str());
            ImGui::Text("重新合成均方根误差 %.5f",d["metrics"]["rmse"].get<double>());
            ImGui::TextWrapped("线性反照率 / 明暗 / 残差，使用模型原生相对尺度。残差不是高光蒙版。灰色表示零残差；误差显示放大 4 倍。");
            if(session.imageView==ImageDebugView::IntrinsicResidual&&d["residualSemantics"]=="unavailable")ImGui::TextWrapped("残差不可用：代理后端没有独立的残差估计。");
            ImGui::TextWrapped("不确定性：%s",ChineseText(d["provenance"]["uncertainty_semantics"].get<std::string>()).c_str());}
    }else if(int(session.imageView)>=24){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(uint64_t(anchor.sourceSize[0])*anchor.sourceSize[1]>16ull*1024*1024)ImGui::TextWrapped("不可用：派生渲染目标预算为 16M 像素 / 512 MiB。原图仍保留完整分辨率。");
        else ImGui::TextWrapped("原图线性 RGB 乘以有界对数比率。置信度是启发式指标，不是概率。无效或完全受保护的像素保留原图，不应用 ACES 或画面调整。质量视图：白色允许、黑色减弱；保护视图：白色保留原图。");
        ImGui::TextWrapped("MoGe 有效性、区域得分和材质一致性含义不同；切线法线置信度不是几何置信度。可用 --protection-mask 导入灰度 PNG（最大 2048 × 2048），采用原图最近邻映射。");
        if(!source.lighting||!source.analysisMaps)ImGui::TextWrapped("光照或分析数据不可用：回退为原图，不推断光照变化。");
    }else if(int(session.imageView)>=19){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(!source.lighting||!source.analysisMaps)ImGui::TextWrapped("不可用：需要几何法线、有效性和来源光照。");
        else ImGui::TextWrapped("GPU 计算单位反射率下的相对响应，已吸收 π 因子。来源相机固定且采用左手系；透明度通道表示有效性。不含反照率、阴影、高光或环境图照明。目标参数仅改变新明暗。");
    }else if(int(session.imageView)>=15){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(!source.lighting)ImGui::TextWrapped("不可用：缺少光照扩展文件。请先拟合已保存的观测数据。");
        else if(!session.lighting.cacheValid&&(session.imageView==ImageDebugView::OldShading||session.imageView==ImageDebugView::LightingResidual))ImGui::TextWrapped("不可用：缓存拟合属于上一次来源校准。");
        else ImGui::TextWrapped("左手系相机法线、最近邻采样；紫色表示排除或不可用。明暗预览固定除以 4；残差为归一化亮度的绝对误差。");
    }else if(int(session.imageView)>=2){
        const auto index=size_t(session.imageView)-2;ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(!source.analysisMaps)ImGui::TextWrapped("不可用：缺少 analysis/analysis.json 运行时贴图。请将已保存预测导出到新包。");
        else{const auto& map=source.analysisMaps->maps[index];const auto& m=map.metadata;
            if(!map.image)ImGui::TextWrapped("不可用：%s",ChineseText(m["reason"].get<std::string>()).c_str());
            else{
                ImGui::TextWrapped("%s | %s | %s",ChineseText(m["format"].get<std::string>()).c_str(),ChineseText(m["space"].get<std::string>()).c_str(),ChineseText(m["units"].get<std::string>()).c_str());
                ImGui::TextWrapped("%s: %s",ChineseText(m["provenance"]["kind"].get<std::string>()).c_str(),ChineseText(m["provenance"]["meaning"].get<std::string>()).c_str());
                ImGui::TextWrapped("后端：%s",ChineseText(m["provenance"]["backend"].get<std::string>()).c_str());
                ImGui::Text("范围：%.6g 至 %.6g",m["range"][0].get<double>(),m["range"][1].get<double>());
                ImGui::TextWrapped("紫色表示无效。采样方式：%s",ChineseText(m["sampling"].get<std::string>()).c_str());
            }}
    }
    ImGui::Text("视口：%u × %u",vw,vh);
    const auto r=FitSourceImage(anchor.sourceSize[0],anchor.sourceSize[1],vw,vh);
    ImGui::Text("图像矩形起点：%.2f，%.2f",r.x,r.y);ImGui::Text("尺寸：%.2f × %.2f | 比例 %.4f",r.width,r.height,r.scale);
    ImGui::TextWrapped("像素中心为 (i+0.5, j+0.5)，网格间隔为 64 个原图像素。");
    const auto& camera=metadata["sourceCamera"];
    ImGui::SeparatorText("来源相机（只读）");
    ImGui::TextWrapped("状态：%s",ChineseText(camera["status"].get<std::string>()).c_str());
    if(camera.contains("sourceIntrinsicsPixels")){
        const auto& k=camera["sourceIntrinsicsPixels"];
        ImGui::Text("fx/fy: %.3f / %.3f px",k[0].get<double>(),k[4].get<double>());
        ImGui::Text("cx/cy: %.3f / %.3f px",k[2].get<double>(),k[5].get<double>());
        ImGui::TextWrapped("左手系，Y 向上、+Z 向前；相机到世界为单位变换。尺度：%s",ChineseText(camera["scaleType"].get<std::string>()).c_str());
    }
    if(metadata["capabilities"]["requiresCalibration"].get<bool>())ImGui::TextWrapped("后续图像空间重建任务需要先校准来源相机。");
    if(source.analysisMaps){
        const auto& cameraData=source.analysisMaps->metadata["camera"];const auto& k=cameraData["intrinsicsPixels"];
        ImGui::SeparatorText("分析相机（只读）");
        ImGui::Text("fx/fy: %.3f / %.3f px",k[0].get<double>(),k[4].get<double>());
        ImGui::Text("cx/cy: %.3f / %.3f px",k[2].get<double>(),k[5].get<double>());
        ImGui::TextWrapped("左手系，Y 向上、+Z 向前；重建世界坐标等于相机坐标。%s",ChineseText(cameraData["scaleType"].get<std::string>()).c_str());
    }
    ImGui::SeparatorText("分析图映射");const auto& mapping=metadata["analysisMapping"]["sourceToAnalysis"];
    ImGui::Text("sx=%.9f  sy=%.9f",mapping[0].get<double>(),mapping[4].get<double>());
    ImGui::Text("分析资产：%zu 项",source.analysisArtifacts.size());
    if(!source.analysisMetadataDiagnostic.empty())ImGui::TextWrapped("%s",ChineseText(source.analysisMetadataDiagnostic).c_str());
    if(ImGui::CollapsingHeader("来源身份 / 元数据###Source identity / metadata")){
        ImGui::TextWrapped("%s",ChineseText(metadata["sourceId"].get<std::string>()).c_str());
        ImGui::TextWrapped("%s",ChineseText(metadata["sourceImage"]["sha256"].get<std::string>()).c_str());
        if(!source.analysisMetadata.is_null())ImGui::TextWrapped("%s",source.analysisMetadata.value("backends",package::Json::object()).dump().c_str());
    }
    ImGui::Text("显示缩放 %.3f | 平移 %.3f，%.3f",session.display.zoom,session.display.panX,session.display.panY);
    ImGui::TextWrapped("以上矩形是界面平移缩放前，图像适应渲染目标的结果。“适应窗口”仅重置显示映射；三维编辑、相机和画面调整不改变此观测数据。");
}
void DrawSourceCoordinates(const RelightingSession& session,ImVec2 origin,uint32_t vw,uint32_t vh,bool hovered){
    if(!session.CanDisplayImage())return;
    const auto& anchor=*session.Source()->anchor;const auto r=FitSourceImage(anchor.sourceSize[0],anchor.sourceSize[1],vw,vh);
    if(session.imageView==ImageDebugView::PixelGrid){
        ImGui::GetWindowDrawList()->AddRect({origin.x+r.x,origin.y+r.y},{origin.x+r.x+r.width,origin.y+r.y+r.height},IM_COL32(40,210,255,255));
        if(hovered){const auto mouse=ImGui::GetIO().MousePos;const float x=(mouse.x-origin.x-r.x)/r.scale,y=(mouse.y-origin.y-r.y)/r.scale;
            if(x>=0&&y>=0&&x<float(anchor.sourceSize[0])&&y<float(anchor.sourceSize[1])){
                ImGui::BeginTooltip();ImGui::Text("原图边缘坐标：%.2f，%.2f",x,y);
                ImGui::Text("纹素：%u，%u",uint32_t(x),uint32_t(y));ImGui::EndTooltip();
            }
        }
    }
}
}
