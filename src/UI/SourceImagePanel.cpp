#include "UI/SourceImagePanel.h"
#include "Assets/AssetIO.h"
namespace isr {
bool DrawWorkModeControls(RelightingSession& session){
    const auto previous=session.Mode();ImGui::TextUnformatted("Mode");ImGui::SameLine();
    if(ImGui::RadioButton("3D Scene",previous==WorkMode::Scene3D))session.SetMode(WorkMode::Scene3D);
    ImGui::SameLine();ImGui::BeginDisabled(!session.CanDisplayImage());
    if(ImGui::RadioButton("Image Relighting",previous==WorkMode::ImageRelighting))session.SetMode(WorkMode::ImageRelighting);
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip(session.CanDisplayImage()?
        "Source, analysis and offline original-lighting diagnostics. No relit image output.":"This package has no validated source anchor. 3D remains available.");
    return session.Mode()!=previous;
}
void DrawSourceInformation(const RelightingSession& session,uint32_t vw,uint32_t vh){
    ImGui::TextUnformatted("Source / Analysis Observation");
    if(!session.CanDisplayImage()){ImGui::TextWrapped("No validated source anchor. Open a package created or upgraded by the reconstruction pipeline.");return;}
    const auto& source=*session.Source();const auto& anchor=*source.anchor;const auto& metadata=anchor.metadata;
    ImGui::TextWrapped("%s",metadata["sourceKind"]=="canonical"?"Canonical RGB8 sRGB; original oriented size.":"Legacy processed anchor; full resolution unavailable.");
    ImGui::TextWrapped("Package: %s",PathUtf8(source.packageRoot).c_str());
    ImGui::Text("Source: %u x %u",anchor.sourceSize[0],anchor.sourceSize[1]);
    ImGui::Text("Analysis: %u x %u",anchor.analysisSize[0],anchor.analysisSize[1]);
    if(int(session.imageView)>=15){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(!source.lighting)ImGui::TextWrapped("Unavailable: lighting sidecar absent. Fit saved observations first.");
        else if(!session.lighting.cacheValid&&(session.imageView==ImageDebugView::OldShading||session.imageView==ImageDebugView::LightingResidual))ImGui::TextWrapped("Unavailable: cached fit belongs to the previous source calibration.");
        else ImGui::TextWrapped("LH camera normals; nearest samples; purple = excluded/unavailable. Shading previews use fixed /4; residual is absolute normalized luminance error.");
    }else if(int(session.imageView)>=2){
        const auto index=size_t(session.imageView)-2;ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(!source.analysisMaps)ImGui::TextWrapped("Unavailable: no analysis/analysis.json runtime maps; export saved predictions to a new package.");
        else{const auto& map=source.analysisMaps->maps[index];const auto& m=map.metadata;
            if(!map.image)ImGui::TextWrapped("Unavailable: %s",m["reason"].get<std::string>().c_str());
            else{
                ImGui::TextWrapped("%s | %s | %s",m["format"].get<std::string>().c_str(),m["space"].get<std::string>().c_str(),m["units"].get<std::string>().c_str());
                ImGui::TextWrapped("%s: %s",m["provenance"]["kind"].get<std::string>().c_str(),m["provenance"]["meaning"].get<std::string>().c_str());
                ImGui::TextWrapped("Backend: %s",m["provenance"]["backend"].get<std::string>().c_str());
                ImGui::Text("Range: %.6g .. %.6g",m["range"][0].get<double>(),m["range"][1].get<double>());
                ImGui::TextWrapped("Invalid = purple. Sampling: %s",m["sampling"].get<std::string>().c_str());
            }}
    }
    ImGui::Text("Viewport: %u x %u",vw,vh);
    const auto r=FitSourceImage(anchor.sourceSize[0],anchor.sourceSize[1],vw,vh);
    ImGui::Text("Image rect: %.2f, %.2f",r.x,r.y);ImGui::Text("Size: %.2f x %.2f | scale %.4f",r.width,r.height,r.scale);
    ImGui::TextWrapped("Pixel centers: (i+0.5, j+0.5). Grid interval: 64 source pixels.");
    const auto& camera=metadata["sourceCamera"];
    ImGui::SeparatorText("Source camera (read-only)");
    ImGui::TextWrapped("Status: %s",camera["status"].get<std::string>().c_str());
    if(camera.contains("sourceIntrinsicsPixels")){
        const auto& k=camera["sourceIntrinsicsPixels"];
        ImGui::Text("fx/fy: %.3f / %.3f px",k[0].get<double>(),k[4].get<double>());
        ImGui::Text("cx/cy: %.3f / %.3f px",k[2].get<double>(),k[5].get<double>());
        ImGui::TextWrapped("LH, Y up, +Z forward; camera-to-world identity. Scale: %s",camera["scaleType"].get<std::string>().c_str());
    }
    if(metadata["capabilities"]["requiresCalibration"].get<bool>())ImGui::TextWrapped("Source-camera calibration required for future image-space reconstruction tasks.");
    if(source.analysisMaps){
        const auto& cameraData=source.analysisMaps->metadata["camera"];const auto& k=cameraData["intrinsicsPixels"];
        ImGui::SeparatorText("Analysis camera (read-only)");
        ImGui::Text("fx/fy: %.3f / %.3f px",k[0].get<double>(),k[4].get<double>());
        ImGui::Text("cx/cy: %.3f / %.3f px",k[2].get<double>(),k[5].get<double>());
        ImGui::TextWrapped("LH Y up, +Z forward; reconstruction world = camera. %s",cameraData["scaleType"].get<std::string>().c_str());
    }
    ImGui::SeparatorText("Analysis mapping");const auto& mapping=metadata["analysisMapping"]["sourceToAnalysis"];
    ImGui::Text("sx=%.9f  sy=%.9f",mapping[0].get<double>(),mapping[4].get<double>());
    ImGui::Text("Analysis artifacts: %zu",source.analysisArtifacts.size());
    if(!source.analysisMetadataDiagnostic.empty())ImGui::TextWrapped("%s",source.analysisMetadataDiagnostic.c_str());
    if(ImGui::CollapsingHeader("Source identity / metadata")){
        ImGui::TextWrapped("%s",metadata["sourceId"].get<std::string>().c_str());
        ImGui::TextWrapped("%s",metadata["sourceImage"]["sha256"].get<std::string>().c_str());
        if(!source.analysisMetadata.is_null())ImGui::TextWrapped("%s",source.analysisMetadata.value("backends",package::Json::object()).dump().c_str());
    }
    ImGui::TextWrapped("Fit is automatic. 3D edits, camera and Look do not modify this observation.");
}
void DrawSourceCoordinates(const RelightingSession& session,ImVec2 origin,uint32_t vw,uint32_t vh,bool hovered){
    if(!session.CanDisplayImage())return;
    const auto& anchor=*session.Source()->anchor;const auto r=FitSourceImage(anchor.sourceSize[0],anchor.sourceSize[1],vw,vh);
    if(session.imageView==ImageDebugView::PixelGrid){
        ImGui::GetWindowDrawList()->AddRect({origin.x+r.x,origin.y+r.y},{origin.x+r.x+r.width,origin.y+r.y+r.height},IM_COL32(40,210,255,255));
        if(hovered){const auto mouse=ImGui::GetIO().MousePos;const float x=(mouse.x-origin.x-r.x)/r.scale,y=(mouse.y-origin.y-r.y)/r.scale;
            if(x>=0&&y>=0&&x<float(anchor.sourceSize[0])&&y<float(anchor.sourceSize[1])){
                ImGui::BeginTooltip();ImGui::Text("Source edge coords: %.2f, %.2f",x,y);
                ImGui::Text("Texel: %u, %u",uint32_t(x),uint32_t(y));ImGui::EndTooltip();
            }
        }
    }
}
}
