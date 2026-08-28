param(
    [string]$LoreRimRoot = "D:\Lorerim",
    [string]$ScriptsArchive = "C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data\Scripts.zip"
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$compilerRoot = Join-Path $LoreRimRoot "mods\Project New Reign - Nemesis Unlimited Behavior Engine\Nemesis_Engine\Papyrus Compiler"
$compiler = Join-Path $compilerRoot "PapyrusCompiler.exe"
$flags = Join-Path $compilerRoot "scripts\TESV_Papyrus_Flags.flg"
$sdkRoot = Join-Path $projectRoot "build\papyrus_sdk"
$vanillaSources = Join-Path $sdkRoot "Source\Scripts"
$projectSources = Join-Path $projectRoot "package\Source\Scripts"
$mcmSources = Join-Path $projectRoot "external\mcm_helper_1_6_2\scripts"
$output = Join-Path $projectRoot "package\Scripts"
$config = Join-Path $projectRoot "package\MCM\Config\ArcaneActivation\config.json"

foreach ($required in @($compiler, $flags, $ScriptsArchive, $config)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required MCM build input not found: $required"
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $vanillaSources "Quest.psc") -PathType Leaf)) {
    New-Item -ItemType Directory -Path $sdkRoot -Force | Out-Null
    Expand-Archive -LiteralPath $ScriptsArchive -DestinationPath $sdkRoot -Force
}

New-Item -ItemType Directory -Path $output -Force | Out-Null
$imports = "$projectSources;$mcmSources;$vanillaSources"
foreach ($script in @("ArcaneActivationNative.psc", "ArcaneActivationMCM.psc")) {
    $source = Join-Path $projectSources $script
    & $compiler $source "-f=$flags" "-i=$imports" "-o=$output" -op
    if ($LASTEXITCODE -ne 0) {
        throw "Papyrus compilation failed for $script with exit code $LASTEXITCODE"
    }
}

$parsedConfig = Get-Content -LiteralPath $config -Raw | ConvertFrom-Json
$topLevelContentCount = @($parsedConfig.content).Count
$pagesProperty = $parsedConfig.PSObject.Properties['pages']
if ($topLevelContentCount -gt 0 -and $null -ne $pagesProperty -and @($parsedConfig.pages).Count -eq 0) {
    throw 'A one-page MCM with top-level content must omit pages instead of declaring an empty array'
}
foreach ($compiled in @("ArcaneActivationNative.pex", "ArcaneActivationMCM.pex")) {
    $path = Join-Path $output $compiled
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Papyrus compiler did not produce $path"
    }
}

Write-Host "Built Arcane Activation MCM scripts"
