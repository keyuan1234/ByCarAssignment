param()

$ErrorActionPreference = 'Stop'
$analyzer = (Resolve-Path (Join-Path $PSScriptRoot '..\Tools\Analyze-BalanceLog.ps1')).Path
$stableLog = (Resolve-Path (Join-Path $PSScriptRoot 'fixtures\telemetry_async_stable.txt')).Path
$jitterLog = (Resolve-Path (Join-Path $PSScriptRoot 'fixtures\telemetry_async_jitter.txt')).Path
$deadzoneLog = (Resolve-Path (Join-Path $PSScriptRoot 'fixtures\telemetry_deadzone_compensated.txt')).Path
$wheeltecLog = (Resolve-Path (Join-Path $PSScriptRoot 'fixtures\telemetry_wheeltec_stable.txt')).Path
$overrunLog = (Resolve-Path (Join-Path $PSScriptRoot 'fixtures\telemetry_drdy_overrun.txt')).Path

$stable = & $analyzer -Path $stableLog
if (($stable -join "`n") -notmatch 'result=STABLE_5S_CANDIDATE') {
    throw 'Stable telemetry fixture was not accepted.'
}
if (($stable -join "`n") -notmatch 'max_control_loop_dt=11 ms') {
    throw 'Stable telemetry fixture timing was parsed incorrectly.'
}

$jitter = & $analyzer -Path $jitterLog
if (($jitter -join "`n") -notmatch 'result=CONTROL_LOOP_JITTER') {
    throw 'Jitter telemetry fixture was not rejected.'
}
if (($jitter -join "`n") -notmatch 'max_control_loop_dt=20 ms') {
    throw 'Jitter telemetry fixture timing was parsed incorrectly.'
}

$deadzone = & $analyzer -Path $deadzoneLog
if (($deadzone -join "`n") -notmatch 'pwm_map=linear compensated_rows=2') {
    throw 'Deadzone-compensated rows were parsed incorrectly.'
}
if (($deadzone -join "`n") -notmatch 'deadband_suppressed_rows=1 max_applied_pwm=763') {
    throw 'Deadzone suppression or maximum applied PWM was parsed incorrectly.'
}

$wheeltec = & $analyzer -Path $wheeltecLog
if (($wheeltec -join "`n") -notmatch 'result=STABLE_5S_CANDIDATE') {
    throw 'New WHEELTEC telemetry format was not accepted.'
}
if (($wheeltec -join "`n") -notmatch 'drdy_missed=0') {
    throw 'New data-ready diagnostics were parsed incorrectly.'
}

$overrun = & $analyzer -Path $overrunLog
if (($overrun -join "`n") -notmatch 'result=DATA_READY_OVERRUN') {
    throw 'Data-ready overrun telemetry was not rejected.'
}
if (($overrun -join "`n") -notmatch 'drdy_missed=1') {
    throw 'Data-ready missed count was parsed incorrectly.'
}

Write-Output 'log analyzer legacy and PB9-synchronized telemetry tests: PASS'
