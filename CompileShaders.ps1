param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot "build")
)

$shaderDirectory = Join-Path $PSScriptRoot "Shaders"
$outputDirectory = Join-Path $BuildDirectory "Shaders"
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

$Version = "6_6"

foreach ($shader in Get-ChildItem -LiteralPath $shaderDirectory -Filter *.hlsl) {
    if ($shader.Name.EndsWith('.ps.hlsl')) {
        $Profile = 'ps'
    }
    elseif ($shader.Name.EndsWith('.vs.hlsl')) {
        $Profile = 'vs'
    }
    elseif ($shader.Name.EndsWith('.cs.hlsl')) {
        $Profile = 'cs'
    }
    elseif ($shader.Name.EndsWith('.as.hlsl')) {
        $Profile = 'as'
    }
    elseif ($shader.Name.EndsWith('.ms.hlsl')) {
        $Profile = 'ms'
    }

    if ($Profile) {
        $outputPath = Join-Path $outputDirectory "$($shader.BaseName).cso"
        Write-Host $shader.FullName
        & dxc /nologo /T "${Profile}_${Version}" /Fo $outputPath /Zi /Qembed_debug $shader.FullName
    }
}
