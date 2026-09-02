param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$gcc = (Get-Command gcc -ErrorAction Stop).Source
$buildDir = Join-Path $PSScriptRoot 'build'
$includeDir = Join-Path $root 'Core\Inc'
$testSource = Join-Path $PSScriptRoot 'test_bluetooth_control.c'
$controlSource = Join-Path $root 'Core\Src\bluetooth_control.c'
$output = Join-Path $buildDir 'bluetooth_control.exe'

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
& $gcc -std=c99 -Wall -Wextra -Werror "-I$includeDir" `
    $testSource $controlSource -o $output
if ($LASTEXITCODE -ne 0) {
    throw 'GCC failed for bluetooth control tests'
}

& $output
if ($LASTEXITCODE -ne 0) {
    throw 'Bluetooth control tests failed'
}

Write-Output 'bluetooth command, timeout, and AT response tests: PASS'
