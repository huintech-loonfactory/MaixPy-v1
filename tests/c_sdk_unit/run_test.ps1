param()

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$testSrc = Join-Path $root "test_lcd_mcu_st7735s.c"
$lcdSrc = Join-Path $root "..\..\components\drivers\lcd\src\lcd_mcu.c"
$stubInc = Join-Path $root "stubs"
$buildDir = Join-Path $root "build"
$outExe = Join-Path $buildDir "test_lcd_mcu_st7735s.exe"

if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

$cc = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $cc) {
    $cc = Get-Command clang -ErrorAction SilentlyContinue
}

if (-not $cc) {
    Write-Error "gcc/clang not found. Install a host C compiler to run this unit test."
}

& $cc.Source "-std=c99" "-Wall" "-Wextra" "-I$stubInc" $testSrc $lcdSrc "-o" $outExe
if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed"
}

& $outExe
if ($LASTEXITCODE -ne 0) {
    Write-Error "Unit test binary failed"
}

Write-Host "Done: $outExe"
