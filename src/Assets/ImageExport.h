#pragma once
#include "Assets/NumericDds.h"
#include "Scene/TextureAsset.h"
namespace isr {
void ValidateNativeExportSize(uint32_t width,uint32_t height);
void WriteRgbPng(const std::filesystem::path&,const ImageData&);
void WriteNumericDds(const std::filesystem::path&,const NumericImage&,bool replace=false);
ImageData DisplayRgb(const NumericImage&,float exposure=0,bool encodeSrgb=true);
}
