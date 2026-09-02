param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$gcc = (Get-Command gcc -ErrorAction Stop).Source
$buildDir = Join-Path $PSScriptRoot 'build'
$includeDir = Join-Path $root 'Core\Inc'
$testSource = Join-Path $PSScriptRoot 'test_pitch_kalman.c'
$filterSource = Join-Path $root 'Core\Src\pitch_kalman.c'
$output = Join-Path $buildDir 'pitch_kalman.exe'

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
& $gcc -std=c99 -Wall -Wextra -Werror "-I$includeDir" `
    $testSource $filterSource -o $output
if ($LASTEXITCODE -ne 0) {
    throw 'GCC failed for pitch Kalman tests'
}

& $output
if ($LASTEXITCODE -ne 0) {
    throw 'Pitch Kalman tests failed'
}

Write-Output 'pitch Kalman filter tests: PASS'
