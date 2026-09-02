param(
    [Parameter(Mandatory = $true)]
    [string]$Path,
    [int]$Trial = -1,
    [double]$Middle = [double]::NaN,
    [double]$BalanceKp = [double]::NaN,
    [double]$BalanceKd = [double]::NaN,
    [double]$VelocityKp = [double]::NaN,
    [double]$VelocityKi = [double]::NaN,
    [double]$PitchLimit = 5.0
)

$culture = [System.Globalization.CultureInfo]::InvariantCulture
$lines = Get-Content -LiteralPath $Path
$config = @{}
$rows = [System.Collections.Generic.List[object]]::new()
$header = $null

function Parse-Double([string]$value, [double]$default = 0.0) {
    if ([string]::IsNullOrWhiteSpace($value)) { return $default }
    return [double]::Parse($value, $culture)
}

function Parse-Int([string]$value, [int]$default = 0) {
    if ([string]::IsNullOrWhiteSpace($value)) { return $default }
    return [int]::Parse($value, $culture)
}

function Column-Value($parts, $map, [string]$name, [string]$default = '0') {
    if ($null -ne $map -and $map.ContainsKey($name)) {
        $index = $map[$name]
        if ($index -lt $parts.Count) { return $parts[$index].Trim() }
    }
    return $default
}

foreach ($line in $lines) {
    $line = $line.Trim()
    if ($line.StartsWith('#config,')) {
        foreach ($field in $line.Substring(8).Split(',')) {
            $pair = $field.Split('=', 2)
            if ($pair.Count -eq 2) {
                $config[$pair[0].Trim()] = $pair[1].Trim()
            }
        }
        continue
    }

    if ($line.StartsWith('ms,')) {
        $header = @{}
        $names = $line.Split(',')
        for ($i = 0; $i -lt $names.Count; $i++) {
            $header[$names[$i].Trim().ToLowerInvariant()] = $i
        }
        continue
    }

    $parts = $line.Split(',')
    if ($parts.Count -lt 10 -or $parts[0] -notmatch '^\d+$') { continue }

    try {
        if ($null -ne $header -and $header.ContainsKey('requested_pwm')) {
            $rows.Add([pscustomobject]@{
                Ms = Parse-Int (Column-Value $parts $header 'ms')
                Pitch = Parse-Double (Column-Value $parts $header 'pitch')
                Gyro = Parse-Double (Column-Value $parts $header 'gyro')
                EncL = Parse-Int (Column-Value $parts $header 'enc_l')
                EncR = Parse-Int (Column-Value $parts $header 'enc_r')
                SpeedF = Parse-Double (Column-Value $parts $header 'speed_f')
                SpeedI = Parse-Double (Column-Value $parts $header 'speed_i')
                BalancePwm = Parse-Int (Column-Value $parts $header 'balance_pwm')
                VelocityPwm = Parse-Int (Column-Value $parts $header 'velocity_pwm')
                RequestedPwm = Parse-Int (Column-Value $parts $header 'requested_pwm')
                PwmL = Parse-Int (Column-Value $parts $header 'pwm_l')
                PwmR = Parse-Int (Column-Value $parts $header 'pwm_r')
                LoopDt = Parse-Int (Column-Value $parts $header 'loop_dt')
                DrdyMissed = Parse-Int (Column-Value $parts $header 'drdy_missed')
                State = Column-Value $parts $header 'state' 'STOPPED'
            })
        }
        elseif ($parts.Count -ge 12) {
            # Legacy target-angle cascade format.
            $balance = Parse-Int $parts[7]
            $rows.Add([pscustomobject]@{
                Ms = Parse-Int $parts[0]; Pitch = Parse-Double $parts[1]
                Gyro = Parse-Double $parts[2]; EncL = Parse-Int $parts[3]
                EncR = Parse-Int $parts[4]; SpeedF = 0.0; SpeedI = 0.0
                BalancePwm = $balance; VelocityPwm = 0; RequestedPwm = $balance
                PwmL = Parse-Int $parts[8]; PwmR = Parse-Int $parts[9]
                LoopDt = Parse-Int $parts[10]; DrdyMissed = 0
                State = $parts[11].Trim()
            })
        }
        else {
            # Original direct-PWM format.
            $balance = Parse-Int $parts[5]
            $velocity = Parse-Int $parts[6]
            $rows.Add([pscustomobject]@{
                Ms = Parse-Int $parts[0]; Pitch = Parse-Double $parts[1]
                Gyro = Parse-Double $parts[2]; EncL = Parse-Int $parts[3]
                EncR = Parse-Int $parts[4]; SpeedF = 0.0; SpeedI = 0.0
                BalancePwm = $balance; VelocityPwm = $velocity
                RequestedPwm = $balance + $velocity
                PwmL = Parse-Int $parts[7]; PwmR = Parse-Int $parts[8]
                LoopDt = 0; DrdyMissed = 0; State = $parts[9].Trim()
            })
        }
    }
    catch { continue }
}

