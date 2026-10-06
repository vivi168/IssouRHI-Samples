param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot "build")
)

$shaderDirectory = Join-Path $PSScriptRoot "Shaders"
$outputDirectory = Join-Path $BuildDirectory "Shaders"
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

foreach ($shader in Get-ChildItem -LiteralPath $shaderDirectory -Filter *.hlsl) {
    $stages = @(
        @{ Suffix = "vs"; Profile = "vs_6_6"; Entry = "VSMain" },
        @{ Suffix = "ps"; Profile = "ps_6_6"; Entry = "PSMain" }
    )
    if ($shader.Name.EndsWith('.ps.hlsl')) {
        $stages = @(@{ Suffix = ''; Profile = 'ps_6_6'; Entry = 'PSMain' })
    }
    if ($shader.Name.EndsWith('.vs.hlsl')) {
        $stages = @(@{ Suffix = ''; Profile = 'vs_6_6'; Entry = 'VSMain' })
    }
    if ($shader.Name.EndsWith('.cs.hlsl')) {
        $stages = @(@{ Suffix = ''; Profile = 'cs_6_6'; Entry = 'CSMain' })
    }
    elseif ($shader.Name.EndsWith('.as.hlsl')) {
        $stages = @(@{ Suffix = ''; Profile = 'as_6_6'; Entry = 'ASMain' })
    }
    elseif ($shader.Name.EndsWith('.ms.hlsl')) {
        $stages = @(@{ Suffix = ''; Profile = 'ms_6_6'; Entry = 'MSMain' })
    }
    foreach ($stage in $stages) {
        $outputPath = Join-Path $outputDirectory ($shader.BaseName + $(if ($stage.Suffix) { "." + $stage.Suffix }) + ".cso")
        & dxc /nologo /T $stage.Profile /E $stage.Entry /Fo $outputPath /Zi /Qembed_debug $shader.FullName
    }
}
