param(
    [int]$Threads = 8,
    [string]$BuildDir = "build",
    [string]$OutputDir = "cppcheck-html",
    [switch]$OpenReport
)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "=== Cppcheck Análisis Exhaustivo ===" -ForegroundColor Cyan
Write-Host ""

# Rutas fijas
$CppcheckExe = "C:\msys64\mingw64\bin\cppcheck.exe"

$PossibleHtmlReports = @(
    "C:\msys64\mingw64\bin\cppcheck-htmlreport",
    "C:\msys64\mingw64\bin\cppcheck-htmlreport.py"
)

$HtmlReportExe = $PossibleHtmlReports |
    Where-Object { Test-Path $_ } |
    Select-Object -First 1

if (-not (Test-Path $CppcheckExe)) {
    throw "No existe: $CppcheckExe"
}

if (-not $HtmlReportExe) {
    throw "No se encontró cppcheck-htmlreport."
}

# Localizar compile_commands.json
$CompileCommands = Join-Path $BuildDir "compile_commands.json"

if (-not (Test-Path $CompileCommands)) {
    throw @"
No existe '$CompileCommands'.

Genera el archivo con:

cmake -B $BuildDir -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
"@
}

# Limpiar reportes anteriores
Remove-Item $OutputDir -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item "cppcheck-report.xml" -Force -ErrorAction SilentlyContinue
Remove-Item "cppcheck.log" -Force -ErrorAction SilentlyContinue

Write-Host "Cppcheck: $CppcheckExe" -ForegroundColor Yellow
Write-Host "HTML Report: $HtmlReportExe" -ForegroundColor Yellow
Write-Host "Compile DB: $CompileCommands" -ForegroundColor Yellow
Write-Host "Hilos: $Threads" -ForegroundColor Yellow
Write-Host ""

$Version = & $CppcheckExe --version
Write-Host $Version -ForegroundColor Green
Write-Host ""

$StartTime = Get-Date

& $CppcheckExe `
    -j $Threads `
    --project="$CompileCommands" `
    --std=c23 `
    --language=c `
    --enable=all `
    --inconclusive `
    --force `
    --inline-suppr `
    --check-level=exhaustive `
    --xml `
    --xml-version=2 `
    2> cppcheck-report.xml

$EndTime = Get-Date
$Duration = New-TimeSpan -Start $StartTime -End $EndTime

Write-Host ""
Write-Host "Análisis finalizado en $($Duration.ToString())" -ForegroundColor Green

# Generar informe HTML
if ($HtmlReportExe.EndsWith(".py")) {

    python $HtmlReportExe `
        --file=cppcheck-report.xml `
        --report-dir=$OutputDir `
        --source-dir=.

} else {

    & $HtmlReportExe `
        --file=cppcheck-report.xml `
        --report-dir=$OutputDir `
        --source-dir=.
}

$Index = Join-Path $OutputDir "index.html"

if (-not (Test-Path $Index)) {
    throw "No se pudo generar el informe HTML."
}

Write-Host ""
Write-Host "Informe generado:" -ForegroundColor Green
Write-Host $Index -ForegroundColor Cyan

if (Test-Path "cppcheck-report.xml") {

    [xml]$Xml = Get-Content "cppcheck-report.xml"

    $Errors = @($Xml.results.errors.error)

    Write-Host ""
    Write-Host "Resumen:" -ForegroundColor Cyan
    Write-Host "Hallazgos: $($Errors.Count)"

    $Errors |
        Group-Object severity |
        Sort-Object Count -Descending |
        ForEach-Object {
            Write-Host (" - {0}: {1}" -f $_.Name, $_.Count)
        }
}

if ($OpenReport) {
    Start-Process $Index
}
```
