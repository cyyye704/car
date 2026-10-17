#requires -Version 5.1
<#
Build only the official E01 LED example; never edit Keil system configuration.
Keil CLI: https://www.keil.com/support/man/docs/uv4/uv4_commandline.asp
VS Code: https://code.visualstudio.com/docs/debugtest/tasks
Start-Process: https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.management/start-process?view=powershell-5.1

Default: full rebuild (-r). Use -Incremental (alias -b) for Keil -b.
An incremental no-op does not satisfy the fresh-HEX verification requirement.
-ShowLog reads the most recent completed build without invoking Keil.
#>
[CmdletBinding()]
param(
    [Alias('b')][switch]$Incremental,
    [switch]$ShowLog,
    [ValidateRange(10, 3600)][int]$TimeoutSeconds = 120,
    [string]$Uv4Path = 'C:\Keil\UV4\UV4.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$utf8 = New-Object System.Text.UTF8Encoding($false)
[Console]::OutputEncoding = $utf8
$OutputEncoding = $utf8
$workspace = Split-Path -Parent $PSScriptRoot
$project = Join-Path $workspace 'STC32G144K246_100Pin_Library\Example\Coreboard_Demo\E01_led_demo\mdk\seekfree.uvproj'
$projectDirectory = Split-Path -Parent $project
$target = 'stc32g144k246'
$buildRoot = Join-Path $workspace '.build\led'
$latestLog = Join-Path $buildRoot 'latest.log'

if ($ShowLog) {
    if (-not (Test-Path -LiteralPath $latestLog -PathType Leaf)) {
        Write-Host ('No build log yet: ' + $latestLog)
        exit 1
    }
    Get-Content -LiteralPath $latestLog -Encoding UTF8
    exit 0
}

$started = [DateTime]::UtcNow
$archive = Join-Path $buildRoot ((Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + $PID)
$null = New-Item -ItemType Directory -Path $archive -Force
$rawLog = Join-Path $archive 'keil.log'
$script:LogLines = New-Object 'System.Collections.Generic.List[string]'
$lockStream = $null
$process = $null
$rawText = ''
$rawRead = $false
$mode = if ($Incremental) { '-b' } else { '-r' }
$result = [ordered]@{
    success = $false
    failureCategory = $null
    failureReason = $null
    startedUtc = $started.ToString('o')
    finishedUtc = $null
    project = $project
    target = $target
    mode = $mode
    uv4 = $Uv4Path
    keilProcessId = $null
    keilExitCode = $null
    hexPath = $null
    hexVerified = $false
    hexLastWriteTimeUtc = $null
    hexSha256 = $null
    hexDataBytes = 0
    previousHexBackup = $null
    rawLog = $rawLog
    log = (Join-Path $archive 'build.log')
}

function Add-Log([string]$Text) {
    $script:LogLines.Add($Text)
    Write-Host $Text
}

function Write-KeilDiagnostics([string]$Text) {
    foreach ($line in ($Text -split '\r?\n')) {
        if ($line -match '^\s*(.+?)\((\d+)\)\s*:\s*(?:fatal\s+)?(error|warning)\s+([A-Z]\d+)\s*:\s*(.*)$') {
            $file = $Matches[1].Trim().Trim([char]34, [char]39)
            $lineNumber = $Matches[2]
            $severity = $Matches[3].ToLowerInvariant()
            $code = $Matches[4]
            $message = $Matches[5]
            if (-not [IO.Path]::IsPathRooted($file)) { $file = Join-Path $projectDirectory $file }
            $file = [IO.Path]::GetFullPath($file)
            Add-Log ('KEIL_DIAGNOSTIC {0}({1}): {2} {3}: {4}' -f $file, $lineNumber, $severity, $code, $message)
        }
        elseif ($line -match '^\s*\*{3}\s*(?:FATAL\s+)?(ERROR|WARNING)\s+([A-Z]\d+)\s*:\s*(.*)$') {
            Add-Log ('KEIL_DIAGNOSTIC {0}(1): {1} {2}: {3}' -f $project, $Matches[1].ToLowerInvariant(), $Matches[2], $Matches[3])
        }
    }
}

function Test-IntelHex([string]$Path) {
    $eof = $false
    [long]$dataBytes = 0
    $lineNumber = 0
    foreach ($record in [IO.File]::ReadAllLines($Path)) {
        $lineNumber++
        $line = $record.Trim()
        if ($line.Length -eq 0) { continue }
        if ($eof) { throw "HEX has content after EOF at line $lineNumber." }
        if ($line -notmatch '^:[0-9A-Fa-f]+$' -or ($line.Length % 2) -ne 1 -or $line.Length -lt 11) {
            throw "Malformed Intel HEX at line $lineNumber."
        }
        $bytes = New-Object 'System.Collections.Generic.List[int]'
        for ($i = 1; $i -lt $line.Length; $i += 2) { $bytes.Add([Convert]::ToInt32($line.Substring($i, 2), 16)) }
        if ($bytes.Count -ne ($bytes[0] + 5)) { throw "HEX length mismatch at line $lineNumber." }
        $sum = 0
        foreach ($value in $bytes) { $sum += $value }
        if (($sum % 256) -ne 0) { throw "HEX checksum mismatch at line $lineNumber." }
        $count = $bytes[0]
        $address = $bytes[1] * 256 + $bytes[2]
        switch ($bytes[3]) {
            0 { $dataBytes += $count }
            1 {
                if ($count -ne 0 -or $address -ne 0) { throw "Invalid HEX EOF at line $lineNumber." }
                $eof = $true
            }
            { $_ -eq 2 -or $_ -eq 4 } {
                if ($count -ne 2 -or $address -ne 0) { throw "Invalid HEX address record at line $lineNumber." }
            }
            { $_ -eq 3 -or $_ -eq 5 } {
                if ($count -ne 4 -or $address -ne 0) { throw "Invalid HEX start record at line $lineNumber." }
            }
            default { throw "Unsupported HEX record type at line $lineNumber." }
        }
    }
    if (-not $eof -or $dataBytes -eq 0) { throw 'HEX must contain an EOF record and nonempty data.' }
    return $dataBytes
}

try {
    try {
        # An exclusive handle avoids stale-lock problems after a crashed task.
        $lockStream = [IO.File]::Open((Join-Path $buildRoot 'build.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    }
    catch {
        $result.failureCategory = 'BuildLocked'
        throw 'Another LED build holds the workspace lock. Wait for it to finish.'
    }
    Add-Log ('Project: ' + $project)
    Add-Log ('Target: ' + $target + ' | Mode: ' + $mode)
    Add-Log ('Archive: ' + $archive)
    if (-not (Test-Path -LiteralPath $Uv4Path -PathType Leaf)) { throw ('UV4 executable not found: ' + $Uv4Path) }
    if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { throw ('LED project not found: ' + $project) }
    [xml]$projectXml = [IO.File]::ReadAllText($project)
    $targets = @($projectXml.Project.Targets.Target | Where-Object { $_.TargetName -eq $target })
    if ($targets.Count -ne 1) { throw ('Expected exactly one LED target: ' + $target) }
    $common = $targets[0].TargetOption.TargetCommonOption
    if ($common.CreateHexFile -ne '1') { throw 'CreateHexFile is disabled in the LED project.' }
    $outputDirectory = [string]$common.OutputDirectory
    if (-not [IO.Path]::IsPathRooted($outputDirectory)) { $outputDirectory = Join-Path $projectDirectory $outputDirectory }
    $hexPath = [IO.Path]::GetFullPath((Join-Path $outputDirectory ([string]$common.OutputName + '.hex')))
    $result.hexPath = $hexPath
    if (Test-Path -LiteralPath $hexPath -PathType Leaf) {
        $result.previousHexBackup = Join-Path $archive 'previous.hex'
        Copy-Item -LiteralPath $hexPath -Destination $result.previousHexBackup
        Add-Log ('Previous HEX preserved: ' + $result.previousHexBackup)
    }
    # Arguments are passed to the executable, never through a command shell.
    # Windows file names cannot contain a quotation mark; target is fixed above.
    $arguments = '{0} "{1}" -t "{2}" -j0 -o "{3}"' -f $mode, $project, $target, $rawLog
    Add-Log ('Command: "' + $Uv4Path + '" ' + $arguments)
    $process = Start-Process -FilePath $Uv4Path -ArgumentList $arguments -WorkingDirectory $projectDirectory -WindowStyle Hidden -PassThru
    $null = $process.Handle
    $result.keilProcessId = $process.Id
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $result.failureCategory = 'Timeout'
        throw ('Keil exceeded ' + $TimeoutSeconds + ' seconds. Inspect the log and any Keil dialog manually.')
    }
    $process.Refresh()
    $result.keilExitCode = $process.ExitCode
    if (Test-Path -LiteralPath $rawLog -PathType Leaf) {
        # Preserve original bytes in keil.log; normalize the task log to UTF-8.
        $rawText = [IO.File]::ReadAllText($rawLog, [Text.Encoding]::Default)
        Add-Log $rawText
        Write-KeilDiagnostics $rawText
        $rawRead = $true
    }
    if ($rawText -match '\bL250\b') {
        $result.failureCategory = 'LicenseCodeSizeLimit'
        throw 'L251 rejected the image (L250): the installed restricted linker code-size limit was exceeded. A properly licensed C251 toolchain is required; no license/configuration changes were made.'
    }
    if ($rawText -match '\bC168\b') {
        $result.failureCategory = 'InterruptRangeC168'
        throw 'C168 interrupt range error: verify the official STC interrupt extension for this C251 installation. Interrupt declarations were not changed.'
    }
    if ($result.keilExitCode -eq 12 -or $rawText -match '(?i)Device not found') {
        $result.failureCategory = 'DeviceDatabaseMissing'
        throw 'Keil cannot resolve the configured STC device. Check the official device database integration; the project chip identity was preserved.'
    }
    if ($null -eq $result.keilExitCode -or $result.keilExitCode -notin @(0, 1)) {
        $result.failureCategory = 'KeilBuildFailed'
        throw ('Keil failed with exit code: ' + $result.keilExitCode)
    }
    if ($rawText -notmatch '(?i)\b0\s+Error\(s\)' -or $rawText -match '(?im)^\s*\*{3}\s*(?:FATAL\s+)?ERROR\b|\berror\s+[A-Z]\d+\s*:') {
        $result.failureCategory = 'NoSuccessfulBuildSummary'
        throw 'The Keil log does not confirm a completed build with 0 Error(s).'
    }
    $result.failureCategory = 'HexVerificationFailed'
    if (-not (Test-Path -LiteralPath $hexPath -PathType Leaf)) { throw ('No HEX generated: ' + $hexPath) }
    $hexInfo = Get-Item -LiteralPath $hexPath
    $result.hexLastWriteTimeUtc = $hexInfo.LastWriteTimeUtc.ToString('o')
    if ($hexInfo.LastWriteTimeUtc -lt $started) { throw 'HEX predates this build. Run the default full rebuild; an old HEX is not accepted.' }
    $result.hexDataBytes = Test-IntelHex $hexPath
    $result.hexSha256 = (Get-FileHash -LiteralPath $hexPath -Algorithm SHA256).Hash
    Copy-Item -LiteralPath $hexPath -Destination (Join-Path $archive 'SEEKFREE.verified.hex')
    $result.hexVerified = $true
    $result.success = $true
    $result.failureCategory = $null
    Add-Log ('VERIFIED: fresh Intel HEX, ' + $result.hexDataBytes + ' data bytes: ' + $hexPath)
    Add-Log ('SHA256: ' + $result.hexSha256)
}
catch {
    $result.success = $false
    $result.hexVerified = $false
    if ($null -eq $result.failureCategory) { $result.failureCategory = 'BuildSetupFailed' }
    $result.failureReason = $_.Exception.Message
    Add-Log ('FAILED: ' + $result.failureReason)
    Add-Log ('KEIL_DIAGNOSTIC {0}(1): error {1}: {2}' -f $project, $result.failureCategory, $result.failureReason)
}
finally {
    try {
        if ($null -ne $process) {
            if (-not $process.HasExited) {
                # Terminate only the process started by this invocation.
                $process.Kill()
                $null = $process.WaitForExit(5000)
            }
            $process.Dispose()
        }
        if (-not $rawRead -and (Test-Path -LiteralPath $rawLog -PathType Leaf)) {
            $rawText = [IO.File]::ReadAllText($rawLog, [Text.Encoding]::Default)
            Add-Log $rawText
            Write-KeilDiagnostics $rawText
        }
        $result.finishedUtc = [DateTime]::UtcNow.ToString('o')
        Add-Log ('Result: ' + (Join-Path $archive 'result.json'))
        [IO.File]::WriteAllText($result.log, ($script:LogLines -join [Environment]::NewLine), $utf8)
        $resultJson = $result | ConvertTo-Json -Depth 4
        [IO.File]::WriteAllText((Join-Path $archive 'result.json'), $resultJson, $utf8)
        if ($null -ne $lockStream) {
            [IO.File]::WriteAllText($latestLog, ($script:LogLines -join [Environment]::NewLine), $utf8)
            [IO.File]::WriteAllText((Join-Path $buildRoot 'latest-result.json'), $resultJson, $utf8)
        }
    }
    finally {
        if ($null -ne $lockStream) { $lockStream.Dispose() }
    }
}
if ($result.success) { exit 0 }
exit 1
