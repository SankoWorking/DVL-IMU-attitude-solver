param(
    [ValidateSet("build", "rebuild", "clean")]
    [string]$Action = "build",

    [string]$KeilExe = "",
    [string]$Project = "MDK-ARM\H743.uvprojx",
    [string]$Log = "MDK-ARM\H743\H743.build_log.htm"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

if (-not $KeilExe) {
    $Command = Get-Command UV4.exe -ErrorAction SilentlyContinue
    if ($Command -and (Test-Path -LiteralPath $Command.Source)) {
        $KeilExe = $Command.Source
    } elseif (Test-Path -LiteralPath "D:\keil\UV4\UV4.exe") {
        $KeilExe = "D:\keil\UV4\UV4.exe"
    } elseif (Test-Path -LiteralPath "D:\Keil5\UV4\UV4.exe") {
        $KeilExe = "D:\Keil5\UV4\UV4.exe"
    } elseif (Test-Path -LiteralPath "C:\Keil_v5\UV4\UV4.exe") {
        $KeilExe = "C:\Keil_v5\UV4\UV4.exe"
    }
}

if (-not $KeilExe -or -not (Test-Path -LiteralPath $KeilExe)) {
    throw "Keil UV4.exe not found. Pass -KeilExe <path>."
}

$ProjectPath = Resolve-Path -LiteralPath $Project
$ProjectDir = Split-Path -Parent $ProjectPath.Path
$LogPath = Join-Path $RepoRoot $Log
$LogDir = Split-Path -Parent $LogPath
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$ActionFlag = switch ($Action) {
    "build" { "-b" }
    "rebuild" { "-r" }
    "clean" { "-c" }
}

if (Test-Path -LiteralPath $LogPath) {
    Remove-Item -LiteralPath $LogPath -Force
}

Write-Host "[Keil] Action : $Action"
Write-Host "[Keil] Project: $($ProjectPath.Path)"
Write-Host "[Keil] Log    : $LogPath"

$ArgsList = @($ActionFlag, $ProjectPath.Path, "-j0", "-o", $LogPath)
$Process = Start-Process -FilePath $KeilExe -ArgumentList $ArgsList -PassThru -WindowStyle Hidden
Wait-Process -Id $Process.Id

$Deadline = (Get-Date).AddSeconds(20)
do {
    if ((Test-Path -LiteralPath $LogPath) -and
        (Select-String -LiteralPath $LogPath -Pattern "Error\(s\),\s+\d+\s+Warning\(s\)" -Quiet)) {
        break
    }
    Start-Sleep -Milliseconds 500
} while ((Get-Date) -lt $Deadline)

if (-not (Test-Path -LiteralPath $LogPath)) {
    throw "Keil build log was not generated: $LogPath"
}

$LogLines = Get-Content -LiteralPath $LogPath -Encoding Default
$SizeLine = $LogLines | Where-Object { $_ -match "Program Size" } | Select-Object -Last 1
$SummaryLine = $LogLines | Where-Object { $_ -match "(\d+)\s+Error\(s\),\s+(\d+)\s+Warning\(s\)" } | Select-Object -Last 1

if ($SizeLine) {
    Write-Host $SizeLine.Trim()
}

if (-not $SummaryLine) {
    Write-Host "[Keil] Build summary was not found. Open the log for details:"
    Write-Host "[Keil] $LogPath"
    exit 2
}

Write-Host $SummaryLine.Trim()
$null = $SummaryLine -match "(\d+)\s+Error\(s\),\s+(\d+)\s+Warning\(s\)"
$ErrorCount = [int]$Matches[1]
$WarningCount = [int]$Matches[2]

if ($ErrorCount -gt 0) {
    Write-Host "[Keil] Last error lines:"
    $LogLines |
        Where-Object { $_ -match "error:|Fatal error|Error:" } |
        Select-Object -Last 20 |
        ForEach-Object { Write-Host $_.Trim() }
    exit $ErrorCount
}

$HexCandidates = @()
try {
    [xml]$ProjectXml = Get-Content -LiteralPath $ProjectPath.Path -Encoding UTF8
    $Target = @($ProjectXml.Project.Targets.Target)[0]
    $OutputDirectory = [string]$Target.TargetOption.TargetCommonOption.OutputDirectory
    $OutputName = [string]$Target.TargetOption.TargetCommonOption.OutputName
    if (-not $OutputName) {
        $OutputName = [System.IO.Path]::GetFileNameWithoutExtension($ProjectPath.Path)
    }
    if ($OutputDirectory) {
        $HexCandidates += (Join-Path (Join-Path $ProjectDir $OutputDirectory) ($OutputName + ".hex"))
    }
    $HexCandidates += (Join-Path $ProjectDir ($OutputName + ".hex"))
} catch {
}

$Hex = $null
foreach ($Candidate in $HexCandidates) {
    if ($Candidate -and (Test-Path -LiteralPath $Candidate)) {
        $Hex = Get-Item -LiteralPath $Candidate
        break
    }
}

if ($Hex) {
    Write-Host ("[Keil] Hex    : {0} ({1} bytes, {2})" -f $Hex.FullName, $Hex.Length, $Hex.LastWriteTime)
}

if ($WarningCount -gt 0) {
    Write-Host "[Keil] Build passed with warnings."
} else {
    Write-Host "[Keil] Build passed."
}

exit 0
