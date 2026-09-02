param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$gcc = (Get-Command gcc -ErrorAction Stop).Source
$buildDir = Join-Path $PSScriptRoot 'build'
$stubDir = Join-Path $PSScriptRoot 'stubs'
$includeDir = Join-Path $root 'Core\Inc'
$testSource = Join-Path $PSScriptRoot 'test_balance_control.c'
$controlSource = Join-Path $root 'Core\Src\balance_control.c'

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

foreach ($mode in @(0, 1)) {
    $output = Join-Path $buildDir "balance_control_mode$mode.exe"
    & $gcc -std=c99 -Wall -Wextra -Werror `
        "-DBALANCE_CONTROL_MODE=$($mode)U" `
        "-I$stubDir" "-I$includeDir" `
        $testSource $controlSource -o $output
    if ($LASTEXITCODE -ne 0) {
        throw "GCC failed for balance mode $mode"
    }

    & $output
    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed for balance mode $mode"
    }
}

Write-Output 'balance control angle-only and WHEELTEC cascade modes: PASS'
