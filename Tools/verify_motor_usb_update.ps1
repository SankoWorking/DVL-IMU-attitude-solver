param(
    [string]$HexPath = "MDK-ARM\H743\H743.hex",
    [string]$ComPort = "auto",
    [string]$ProgrammerCli = "",
    [int]$BaudRate = 115200,
    [int]$MscWaitSeconds = 30,
    [int]$AppWaitSeconds = 45,
    [int]$DfuWaitSeconds = 20,
    [int]$ToolTimeoutSeconds = 30,
    [switch]$PreflightOnly
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot
$LogPath = Join-Path $RepoRoot "MDK-ARM\H743\motor_update_verify.log"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $LogPath) | Out-Null

function Write-Log {
    param([string]$Message)

    $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $Message
    Write-Host $Message
    Add-Content -LiteralPath $LogPath -Value $line -Encoding ASCII
}

function Resolve-ExistingPath {
    param([string]$Path)

    $candidate = $Path
    if (-not [System.IO.Path]::IsPathRooted($candidate)) {
        $candidate = Join-Path $RepoRoot $candidate
    }
    if (-not (Test-Path -LiteralPath $candidate)) {
        throw "File not found: $candidate"
    }
    return (Resolve-Path -LiteralPath $candidate).Path
}

function Get-RepoRelativePath {
    param([string]$ResolvedPath)

    $prefix = $RepoRoot.TrimEnd("\") + "\"
    if (-not $ResolvedPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Hex must be inside the repository so the existing download scripts can use it: $ResolvedPath"
    }
    return $ResolvedPath.Substring($prefix.Length)
}

function Get-IntelHexRange {
    param([string]$Path)

    [int64]$upper = 0
    [int64]$minAddress = [int64]::MaxValue
    [int64]$maxAddress = 0

    foreach ($line in Get-Content -LiteralPath $Path) {
        if (-not $line.StartsWith(":")) {
            continue
        }

        $length = [Convert]::ToInt32($line.Substring(1, 2), 16)
        $address = [Convert]::ToInt32($line.Substring(3, 4), 16)
        $recordType = [Convert]::ToInt32($line.Substring(7, 2), 16)

        if ($recordType -eq 4) {
            $upper = [Convert]::ToInt64($line.Substring(9, 4), 16) -shl 16
        } elseif (($recordType -eq 0) -and ($length -gt 0)) {
            $start = $upper + $address
            $end = $start + $length - 1
            if ($start -lt $minAddress) {
                $minAddress = $start
            }
            if ($end -gt $maxAddress) {
                $maxAddress = $end
            }
        }
    }

    if ($minAddress -eq [int64]::MaxValue) {
        throw "No data records found in hex: $Path"
    }

    return [PSCustomObject]@{
        Min = $minAddress
        Max = $maxAddress
    }
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

function Get-SelectedCdcPort {
    param([string]$RequestedPort)

    $ports = @(Get-CdcComPorts)
    if ($RequestedPort -and ($RequestedPort -ne "auto")) {
        return ($ports | Where-Object { $_.Port -eq $RequestedPort } | Select-Object -First 1)
    }
    if ($ports.Count -eq 1) {
        return $ports[0]
    }
    if ($ports.Count -gt 1) {
        $names = ($ports | ForEach-Object { $_.Port }) -join ", "
        throw "Multiple H7 CDC ports found: $names. Re-run with -ComPort COMx."
    }
    return $null
}

function Get-H7BootDrive {
    $volume = Get-CimInstance Win32_Volume |
        Where-Object { $_.DriveLetter -and ($_.Label -eq "H7BOOT") } |
        Select-Object -First 1

    if ($null -eq $volume) {
        return $null
    }
    return $volume.DriveLetter
}

function Test-MotorPort {
    param(
        [string]$PortName,
        [int]$Rate
    )

    $serial = New-Object System.IO.Ports.SerialPort(
        $PortName,
        $Rate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One)
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
            Response = $response
            Error = ""
        }
    } catch {
        return [PSCustomObject]@{
            Ready = $false
            Response = $response
            Error = $_.Exception.Message
        }
    } finally {
        if ($serial.IsOpen) {
            $serial.Close()
        }
        $serial.Dispose()
    }
}

