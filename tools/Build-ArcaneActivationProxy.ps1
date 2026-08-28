param(
    [string]$LoreRimRoot = "D:\Lorerim",
    [string]$XEdit = "C:\Users\Theo\Documents\DiageticFastTravel\build\xedit-patched\SSEEdit64.exe"
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot "build"
$generationRoot = Join-Path $buildRoot "arcane-proxy-generation"
$stagingData = Join-Path $generationRoot "data"
$pluginsList = Join-Path $generationRoot "plugins.txt"
$xeditLog = Join-Path $generationRoot "xedit.log"
$generatorScript = Join-Path $PSScriptRoot "xedit\AA_GenerateProxySpell.pas"
$validator = Join-Path $PSScriptRoot "validate_arcane_proxy.py"
$packagePlugin = Join-Path $projectRoot "package\ArcaneActivation.esp"

foreach ($required in @($XEdit, $generatorScript, $validator)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required build input not found: $required"
    }
}

$resolvedBuild = [IO.Path]::GetFullPath($buildRoot)
$resolvedGeneration = [IO.Path]::GetFullPath($generationRoot)
if (-not $resolvedGeneration.StartsWith($resolvedBuild, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to clean a path outside the build directory: $resolvedGeneration"
}
if (Test-Path -LiteralPath $generationRoot) {
    Remove-Item -LiteralPath $generationRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stagingData | Out-Null

$inputs = [ordered]@{
    "Skyrim.esm" = "Stock Game\Data\Skyrim.esm"
    "Update.esm" = "mods\Official Master Files - Cleaned Plugins\Update.esm"
    "Dawnguard.esm" = "mods\Official Master Files - Cleaned Plugins\Dawnguard.esm"
    "HearthFires.esm" = "mods\Official Master Files - Cleaned Plugins\HearthFires.esm"
    "Dragonborn.esm" = "mods\Official Master Files - Cleaned Plugins\Dragonborn.esm"
    "ccBGSSSE001-Fish.esm" = "mods\Official Master Files - Cleaned Plugins\ccBGSSSE001-Fish.esm"
    "ccQDRSSE001-SurvivalMode.esl" = "mods\Official Master Files - Cleaned Plugins\ccQDRSSE001-SurvivalMode.esl"
    "ccBGSSSE037-Curios.esl" = "mods\Official Master Files - Cleaned Plugins\ccBGSSSE037-Curios.esl"
    "ccBGSSSE025-AdvDSGS.esm" = "mods\Official Master Files - Cleaned Plugins\ccBGSSSE025-AdvDSGS.esm"
    "Unofficial Skyrim Special Edition Patch.esp" = "mods\Unofficial Skyrim Special Edition Patch\Unofficial Skyrim Special Edition Patch.esp"
    "Requiem.esp" = "mods\Requiem - The Roleplaying Overhaul (No Messages ESLIFIED)\Requiem.esp"
    "Requiem - Magic Redone.esp" = "mods\Requiem - Magic Redone\Requiem - Magic Redone.esp"
    "SpellHotbar.esp" = "mods\SpellHotbar2\SpellHotbar.esp"
    "SkyUI_SE.esp" = "mods\SkyUI\SkyUI_SE.esp"
    "MCMHelper.esp" = "mods\MCM Helper\MCMHelper.esp"
}

foreach ($entry in $inputs.GetEnumerator()) {
    $source = Join-Path $LoreRimRoot $entry.Value
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "LoreRim build input not found: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $stagingData $entry.Key) -Force
}

$pluginLines = foreach ($name in $inputs.Keys) {
    "*$name"
}
[IO.File]::WriteAllText(
    $pluginsList,
    (($pluginLines -join "`r`n") + "`r`n"),
    [Text.UTF8Encoding]::new($false)
)

$successPath = Join-Path $stagingData "aa_proxy_generator.success"
$failurePath = Join-Path $stagingData "aa_proxy_generator.failed"
$errorPath = Join-Path $stagingData "aa_proxy_generator.error"
$arguments = @(
    "-sse",
    "-D:`"$stagingData`"",
    "-P:`"$pluginsList`"",
    "-R:`"$xeditLog`"",
    "-IKnowWhatImDoing",
    "-nobuildrefs",
    "-autoload",
    "-autoexit",
    "-PseudoESL",
    "-script:`"$generatorScript`""
)

Write-Host "Generating Arcane Activation proxy plugin with xEdit"
$process = Start-Process -FilePath $XEdit -ArgumentList $arguments -WindowStyle Hidden -PassThru
$deadline = [DateTime]::UtcNow.AddMinutes(5)
$terminalStatus = $null
while (-not $process.HasExited -and [DateTime]::UtcNow -lt $deadline) {
    if (Test-Path -LiteralPath $successPath -PathType Leaf) {
        $terminalStatus = "success"
        break
    }
    if (Test-Path -LiteralPath $failurePath -PathType Leaf) {
        $terminalStatus = "failed"
        break
    }
    Start-Sleep -Milliseconds 200
    $process.Refresh()
}
if ($terminalStatus -in @("success", "failed") -and -not $process.HasExited) {
    $null = $process.WaitForExit(15000)
    $process.Refresh()
}
if (-not $process.HasExited) {
    Stop-Process -Id $process.Id -Force
    if ($terminalStatus -eq "failed") {
        $detail = if (Test-Path -LiteralPath $errorPath -PathType Leaf) {
            (Get-Content -LiteralPath $errorPath -Raw).Trim()
        } else {
            "See $xeditLog"
        }
        throw "xEdit generation failed: $detail"
    }
    throw "xEdit timed out while generating the proxy plugin"
}
if ($terminalStatus -ne "success" -or $process.ExitCode -ne 0) {
    $detail = if (Test-Path -LiteralPath $errorPath -PathType Leaf) {
        (Get-Content -LiteralPath $errorPath -Raw).Trim()
    } else {
        "See $xeditLog"
    }
    throw "xEdit generation failed: $detail"
}
if (Select-String -LiteralPath $xeditLog -Pattern "Exception in unit|Aborted: Applying script|Error assigning to" -Quiet) {
    throw "xEdit reported a script failure. See $xeditLog"
}

$generatedPlugin = Join-Path $stagingData "ArcaneActivation.esp"
if (-not (Test-Path -LiteralPath $generatedPlugin -PathType Leaf)) {
    throw "xEdit did not produce ArcaneActivation.esp"
}

python $validator $generatedPlugin
if ($LASTEXITCODE -ne 0) {
    throw "Generated plugin validation failed"
}
Copy-Item -LiteralPath $generatedPlugin -Destination $packagePlugin -Force
Write-Host "Built: $packagePlugin"
Write-Host "ESP SHA-256: $((Get-FileHash -LiteralPath $packagePlugin -Algorithm SHA256).Hash)"
