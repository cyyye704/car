param([string]$Compiler = 'C:\msys64\ucrt64\bin\gcc.exe')
$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$output = Join-Path $workspace '.build\motor-logic'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$binary = Join-Path $output 'test_motor.exe'
$log = Join-Path $output 'latest.log'
$source = Join-Path $PSScriptRoot 'test_motor.c'
$firmware = Join-Path $workspace 'STC32G144K246_100Pin_Library\Example\Coreboard_Demo\E14_motor_test\user\main.c'
$lines = @('Firmware SHA256: ' + (Get-FileHash -LiteralPath $firmware -Algorithm SHA256).Hash)
& $Compiler '-std=c99' '-O2' '-Wall' '-Wextra' '-Werror' "-I$PSScriptRoot" $source '-o' $binary
if ($LASTEXITCODE -ne 0) { throw "Host compiler failed: $LASTEXITCODE" }
foreach ($case in @('boot-held', 'boot-lockout', 'release-required', 'button-bounce', 'both-channels-configured-duty', 'release-keeps-running', 'start-held-no-retrigger', 'stop-held-no-restart', 'release-before-stop', 'release-before-restart', 'stop-press-bounce', 'no-old-cooldown', 'repeated-start-stop', 'led-ready-heartbeat')) {
    $result = & $binary $case 2>&1
    $lines += $result
    $result | Write-Output
    if ($LASTEXITCODE -ne 0) {
        $lines | Set-Content -LiteralPath $log -Encoding UTF8
        throw "Host scenario failed: $case"
    }
}
foreach ($seed in @('0x32A5144', '0x251560', '0x12345678', '0xDEADBEEF')) {
    $result = & $binary 'random-inputs' $seed 2>&1
    $lines += "Seed: $seed"
    $lines += $result
    $result | Write-Output
    if ($LASTEXITCODE -ne 0) {
        $lines | Set-Content -LiteralPath $log -Encoding UTF8
        throw "Random host scenario failed: $seed"
    }
}
$lines += 'Host tests exercise the actual motor_tick C logic with mocked APIs. They do not verify MCU interrupt timing, PWM waveforms, boot/reset electrical states, motor current, or wiring.'
$lines | Set-Content -LiteralPath $log -Encoding UTF8
Write-Output "Host logic tests passed. Log: $log"