function Wait-MotorReady {
    param(
        [string]$RequestedPort,
        [int]$Rate,
        [int]$TimeoutSeconds,
        [string]$Stage
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $lastError = ""
    Write-Log "[VERIFY] Waiting up to ${TimeoutSeconds}s for Motor CDC after $Stage ..."

    do {
        if (-not (Get-H7BootDrive)) {
            $port = Get-SelectedCdcPort -RequestedPort $RequestedPort
            if ($null -ne $port) {
                $test = Test-MotorPort -PortName $port.Port -Rate $Rate
                if ($test.Ready) {
                    Write-Log "[VERIFY] Motor responded on $($port.Port)."
                    return $port.Port
                }
                if ($test.Error) {
                    $lastError = $test.Error
                }
            }
        }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $deadline)

    if ($lastError -match "denied|拒绝") {
        throw "Motor CDC is busy. Close all serial terminals and retry. Last error: $lastError"
    }
    throw "Motor did not respond after $Stage. A power cycle with BOOT0=0 is only a fallback after checking the USB connection. Last error: $lastError"
}

function Resolve-ProgrammerCli {
    param([string]$ConfiguredPath)

    $candidates = @()
    if ($ConfiguredPath) {
        $candidates += $ConfiguredPath
    }
    $command = Get-Command STM32_Programmer_CLI.exe -ErrorAction SilentlyContinue
    if ($command) {
        $candidates += $command.Source
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
    throw "STM32_Programmer_CLI.exe not found."
}

function Test-DfuPresent {
    param([string]$CliPath)

    $output = (& $CliPath -l usb 2>&1 | Out-String)
    return (($output -match "DFU Interface") -and ($output -notmatch "No STM32 device"))
}

function Send-BootDfu {
    param(
        [string]$PortName,
        [int]$Rate
    )

    Write-Log "[VERIFY] Sending BOOTDFU over $PortName ..."
    $serial = New-Object System.IO.Ports.SerialPort(
        $PortName,
        $Rate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One)
    $serial.NewLine = "`r`n"
    $serial.WriteTimeout = 1000

    try {
        $serial.Open()
        $serial.DtrEnable = $true
        $serial.RtsEnable = $true
        Start-Sleep -Milliseconds 250
        $serial.WriteLine("BOOTDFU")
        Start-Sleep -Milliseconds 500
    } catch {
        if (-not (Test-DfuPresent -CliPath $script:ResolvedProgrammerCli)) {
            throw
        }
    } finally {
        if ($serial.IsOpen) {
            $serial.Close()
        }
        $serial.Dispose()
    }
}

function Wait-DfuReady {
    param(
        [string]$CliPath,
        [int]$TimeoutSeconds
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    Write-Log "[VERIFY] Waiting up to ${TimeoutSeconds}s for STM32 ROM DFU ..."
    do {
        if (Test-DfuPresent -CliPath $CliPath) {
            Write-Log "[VERIFY] STM32 ROM DFU detected."
            return
        }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $deadline)

    throw "STM32 ROM DFU did not enumerate after BOOTDFU. Check the USB cable and BOOT0 option-byte configuration."
}

function Invoke-ChildPowerShell {
    param(
        [string]$ScriptPath,
        [string[]]$Arguments,
        [string]$Stage
    )

    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $ScriptPath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Stage failed with exit code $LASTEXITCODE."
    }
}

try {
    Set-Content -LiteralPath $LogPath -Value ("[{0}] Motor USB update verification started" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss")) -Encoding ASCII

    $resolvedHex = Resolve-ExistingPath -Path $HexPath
    $helperHexPath = Get-RepoRelativePath -ResolvedPath $resolvedHex
    $range = Get-IntelHexRange -Path $resolvedHex
    if (($range.Min -lt 0x08040000L) -or ($range.Max -ge 0x08200000L)) {
        throw ("Motor hex address range is invalid: 0x{0:X8}-0x{1:X8}. Expected 0x08040000-0x081FFFFF." -f $range.Min, $range.Max)
    }

    $script:ResolvedProgrammerCli = Resolve-ProgrammerCli -ConfiguredPath $ProgrammerCli
    Write-Log "[VERIFY] Hex: $resolvedHex"
    Write-Log ("[VERIFY] Hex range: 0x{0:X8}-0x{1:X8}" -f $range.Min, $range.Max)
    Write-Log "[VERIFY] STM32CubeProgrammer: $script:ResolvedProgrammerCli"
    Write-Log "[VERIFY] H7BOOT drive: $(Get-H7BootDrive)"
    $ports = @(Get-CdcComPorts)
    Write-Log "[VERIFY] H7 CDC ports: $(($ports.Port -join ', '))"
    Write-Log "[VERIFY] ROM DFU present: $(Test-DfuPresent -CliPath $script:ResolvedProgrammerCli)"

    if ($PreflightOnly) {
        Write-Log "[VERIFY] Preflight passed. No device state was changed."
        exit 0
    }

    if (Test-DfuPresent -CliPath $script:ResolvedProgrammerCli) {
        throw "Board is already in ROM DFU. Set BOOT0=0 and reset to start the H7BOOT/Motor verification flow."
    }

    $existingDrive = Get-H7BootDrive
    if (-not $existingDrive) {
        $port = Get-SelectedCdcPort -RequestedPort $ComPort
        if ($null -ne $port) {
            $portTest = Test-MotorPort -PortName $port.Port -Rate $BaudRate
            if ($portTest.Error -match "denied|拒绝") {
                throw "Close the serial terminal using $($port.Port) before running this script."
            }
        }
    }

    Write-Log "[VERIFY] Stage 1/4: install Motor through H7BOOT MSC."
    $mscArguments = @(
        "-HexPath", $helperHexPath,
        "-ComPort", $ComPort,
        "-BaudRate", "$BaudRate",
        "-WaitSeconds", "$MscWaitSeconds",
        "-NoPause"
    )
    Invoke-ChildPowerShell -ScriptPath (Join-Path $PSScriptRoot "msc_download.ps1") -Arguments $mscArguments -Stage "H7BOOT install"

    Write-Log "[VERIFY] Stage 2/4: verify Motor starts after the bootloader reset."
    $motorPort = Wait-MotorReady -RequestedPort $ComPort -Rate $BaudRate -TimeoutSeconds $AppWaitSeconds -Stage "H7BOOT install"

    Write-Log "[VERIFY] Stage 3/4: request ROM DFU from Motor and download the same hex."
    Send-BootDfu -PortName $motorPort -Rate $BaudRate
    Wait-DfuReady -CliPath $script:ResolvedProgrammerCli -TimeoutSeconds $DfuWaitSeconds

    $dfuArguments = @(
        "-HexPath", $helperHexPath,
        "-ProgrammerCli", $script:ResolvedProgrammerCli,
        "-WaitSeconds", "$DfuWaitSeconds",
        "-ToolTimeoutSeconds", "$ToolTimeoutSeconds",
        "-GoAddress", "0x08000000",
        "-SkipSerialDetach",
        "-NoPause"
    )
    Invoke-ChildPowerShell -ScriptPath (Join-Path $PSScriptRoot "dfu_download.ps1") -Arguments $dfuArguments -Stage "ROM DFU download"

    Write-Log "[VERIFY] Stage 4/4: verify Motor starts after the ROM DFU download."
    [void](Wait-MotorReady -RequestedPort $ComPort -Rate $BaudRate -TimeoutSeconds $AppWaitSeconds -Stage "ROM DFU download")

    Write-Log "[VERIFY] PASS: H7BOOT install, Motor BOOTDFU, ROM DFU download, and final Motor startup all succeeded."
    exit 0
} catch {
    Write-Log "[VERIFY] FAIL: $($_.Exception.Message)"
    exit 1
}
