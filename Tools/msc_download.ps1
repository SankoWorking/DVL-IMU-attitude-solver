param(
    [string]$HexPath = "MDK-ARM\H743\H743.hex",
    [string]$ComPort = "auto",
    [int]$BaudRate = 115200,
    [int]$WaitSeconds = 20,
    [int]$InstallWaitSeconds = 20,
    [int]$AppWaitSeconds = 30,
    [string]$VolumeLabel = "H7BOOT",
    [switch]$NoDetach,
    [switch]$NoPause,
    [switch]$ListOnly
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot
$LogPath = Join-Path $RepoRoot "MDK-ARM\H743\msc_download.log"
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

    Write-Log "[MSC] ERROR: $Message"
    exit $ExitCode
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

function Send-BootMscCommand {
    param(
        [string]$PortName,
        [int]$Rate
    )

    Write-Log "[MSC] Sending BOOTMSC over $PortName ..."
    $serial = New-Object System.IO.Ports.SerialPort($PortName, $Rate, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $serial.NewLine = "`r`n"
    $serial.ReadTimeout = 500
    $serial.WriteTimeout = 1000
    try {
        $serial.Open()
        $serial.DtrEnable = $true
        $serial.RtsEnable = $true
        Start-Sleep -Milliseconds 300
        foreach ($command in @("?", "BOOTMSC", "UPDATE", "BOOTMSC")) {
            Write-Log "[MSC] TX ${PortName}: $command"
            try {
                $serial.WriteLine($command)
            } catch {
                Write-Log "[MSC] $PortName closed while sending; device is probably resetting."
                break
            }
            Start-Sleep -Milliseconds 250
        }
    }
    finally {
        if ($serial.IsOpen) {
            try{
                $serial.Close()
            } catch{
            
            }
        }
        $serial.Dispose()
    }
}

function Test-MotorResponse {
    param(
        [string]$PortName,
        [int]$Rate
    )

    $serial = New-Object System.IO.Ports.SerialPort($PortName, $Rate, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $serial.NewLine = "`r`n"
    $serial.ReadTimeout = 300
    $serial.WriteTimeout = 1000
    $response = ""

    try {
        $serial.Open()
        $serial.DtrEnable = $true
        $serial.RtsEnable = $true
        Start-Sleep -Milliseconds 250
        $response += $serial.ReadExisting()
        $serial.WriteLine("?")

        $deadline = (Get-Date).AddSeconds(2)
        do {
            Start-Sleep -Milliseconds 100
            $response += $serial.ReadExisting()
            if ($response -match "\[MOTOR\]") {
                break
            }
        } while ((Get-Date) -lt $deadline)

        return [PSCustomObject]@{
            Ready = ($response -match "\[MOTOR\]")
            Error = ""
        }
    } catch {
        return [PSCustomObject]@{
            Ready = $false
            Error = $_.Exception.Message
        }
    } finally {
        if ($serial.IsOpen) {
            $serial.Close()
        }
        $serial.Dispose()
    }
}

function Get-H7BootDrive {
    param([string]$Label)

    $volumes = @(Get-CimInstance Win32_Volume |
        Where-Object {
            $_.DriveLetter -and
            $_.Label -eq $Label
        })

    if ($volumes.Count -eq 0) {
        return $null
    }

    return $volumes[0].DriveLetter
}

$ResolvedHex = Join-Path $RepoRoot $HexPath
if (-not (Test-Path -LiteralPath $ResolvedHex)) {
    throw "Hex file not found: $ResolvedHex. Build the Keil target first."
}
$ResolvedHex = (Resolve-Path -LiteralPath $ResolvedHex).Path

Set-Content -LiteralPath $LogPath -Value ("[{0}] MSC download started" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss")) -Encoding ASCII
Write-Log "[MSC] Hex: $ResolvedHex"

$cdcPorts = @(Get-CdcComPorts)
$selectedPort = $null
if ($ListOnly) {
    if ($cdcPorts.Count -eq 0) {
        Write-Log "[MSC] CDC COM ports: none"
    } else {
        $cdcPorts | Format-Table -AutoSize | Out-String | Add-Content -LiteralPath $LogPath -Encoding ASCII
        $cdcPorts | Format-Table -AutoSize
    }
    $drive = Get-H7BootDrive -Label $VolumeLabel
    Write-Log "[MSC] $VolumeLabel drive: $drive"
    exit 0
}

$drive = Get-H7BootDrive -Label $VolumeLabel
if ((-not $drive) -and (-not $NoDetach)) {
    if ($ComPort -and ($ComPort -ne "auto")) {
        $selectedPort = $ComPort
    } elseif ($cdcPorts.Count -eq 1) {
        $selectedPort = $cdcPorts[0].Port
    } elseif ($cdcPorts.Count -gt 1) {
        $ports = ($cdcPorts | ForEach-Object { $_.Port }) -join ", "
        Stop-WithMessage -Message "Multiple H7 Luxiaoban CDC ports found: $ports. Re-run with -ComPort COMx." -ExitCode 2
    }

    if ($selectedPort) {
        Send-BootMscCommand -PortName $selectedPort -Rate $BaudRate
    } else {
        Write-Log "[MSC] H7 Luxiaoban CDC COM port not found."
    }
}

$deadline = (Get-Date).AddSeconds($WaitSeconds)
Write-Log "[MSC] Waiting up to ${WaitSeconds}s for $VolumeLabel USB disk ..."
do {
    $drive = Get-H7BootDrive -Label $VolumeLabel
    if ($drive) {
        break
    }
    Start-Sleep -Milliseconds 500
} while ((Get-Date) -lt $deadline)

if (-not $drive) {
    Stop-WithMessage -Message "$VolumeLabel USB disk not detected. Burn Bootloader first, then from the app CDC send BOOTMSC/UPDATE." -ExitCode 2
}

$destination = Join-Path ($drive + "\") "H743.hex"
Write-Log "[MSC] Copying to $destination ..."
Copy-Item -LiteralPath $ResolvedHex -Destination $destination -Force

$deadline = (Get-Date).AddSeconds($InstallWaitSeconds)
Write-Log "[MSC] Waiting up to ${InstallWaitSeconds}s for the bootloader to install and disconnect ..."
do {
    Start-Sleep -Milliseconds 500
    $activeDrive = Get-H7BootDrive -Label $VolumeLabel
    if (-not $activeDrive) {
        break
    }
} while ((Get-Date) -lt $deadline)

if ($activeDrive) {
    Stop-WithMessage -Message "$VolumeLabel remained mounted after the copy. The bootloader did not start the install." -ExitCode 3
}

Write-Log "[MSC] $VolumeLabel disconnected; waiting up to ${AppWaitSeconds}s for Motor startup ..."
$deadline = (Get-Date).AddSeconds($AppWaitSeconds)
$lastError = ""
$verifiedPort = $null

do {
    $motorPorts = @(Get-CdcComPorts)
    if ($ComPort -and ($ComPort -ne "auto")) {
        $motorPorts = @($motorPorts | Where-Object { $_.Port -eq $ComPort })
    }

    if ($motorPorts.Count -gt 1) {
        $ports = ($motorPorts | ForEach-Object { $_.Port }) -join ", "
        Stop-WithMessage -Message "Multiple Motor CDC ports found after install: $ports. Re-run with -ComPort COMx." -ExitCode 4
    }

    if ($motorPorts.Count -eq 1) {
        $test = Test-MotorResponse -PortName $motorPorts[0].Port -Rate $BaudRate
        if ($test.Ready) {
            $verifiedPort = $motorPorts[0].Port
            break
        }
        if ($test.Error) {
            $lastError = $test.Error
        }
    }

    Start-Sleep -Milliseconds 500
} while ((Get-Date) -lt $deadline)

if (-not $verifiedPort) {
    if ($lastError -match "denied|拒绝") {
        Stop-WithMessage -Message "Motor CDC is busy after install. Close the serial terminal and retry. Last error: $lastError" -ExitCode 4
    }
    Stop-WithMessage -Message "Motor did not respond after the H7BOOT install. Last error: $lastError" -ExitCode 4
}

Write-Log "[MSC] SUCCESS: Motor download verified on $verifiedPort."
