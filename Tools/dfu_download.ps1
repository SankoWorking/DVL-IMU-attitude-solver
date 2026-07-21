param(
    [string]$HexPath = "MDK-ARM\H743\H743.hex",
    [string]$ComPort = "auto",
    [string]$ProgrammerCli = "",
    [int]$WaitSeconds = 15,
    [int]$ToolTimeoutSeconds = 20,
    [int]$BaudRate = 115200,
    [string]$GoAddress = "0x08000000",
    [switch]$SkipSerialDetach,
    [switch]$NoGo,
    [switch]$NoPause,
    [switch]$ListOnly
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot
$LogPath = Join-Path $RepoRoot "MDK-ARM\H743\dfu_download.log"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $LogPath) | Out-Null

function Write-Log {
    param([string]$Message)
    $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $Message
    Write-Host $Message
    Add-Content -LiteralPath $LogPath -Value $line -Encoding ASCII
}

function Stop-WithMessage {
    param(
        [string]$Message,
        [int]$ExitCode
    )

    Write-Log "[DFU] ERROR: $Message"
    exit $ExitCode
}

function Join-ProcessArguments {
    param([string[]]$Arguments)

    $quoted = foreach ($arg in $Arguments) {
        if ($arg -match '[\s"]') {
            '"' + ($arg -replace '"', '\"') + '"'
        } else {
            $arg
        }
    }

    return ($quoted -join ' ')
}

function Invoke-ExternalTool {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [int]$TimeoutSeconds,
        [switch]$AllowFailure
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.Arguments = Join-ProcessArguments -Arguments $Arguments
    $psi.WorkingDirectory = $RepoRoot
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()

    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        try {
            $process.Kill()
        } catch {
        }
        throw "Timed out after ${TimeoutSeconds}s: $FilePath $($Arguments -join ' ')"
    }

    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    if ($stdout) {
        Add-Content -LiteralPath $LogPath -Value $stdout -Encoding ASCII
    }
    if ($stderr) {
        Add-Content -LiteralPath $LogPath -Value $stderr -Encoding ASCII
    }

    if (($process.ExitCode -ne 0) -and -not $AllowFailure) {
        throw "Command failed with exit code $($process.ExitCode): $FilePath $($Arguments -join ' ')"
    }

    [PSCustomObject]@{
        ExitCode = $process.ExitCode
        Output = ($stdout + $stderr)
    }
}

function Resolve-ProgrammerCli {
    param([string]$ConfiguredPath)

    $candidates = @()
    if ($ConfiguredPath) {
        $candidates += $ConfiguredPath
    }

    $cmd = Get-Command STM32_Programmer_CLI.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        $candidates += $cmd.Source
    }

    $candidates += @(
        "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        "C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        "D:\bin\STM32_Programmer_CLI.exe"
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "STM32_Programmer_CLI.exe not found. Install STM32CubeProgrammer or pass -ProgrammerCli <path>."
}

function Invoke-ProgrammerListUsb {
    param([string]$CliPath)
    $result = Invoke-ExternalTool -FilePath $CliPath -Arguments @("-l", "usb") -TimeoutSeconds 8 -AllowFailure
    return $result.Output
}

function Test-DfuPresent {
    param([string]$CliPath)
    $output = Invoke-ProgrammerListUsb -CliPath $CliPath
    return (($output -match "DFU Interface") -and ($output -notmatch "No STM32 device"))
}

function Get-CdcComPorts {
    $devices = Get-CimInstance Win32_PnPEntity |
        Where-Object {
            ($_.Name -match "\(COM\d+\)") -and
            ($_.PNPDeviceID -match "VID_0483&PID_5740")
        }

    foreach ($device in $devices) {
        if ($device.Name -match "\((COM\d+)\)") {
            [PSCustomObject]@{
                Port = $Matches[1]
                Name = $device.Name
                PnpId = $device.PNPDeviceID
            }
        }
    }
}