if ($rows.Count -eq 0) { throw 'No balance telemetry rows were found.' }

if ($Trial -lt 0 -and $config.ContainsKey('trial')) { $Trial = [int]$config['trial'] }
if ([double]::IsNaN($Middle)) {
    $Middle = if ($config.ContainsKey('middle')) { Parse-Double $config['middle'] } else { 0.0 }
}
if ([double]::IsNaN($BalanceKp) -and $config.ContainsKey('balance_kp')) { $BalanceKp = Parse-Double $config['balance_kp'] }
if ([double]::IsNaN($BalanceKd) -and $config.ContainsKey('balance_kd')) { $BalanceKd = Parse-Double $config['balance_kd'] }
if ([double]::IsNaN($VelocityKp) -and $config.ContainsKey('velocity_kp')) { $VelocityKp = Parse-Double $config['velocity_kp'] }
if ([double]::IsNaN($VelocityKi) -and $config.ContainsKey('velocity_ki')) { $VelocityKi = Parse-Double $config['velocity_ki'] }

$pwmMap = if ($config.ContainsKey('pwm_map')) { $config['pwm_map'] } else { 'off' }
$pwmDeadband = if ($config.ContainsKey('pwm_deadband')) { Parse-Int $config['pwm_deadband'] } else { 0 }
$mode = if ($config.ContainsKey('mode')) { $config['mode'] } else { 'UNKNOWN' }
$isWheeltecMode = $mode -match 'WHEELTEC'
$runRows = @($rows | Where-Object { $_.State -eq 'RUN' })
if ($runRows.Count -eq 0) { throw 'No RUN telemetry rows were found.' }

$firstMs = $runRows[0].Ms
$runDurationMs = $runRows[-1].Ms - $firstMs
$firstFailure = $rows | Where-Object {
    $_.Ms -ge $firstMs -and
    ($_.State -ne 'RUN' -or [math]::Abs($_.Pitch - $Middle) -gt $PitchLimit -or
     [math]::Abs($_.PwmL) -ge 6800 -or [math]::Abs($_.PwmR) -ge 6800)
} | Select-Object -First 1
$controlledDurationMs = if ($null -eq $firstFailure) { $runDurationMs } else { $firstFailure.Ms - $firstMs }

$errors = @($runRows | ForEach-Object { $_.Pitch - $Middle })
$rms = [math]::Sqrt((($errors | ForEach-Object { $_ * $_ } | Measure-Object -Sum).Sum) / $errors.Count)
$maxAbs = ($errors | ForEach-Object { [math]::Abs($_) } | Measure-Object -Maximum).Maximum
$third = [math]::Max(1, [int][math]::Floor($errors.Count / 3))
$early = @($errors | Select-Object -First $third)
$late = @($errors | Select-Object -Last $third)
$earlyRms = [math]::Sqrt((($early | ForEach-Object { $_ * $_ } | Measure-Object -Sum).Sum) / $early.Count)
$lateRms = [math]::Sqrt((($late | ForEach-Object { $_ * $_ } | Measure-Object -Sum).Sum) / $late.Count)
$saturationCount = @($runRows | Where-Object { [math]::Abs($_.PwmL) -ge 6800 -or [math]::Abs($_.PwmR) -ge 6800 }).Count
$maxLoopDtMs = ($rows | Measure-Object -Property LoopDt -Maximum).Maximum
$maxDrdyMissed = ($rows | Measure-Object -Property DrdyMissed -Maximum).Maximum
$maxVelocityPwm = ($runRows | ForEach-Object { [math]::Abs($_.VelocityPwm) } | Measure-Object -Maximum).Maximum
$movingRows = @($runRows | Where-Object { [math]::Abs($_.EncL + $_.EncR) -ge 20 })
$velocityInactiveRows = @($movingRows | Where-Object { $_.VelocityPwm -eq 0 }).Count
$velocityWrongSignRows = @($movingRows | Where-Object {
    $_.VelocityPwm -ne 0 -and [math]::Sign($_.VelocityPwm) -ne [math]::Sign($_.EncL + $_.EncR)
}).Count
$inactiveRatio = if ($movingRows.Count -eq 0) { 0.0 } else { $velocityInactiveRows / $movingRows.Count }
$wrongSignRatio = if ($movingRows.Count -eq 0) { 0.0 } else { $velocityWrongSignRows / $movingRows.Count }
$pwmCompensatedCount = @($runRows | Where-Object {
    [math]::Abs($_.RequestedPwm) -gt $pwmDeadband -and
    ([math]::Abs($_.PwmL) -gt [math]::Abs($_.RequestedPwm) -or
     [math]::Abs($_.PwmR) -gt [math]::Abs($_.RequestedPwm))
}).Count
$pwmDeadbandSuppressedCount = @($runRows | Where-Object {
    [math]::Abs($_.RequestedPwm) -gt 0 -and
    [math]::Abs($_.RequestedPwm) -le $pwmDeadband -and
    $_.PwmL -eq 0 -and $_.PwmR -eq 0
}).Count
$maxAppliedPwm = ($runRows | ForEach-Object { [math]::Max([math]::Abs($_.PwmL), [math]::Abs($_.PwmR)) } | Measure-Object -Maximum).Maximum

