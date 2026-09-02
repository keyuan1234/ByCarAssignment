param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$gcc = (Get-Command gcc -ErrorAction Stop).Source
$buildDir = Join-Path $PSScriptRoot 'build'
$includeDir = Join-Path $root 'Core\Inc'
$testSource = Join-Path $PSScriptRoot 'test_oled_dashboard.c'
$dashboardSource = Join-Path $root 'Core\Src\oled_dashboard.c'
$lineTrackingSource = Join-Path $root 'Core\Src\line_tracking.c'
$output = Join-Path $buildDir 'oled_dashboard.exe'

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

& $gcc -std=c99 -Wall -Wextra -Werror `
    "-I$includeDir" `
    $testSource $dashboardSource $lineTrackingSource -o $output
if ($LASTEXITCODE -ne 0) {
    throw 'GCC failed for OLED dashboard tests'
}

& $output
if ($LASTEXITCODE -ne 0) {
    throw 'OLED dashboard tests failed'
}

Write-Output 'OLED dashboard formatting and rendering: PASS'