function Send-DfuCommand {
    param(
        [string]$PortName,
        [int]$Rate
    )

    Write-Log "[DFU] Sending BOOTDFU over $PortName ..."
    $serial = New-Object System.IO.Ports.SerialPort($PortName, $Rate, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $serial.NewLine = "`r`n"
    $serial.ReadTimeout = 500
    $serial.WriteTimeout = 1000
    $serial.DtrEnable = $true
    $serial.RtsEnable = $true
    try {
        $serial.Open()
        Start-Sleep -Milliseconds 300
        $serial.DiscardInBuffer()
        $serial.DiscardOutBuffer()
        $commands = @("?", "BOOTDFU", "DFU", "BOOTDFU")
        foreach ($command in $commands) {
            Write-Log "[DFU] TX ${PortName}: $command"
            $serial.WriteLine($command)
            Start-Sleep -Milliseconds 350
        }
    }
    finally {
        if ($serial.IsOpen) {
            $serial.Close()
        }
        $serial.Dispose()
    }
}

$ProgrammerCli = Resolve-ProgrammerCli -ConfiguredPath $ProgrammerCli
$ResolvedHex = Join-Path $RepoRoot $HexPath
if (-not (Test-Path -LiteralPath $ResolvedHex)) {
    throw "Hex file not found: $ResolvedHex. Build the Keil target first."
}
$ResolvedHex = (Resolve-Path -LiteralPath $ResolvedHex).Path

Set-Content -LiteralPath $LogPath -Value ("[{0}] DFU download started" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss")) -Encoding ASCII
Write-Log "[DFU] STM32CubeProgrammer: $ProgrammerCli"
Write-Log "[DFU] Hex: $ResolvedHex"

$dfuOutput = Invoke-ProgrammerListUsb -CliPath $ProgrammerCli
$dfuReady = (($dfuOutput -match "DFU Interface") -and ($dfuOutput -notmatch "No STM32 device"))
$cdcPorts = @(Get-CdcComPorts)
$detachPort = $null

if ($ListOnly) {
    Write-Log "[DFU] ROM DFU present: $dfuReady"
    if ($cdcPorts.Count -eq 0) {
        Write-Log "[DFU] CDC COM ports: none"
    } else {
        $cdcPorts | Format-Table -AutoSize | Out-String | Add-Content -LiteralPath $LogPath -Encoding ASCII
        $cdcPorts | Format-Table -AutoSize
    }
    exit 0
}

if (-not $dfuReady -and -not $SkipSerialDetach) {
    $selectedPort = $null
    if ($ComPort -and ($ComPort -ne "auto")) {
        $selectedPort = $ComPort
    } elseif ($cdcPorts.Count -eq 1) {
        $selectedPort = $cdcPorts[0].Port
    } elseif ($cdcPorts.Count -gt 1) {
        $ports = ($cdcPorts | ForEach-Object { $_.Port }) -join ", "
        Stop-WithMessage -Message "Multiple H7 Luxiaoban CDC ports found: $ports. Re-run with -ComPort COMx." -ExitCode 2
    }

    if ($selectedPort) {
        $detachPort = $selectedPort
        Send-DfuCommand -PortName $selectedPort -Rate $BaudRate
    } else {
        Write-Log "[DFU] H7 Luxiaoban CDC COM port not found. If this is the first programming, enter ROM DFU with BOOT0=1 and reset."
    }
}

if (-not $dfuReady -and -not $detachPort -and -not $SkipSerialDetach) {
    Write-Log "[DFU] No DFU device and no CDC command port available."
    Stop-WithMessage -Message "No STM32 DFU/CDC device detected. For first programming, set BOOT0=1, reset the board, then run Download again. Log: $LogPath" -ExitCode 2
}

$deadline = (Get-Date).AddSeconds($WaitSeconds)
$earlyDetachCheckTime = (Get-Date).AddSeconds(5)
$earlyDetachChecked = $false
Write-Log "[DFU] Waiting up to ${WaitSeconds}s for STM32 ROM DFU ..."
do {
    if (Test-DfuPresent -CliPath $ProgrammerCli) {
        $dfuReady = $true
        break
    }
    if ($detachPort -and -not $earlyDetachChecked -and ((Get-Date) -ge $earlyDetachCheckTime)) {
        $earlyDetachChecked = $true
        $portsAfterDetach = @(Get-CdcComPorts | Where-Object { $_.Port -eq $detachPort })
        if ($portsAfterDetach.Count -ne 0) {
            Write-Log "[DFU] $detachPort is still present after BOOTDFU, stopping early."
            break
        }
    }
    Start-Sleep -Milliseconds 500
} while ((Get-Date) -lt $deadline)

if (-not $dfuReady) {
    if ($detachPort) {
        $portsAfterDetach = @(Get-CdcComPorts | Where-Object { $_.Port -eq $detachPort })
        if ($portsAfterDetach.Count -eq 0) {
            Write-Log "[DFU] $detachPort disappeared, but ROM DFU did not enumerate."
        } else {
            Write-Log "[DFU] $detachPort is still present, so the firmware did not accept the DFU command."
        }
    }
    Write-Log "[DFU] No STM32 ROM DFU device detected."
    Stop-WithMessage -Message "No STM32 ROM DFU device detected. Put the board in DFU mode: either run existing firmware and expose CDC, or set BOOT0=1 then reset. Log: $LogPath" -ExitCode 2
}

Write-Log "[DFU] Downloading over USB DFU ..."
$args = @("-c", "port=usb1", "-d", $ResolvedHex, "-v")
if (-not $NoGo) {
    $args += @("-g", $GoAddress)
}

try {
    $download = Invoke-ExternalTool -FilePath $ProgrammerCli -Arguments $args -TimeoutSeconds $ToolTimeoutSeconds
    if ($download.Output) {
        Write-Host $download.Output
    }
} catch {
    Write-Log "[DFU] Download failed: $($_.Exception.Message)"
    Stop-WithMessage -Message "$($_.Exception.Message). Log: $LogPath" -ExitCode 3
}

Write-Log "[DFU] Done."