if ($maxDrdyMissed -gt 0 -or @($rows | Where-Object { $_.State -eq 'OVERRUN' }).Count -gt 0) {
    $result = 'DATA_READY_OVERRUN'
    $decision = 'Fix PB9 interrupt delivery or ControlTask execution time before changing PID gains.'
}
elseif ($maxLoopDtMs -gt 15) {
    $result = 'CONTROL_LOOP_JITTER'
    $decision = 'Fix control timing before changing PID gains.'
}
elseif ($isWheeltecMode -and $movingRows.Count -ge 3 -and $wrongSignRatio -ge 0.5) {
    $result = 'SPEED_FEEDBACK_DIRECTION_ERROR'
    $decision = 'Correct only the motor or encoder direction macros; keep the controller formula unchanged.'
}
elseif ($isWheeltecMode -and $movingRows.Count -ge 3 -and $inactiveRatio -ge 0.5) {
    $result = 'VELOCITY_LOOP_INACTIVE'
    $decision = 'Confirm the WHEELTEC direct-PWM speed loop is compiled and velocity_pwm is not being cleared.'
}
elseif ($controlledDurationMs -ge 5000 -and $lateRms -le ($earlyRms * 1.25 + 0.25)) {
    $result = 'STABLE_5S_CANDIDATE'
    $decision = 'Keep angle gains; verify drift recovery and the push-back test.'
}
elseif ($lateRms -gt ($earlyRms * 1.5 + 0.5)) {
    $result = 'GROWING_OSCILLATION'
    $decision = 'Verify signs and timing, then reduce Balance_Kp or increase Balance_Kd.'
}
elseif ($saturationCount -gt 0) {
    $result = 'PWM_SATURATION'
    $decision = 'Inspect angle error and speed integral before another floor test.'
}
else {
    $result = 'UNDER_5S_REVIEW'
    $decision = 'Review direction, midpoint, angle damping, and speed-loop output.'
}

$trialText = if ($Trial -ge 0) { $Trial.ToString($culture) } else { '?' }
$kpText = if ([double]::IsNaN($BalanceKp)) { '?' } else { $BalanceKp.ToString('0.###', $culture) }
$kdText = if ([double]::IsNaN($BalanceKd)) { '?' } else { $BalanceKd.ToString('0.###', $culture) }
$vkpText = if ([double]::IsNaN($VelocityKp)) { '?' } else { $VelocityKp.ToString('0.####', $culture) }
$vkiText = if ([double]::IsNaN($VelocityKi)) { '?' } else { $VelocityKi.ToString('0.#####', $culture) }

Write-Output "trial=$trialText mode=$mode"
Write-Output ("pitch_acceptance_limit={0:F2} deg" -f $PitchLimit)
Write-Output ("run_ms={0} controlled_ms={1} pitch_rms={2:F2} max_abs_pitch={3:F2}" -f $runDurationMs, $controlledDurationMs, $rms, $maxAbs)
Write-Output ("early_rms={0:F2} late_rms={1:F2} saturation_rows={2}" -f $earlyRms, $lateRms, $saturationCount)
Write-Output ("max_velocity_pwm={0} moving_rows={1} inactive_rows={2} wrong_sign_rows={3}" -f $maxVelocityPwm, $movingRows.Count, $velocityInactiveRows, $velocityWrongSignRows)
Write-Output ("max_control_loop_dt={0} ms drdy_missed={1}" -f $maxLoopDtMs, $maxDrdyMissed)
Write-Output ("pwm_map={0} compensated_rows={1} deadband_suppressed_rows={2} max_applied_pwm={3}" -f $pwmMap, $pwmCompensatedCount, $pwmDeadbandSuppressedCount, $maxAppliedPwm)
Write-Output "result=$result"
Write-Output "decision=$decision"
Write-Output ''
Write-Output '| Trial | Mode and parameters | Observation | Analysis and next decision |'
Write-Output '|---:|---|---|---|'
Write-Output "| $trialText | $mode; B_Kp=$kpText, B_Kd=$kdText, V_Kp=$vkpText, V_Ki=$vkiText | $result; controlled ${controlledDurationMs} ms, pitch RMS $($rms.ToString('0.00', $culture)) deg | $decision |"
