param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot "build")
)

$ErrorActionPreference = "Stop"
$shaderDirectory = Join-Path $PSScriptRoot "Shaders"
$outputDirectory = Join-Path $BuildDirectory "Shaders"
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

foreach ($shader in Get-ChildItem -LiteralPath $shaderDirectory -Filter *.hlsl) {
    foreach ($stage in @(
        @{ Suffix = "vs"; Profile = "vs_6_6"; Entry = "VSMain" },
        @{ Suffix = "ps"; Profile = "ps_6_6"; Entry = "PSMain" }
    )) {
        $outputPath = Join-Path $outputDirectory ($shader.BaseName + "." + $stage.Suffix + ".cso")
        & dxc /nologo /T $stage.Profile /E $stage.Entry /Fo $outputPath /Zi /Qembed_debug $shader.FullName
        if ($LASTEXITCODE -ne 0) {
            throw "Shader compilation failed: $($shader.Name) ($($stage.Entry))"
        }
    }
}
