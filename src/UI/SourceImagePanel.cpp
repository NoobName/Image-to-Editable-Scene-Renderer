#include "UI/SourceImagePanel.h"
#include "Assets/AssetIO.h"
namespace isr {
bool DrawWorkModeControls(RelightingSession& session){
    const auto previous=session.Mode();ImGui::TextUnformatted("Mode");ImGui::SameLine();
    if(ImGui::RadioButton("3D Scene",previous==WorkMode::Scene3D))session.SetMode(WorkMode::Scene3D);
    if(ImGui::GetContentRegionAvail().x>150)ImGui::SameLine();ImGui::BeginDisabled(!session.CanDisplayImage());
    if(ImGui::RadioButton("Image Relighting",previous==WorkMode::ImageRelighting))session.SetMode(WorkMode::ImageRelighting);
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip(session.CanDisplayImage()?
        "Source, analysis and relative diffuse relighting; fixed source camera.":"This package has no validated source anchor. 3D remains available.");
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
    if(int(session.imageView)>=74){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        ImGui::TextWrapped("Ray distance = length(camera point), not camera Z. Distance preview divided by supported maximum; invalid = purple. Transmission is effective (protected = 1); confidence and airlight contribution are raw linear values, without Look/exposure. Unknown scale or absent maps: no fog. See capture report for units/coverage.");
    }else if(int(session.imageView)>=64){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(!source.shadow||!source.intrinsic||!source.analysisMaps)ImGui::TextWrapped("Unavailable: paired cast shadows require old-shadow, intrinsic and source geometry evidence.");
        else if(!session.lighting.cacheValid)ImGui::TextWrapped("Unavailable: source calibration invalidated old-shadow evidence. Refit and reanalyze into a new package.");
        else ImGui::TextWrapped("Independent old/new 2048 shadow maps; fixed observed LH source shell. Visibility: white=observed unblocked, black=blocked, purple=unsupported receiver. Additive bounded direct change; no ambient/emission mask. Readback report records actual coverage and budget diagnostics.");
    }else if(int(session.imageView)>=55){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(!source.shadow)ImGui::TextWrapped("Unavailable: analyze saved old-shadow evidence first.");
        else if(!session.lighting.cacheValid)ImGui::TextWrapped("Unavailable: source calibration changed; refit and reanalyze into a new package.");
        else{const auto& d=source.shadow->metadata;ImGui::TextWrapped("%s | analysis only; Final unchanged.",d["diagnostics"]["backend"].get<std::string>().c_str());
            ImGui::Text("Candidates %u | unknown %u",d["diagnostics"]["candidatePixels"].get<unsigned>(),d["diagnostics"]["unknownPixels"].get<unsigned>());
            ImGui::Text("Fixed shading scale %.6g",d["parameters"]["shadingScale"].get<double>());
            ImGui::TextWrapped("Unknown is not confirmed lit. Visible cross-region depth-shell support only. Manual PNG layers use normalized-source nearest mapping; no source RGB changes.");}
    }else if(int(session.imageView)>=46){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        ImGui::TextWrapped("Conservative dielectric directional GGX; fixed source view. Residual is not automatically specular. Purple: insufficient support or stale source calibration. No glass/environment/cast-shadow reconstruction.");
    }else if(int(session.imageView)>=44){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        ImGui::TextWrapped(source.intrinsic?"Intrinsic diffuse support / protected residual fraction. Heuristics, not specular identification. Applied only after accepted intrinsic fit.":"Unavailable: intrinsic observations absent.");
    }else if(int(session.imageView)>=38){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(!source.intrinsic)ImGui::TextWrapped("Unavailable: intrinsic/intrinsic.json absent. Estimate saved image intrinsics first.");
        else{const auto& d=source.intrinsic->metadata;ImGui::TextWrapped("Backend: %s",d["provenance"]["backend"].get<std::string>().c_str());
            ImGui::Text("Recomposition RMSE %.5f",d["metrics"]["rmse"].get<double>());
            ImGui::TextWrapped("Linear A/S/R; checkpoint-native relative scale. Residual is NOT a specular mask. Gray=zero residual; error display x4.");
            if(session.imageView==ImageDebugView::IntrinsicResidual&&d["residualSemantics"]=="unavailable")ImGui::TextWrapped("Residual unavailable: proxy provides no independent residual estimate.");
            ImGui::TextWrapped("Uncertainty: %s",d["provenance"]["uncertainty_semantics"].get<std::string>().c_str());}
    }else if(int(session.imageView)>=24){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(uint64_t(anchor.sourceSize[0])*anchor.sourceSize[1]>16ull*1024*1024)ImGui::TextWrapped("Unavailable: 16M-pixel / 512MiB derived target budget. Source remains full resolution.");
        else ImGui::TextWrapped("Original linear RGB x bounded log-ratio. Confidence is heuristic, not probability. Invalid/fully protected pixels preserve source. No ACES/Look. Quality views: white=allow, black=reduce; protection: white=keep source.");
        ImGui::TextWrapped("MoGe validity, region scores and material consistency keep separate meanings; tangent-normal confidence is not geometry confidence. Import gray PNG with --protection-mask (max 2048 square); nearest source mapping.");
        if(!source.lighting||!source.analysisMaps)ImGui::TextWrapped("Lighting/analysis unavailable: identity fallback; no lighting edit is inferred.");
    }else if(int(session.imageView)>=19){
        ImGui::SeparatorText(ImageDebugNames[int(session.imageView)]);
        if(!source.lighting||!source.analysisMaps)ImGui::TextWrapped("Unavailable: geometry normal, validity and source lighting are required.");
        else ImGui::TextWrapped("GPU unit-reflectance relative response; pi absorbed. Fixed LH source camera; alpha=validity. No albedo, shadows, specular or IBL. Target changes only New Shading.");
    }else if(int(session.imageView)>=15){
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
    ImGui::Text("Display zoom %.3f | pan %.3f, %.3f",session.display.zoom,session.display.panX,session.display.panY);
    ImGui::TextWrapped("Rect above is render-target fit before UI pan/zoom. Fit resets only the display mapping. 3D edits, camera and Look do not modify this observation.");
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
