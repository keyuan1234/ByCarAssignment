param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$gcc = (Get-Command gcc -ErrorAction Stop).Source
$buildDir = Join-Path $PSScriptRoot 'build'
$includeDir = Join-Path $root 'Core\Inc'
$testSource = Join-Path $PSScriptRoot 'test_line_tracking.c'
$lineTrackingSource = Join-Path $root 'Core\Src\line_tracking.c'
$output = Join-Path $buildDir 'line_tracking.exe'

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

& $gcc -std=c99 -Wall -Wextra -Werror `
    "-I$includeDir" `
    $testSource $lineTrackingSource -o $output
if ($LASTEXITCODE -ne 0) {
    throw 'GCC failed for line tracking tests'
}

& $output
if ($LASTEXITCODE -ne 0) {
    throw 'Line tracking tests failed'
}

Write-Output 'line tracking decode and motion mapping: PASS'
