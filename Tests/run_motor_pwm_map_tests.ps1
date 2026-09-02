param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$gcc = (Get-Command gcc -ErrorAction Stop).Source
$buildDir = Join-Path $PSScriptRoot 'build'
$includeDir = Join-Path $root 'Core\Inc'
$testSource = Join-Path $PSScriptRoot 'test_motor_pwm_map.c'
$mapSource = Join-Path $root 'Core\Src\motor_pwm_map.c'

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

foreach ($enabled in @(1, 0)) {
    $output = Join-Path $buildDir "motor_pwm_map_enabled$enabled.exe"
    & $gcc -std=c99 -Wall -Wextra -Werror `
        "-DMOTOR_PWM_DEADZONE_ENABLE=$($enabled)U" `
        "-I$includeDir" `
        $testSource $mapSource -o $output
    if ($LASTEXITCODE -ne 0) {
        throw "GCC failed for deadzone enable=$enabled"
    }

    & $output
    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed for deadzone enable=$enabled"
    }
}

Write-Output 'motor PWM deadzone enabled and disabled: PASS'
