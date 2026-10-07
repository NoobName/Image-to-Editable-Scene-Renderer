param([string]$Python='python',[switch]$Weights,[switch]$Dependencies)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$pipeline=Join-Path $PSScriptRoot 'reconstruction'
# Explicit opt-in only. Core startup never downloads code, weights, or dependencies.
& $Python "$pipeline/fetch_refinement.py"
if($LASTEXITCODE -ne 0){throw 'Pinned source fetch failed'}
if($Dependencies){
    & $Python -m pip install --target "$pipeline/.vendor/refinement-deps" --no-deps -r "$pipeline/requirements-refinement.txt"
    if($LASTEXITCODE -ne 0){throw 'Optional dependency install failed'}
}
if($Weights){
    $revision='5def370459c15ee36c524068bb8338b3f6c6d6c0'
    $destination=Join-Path $pipeline ".cache/huggingface/models--mlfarinha--pixlrelight/snapshots/$revision"
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    $files=@(
        @{Name='config.yaml';Sha='a3ef2cada8a223aee15b2e85d6627980962ea5446242d0d8ec24791ec2e21021'},
        @{Name='model.safetensors';Sha='69c2bd11c2f272754f7080bc33e4b049fd334ea5d596df0c475f51c93699710e'}
    )
    foreach($file in $files){
        $path=Join-Path $destination $file.Name
        if(Test-Path -LiteralPath $path){
            if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.Sha){throw "Cached optional asset hash mismatch: $path"}
            continue
        }
        if((Get-PSDrive ([IO.Path]::GetPathRoot($root).Substring(0,1))).Free -lt 6GB){throw 'Need 6 GiB free before optional download'}
        # A failed transfer leaves only a uniquely named partial file for diagnosis.
        $partial=$path+'.'+[guid]::NewGuid().ToString('N')+'.partial'
        Invoke-WebRequest -Uri "https://huggingface.co/mlfarinha/pixlrelight/resolve/$revision/$($file.Name)?download=true" -OutFile $partial
        if((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.Sha){throw 'Downloaded optional asset hash mismatch'}
        Move-Item -LiteralPath $partial -Destination $path
    }
}
Write-Output 'Optional source verified. Weights require -Weights; dependencies require -Dependencies. Review CC BY-NC 4.0 and inherited terms before use.'
