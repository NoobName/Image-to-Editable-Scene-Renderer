$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
function RunCase($name,$extra) {
    $parameters=@{LightingTest=$true;NoSky=$true;Lights='none';Frames=90;Capture=$true;LogName="lighting-$name"}
    foreach($key in $extra.Keys){$parameters[$key]=$extra[$key]}
    & "$PSScriptRoot/Run-Smoke.ps1" @parameters | Out-File (Join-Path $root "generated/lighting-$name-console.txt")
    $log=Get-Content (Join-Path $root "generated/lighting-$name.log") -Raw
    if($log -notmatch 'HDR statistics: max=[0-9.]+ mean=[0-9.]+ nonfinite=0'){throw "Nonfinite or missing HDR output: $name"}
    Write-Output "PASS: $name"
}
RunCase 'studio' @{}
RunCase 'sunset' @{Environment='assets/environments/SunsetCourtyard.hdr'}
RunCase 'ibl-off' @{NoIBL=$true}
RunCase 'intensity-zero' @{EnvironmentIntensity=0}
RunCase 'intensity-two' @{EnvironmentIntensity=2}
RunCase 'rotated' @{EnvironmentRotation=90}
RunCase 'sky-studio' @{NoSky=$false}
RunCase 'sky-sunset' @{NoSky=$false;Environment='assets/environments/SunsetCourtyard.hdr'}
RunCase 'shadow-off' @{NoIBL=$true;Lights='directional';NoShadows=$true}
RunCase 'hard' @{NoIBL=$true;Lights='directional';ShadowPcf=0}
RunCase 'pcf3' @{NoIBL=$true;Lights='directional';ShadowPcf=1}
RunCase 'pcf5' @{NoIBL=$true;Lights='directional';ShadowPcf=2}
RunCase 'depth-bias' @{NoIBL=$true;Lights='directional';ShadowBias=.015;NormalBias=0}
RunCase 'normal-bias' @{NoIBL=$true;Lights='directional';ShadowBias=0;NormalBias=.3}
RunCase 'point' @{NoIBL=$true;Lights='point'}
RunCase 'point-no-shadow' @{NoIBL=$true;Lights='point';NoShadows=$true}
RunCase 'switch' @{EnvironmentSmoke=$true}
RunCase 'switch-failure' @{EnvironmentSmoke=$true;Frames=65}
RunCase 'release' @{Configuration='Release'}
RunCase 'warp' @{Warp=$true}
RunCase 'ui' @{UI=$true;EnvironmentSmoke=$true;Lights='all';NoSky=$false}

# Compare object pixels with direct lights and the sky disabled: a background-only
# implementation cannot satisfy these assertions. File readback, no desktop dependency.
Add-Type -TypeDefinition @'
using System;
using System.IO;
public static class LightingPixels {
    public static double Difference(string first,string second,int x,int y,int w,int h) {
        byte[] a=File.ReadAllBytes(first), b=File.ReadAllBytes(second);
        int width=BitConverter.ToInt32(a,18), height=BitConverter.ToInt32(a,22);
        if(BitConverter.ToInt16(a,28)!=32 || BitConverter.ToInt16(b,28)!=32 ||
           width!=BitConverter.ToInt32(b,18) || height!=BitConverter.ToInt32(b,22))
            throw new Exception("Expected equal 32-bit BMP captures");
        int startA=BitConverter.ToInt32(a,10),startB=BitConverter.ToInt32(b,10);
        double sum=0;
        for(int row=y;row<y+h;row++)for(int col=x;col<x+w;col++) {
            int offset=((height<0?row:height-1-row)*width+col)*4;
            for(int c=0;c<3;c++)sum+=Math.Abs(a[startA+offset+c]-b[startB+offset+c]);
        }
        return sum/(w*h*3);
    }
}
'@
function PathFor($name){Join-Path $root "generated/lighting-$name.bmp"}
function Difference($a,$b,$region){[LightingPixels]::Difference((PathFor $a),(PathFor $b),$region[0],$region[1],$region[2],$region[3])}
function Assert($condition,$message){if(!$condition){throw $message}}
function Hash($name){(Get-FileHash (PathFor $name)).Hash}
$metal=@(690,300,110,100);$diffuse=@(490,300,90,80);$background=@(1100,50,100,100)
$metalChange=Difference 'studio' 'sunset' $metal
$diffuseChange=Difference 'studio' 'sunset' $diffuse
$backgroundChange=Difference 'studio' 'sunset' $background
Assert ($metalChange -gt 10) 'HDRI did not significantly change the metal reflection.'
Assert ($diffuseChange -gt 5) 'HDRI did not change diffuse illumination.'
Assert ($backgroundChange -eq 0) 'NoSky background changed with HDRI.'
Assert ((Difference 'studio' 'ibl-off' $metal) -gt 10) 'Metal sphere did not receive IBL.'
Assert ((Difference 'studio' 'rotated' $metal) -gt 10) 'Environment rotation did not affect reflection.'
Assert ((Difference 'sky-studio' 'sky-sunset' $background) -gt 10) 'Skybox did not switch HDRI.'
Assert ((Hash 'intensity-zero') -eq (Hash 'ibl-off')) 'Zero intensity differs from disabled IBL.'
Assert ((Difference 'studio' 'intensity-two' $metal) -gt 5) 'Environment intensity did not affect reflection.'
Assert ((Hash 'point') -eq (Hash 'point-no-shadow')) 'Directional shadow incorrectly affected a point light.'
Assert ((Hash 'switch') -eq (Hash 'studio')) 'A/B/A environment switch did not restore original output.'
Assert ((Hash 'switch-failure') -eq (Hash 'sunset')) 'Failed HDR import did not preserve the current environment.'
foreach($name in @('hard','pcf3','pcf5','depth-bias','normal-bias')){
    Assert ((Hash 'shadow-off') -ne (Hash $name)) "Shadow option has no visible effect: $name"
}
Assert ((Hash 'hard') -ne (Hash 'pcf5')) 'PCF has no visible effect.'
Assert ((Hash 'pcf3') -ne (Hash 'depth-bias')) 'Shadow depth bias has no visible effect.'
Assert ((Hash 'pcf3') -ne (Hash 'normal-bias')) 'Shadow normal bias has no visible effect.'
# The central unoccluded plane should not gain PCF self-shadow stripes.
$planeDelta=Difference 'hard' 'pcf5' @(350,530,200,100)
Assert ($planeDelta -lt .1) 'PCF introduced receiver-plane self-shadowing.'
$switchLog=Get-Content (Join-Path $root 'generated/lighting-switch.log') -Raw
Assert ($switchLog.Contains('Environment GPU cache hit')) 'Environment GPU cache was not reused.'
Assert ($switchLog.Contains('retaining previous environment')) 'Invalid environment fallback was not exercised.'
Write-Output "ROI mean absolute RGB difference [0..255]: metal=$metalChange diffuse=$diffuseChange background=$backgroundChange unoccluded-plane-PCF=$planeDelta"
Write-Output 'PASS: 21 lighting runs; real diffuse/specular IBL, sky, rotation, intensity, shadows, PCF, biases, failed import recovery, cached switching, Debug/Release/UI/WARP.'
