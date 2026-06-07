# ==============================================================================
# sonardupli.ps1  -  Exporta codigo duplicado de SonarCloud
# Compatible con PowerShell 5 (Windows por defecto)
# Uso:
#   $env:SONAR_TOKEN = "tu_token"
#   powershell -ExecutionPolicy Bypass -File ".\Export-SonarDuplications.ps1"
# ==============================================================================
param(
    [string]$Token           = $env:SONAR_TOKEN,
    [string]$Project         = "thomashamer3_MiFutbolC",
    [string]$OutDir          = ".",
    [int]$MinDuplicatedLines = 0
)

if (-not $Token) {
    Write-Error "Token no encontrado. Definí SONAR_TOKEN o pasa -Token <valor>."
    exit 1
}
if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$headers = @{ Authorization = "Bearer $Token" }

# Helper: reemplaza ?? que no existe en PS5
function Coalesce($a, $b) { if ($null -ne $a -and $a -ne "") { $a } else { $b } }

# ---------- 1. Resumen general ------------------------------------------------
Write-Host ""
Write-Host "Obteniendo resumen de duplicaciones del proyecto..." -ForegroundColor Yellow

$metricsUrl = "https://sonarcloud.io/api/measures/component?component={0}&metricKeys=duplicated_lines,duplicated_lines_density,duplicated_blocks,duplicated_files,ncloc" -f $Project

try {
    $metricsResp = Invoke-RestMethod -Headers $headers -Uri $metricsUrl -ErrorAction Stop
}
catch {
    Write-Error "Error al obtener metricas: $_"
    exit 1
}

$summary = @{}
foreach ($m in $metricsResp.component.measures) {
    $summary[$m.metric] = $m.value
}

Write-Host ("  Lineas totales       : {0}" -f (Coalesce $summary['ncloc'] '0'))
Write-Host ("  Lineas duplicadas    : {0}" -f (Coalesce $summary['duplicated_lines'] '0'))
Write-Host ("  % duplicacion        : {0}%" -f (Coalesce $summary['duplicated_lines_density'] '0'))
Write-Host ("  Bloques duplicados   : {0}" -f (Coalesce $summary['duplicated_blocks'] '0'))
Write-Host ("  Archivos con duplic. : {0}" -f (Coalesce $summary['duplicated_files'] '0'))

# ---------- 2. Detalle por archivo (paginado) ---------------------------------
Write-Host ""
Write-Host "Obteniendo detalle por archivo..." -ForegroundColor Yellow

$allFiles = New-Object System.Collections.Generic.List[object]
$page     = 1
$pageSize = 500
$total    = $null

do {
    $filesUrl = "https://sonarcloud.io/api/measures/component_tree?component={0}&metricKeys=duplicated_lines,duplicated_lines_density,duplicated_blocks,ncloc&qualifiers=FIL&ps={1}&p={2}" -f $Project, $pageSize, $page
    Write-Host ("  GET pagina {0}..." -f $page) -ForegroundColor DarkGray

    try {
        $r = Invoke-RestMethod -Headers $headers -Uri $filesUrl -ErrorAction Stop
    }
    catch {
        Write-Warning ("Error en pagina {0}: {1}" -f $page, $_)
        break
    }

    if ($null -eq $total) {
        $total = $r.paging.total
        Write-Host ("  {0} archivo(s) encontrados" -f $total) -ForegroundColor Cyan
    }

    foreach ($comp in $r.components) {
        $mMap = @{}
        foreach ($m in $comp.measures) { $mMap[$m.metric] = $m.value }

        $dupLines = [int](Coalesce $mMap['duplicated_lines'] 0)
        if ($dupLines -lt $MinDuplicatedLines) { continue }

        $prefix   = "^" + [regex]::Escape($Project) + ":"
        $filePath = $comp.key -replace $prefix, ""

        $allFiles.Add([PSCustomObject]@{
            file                     = $filePath
            ncloc                    = [int](Coalesce $mMap['ncloc'] 0)
            duplicated_lines         = $dupLines
            duplicated_lines_density = Coalesce $mMap['duplicated_lines_density'] "0"
            duplicated_blocks        = [int](Coalesce $mMap['duplicated_blocks'] 0)
        })
    }

    $page++

} while (($page - 1) * $pageSize -lt $total)

$sorted = $allFiles | Sort-Object duplicated_lines -Descending
Write-Host ("  {0} archivo(s) con duplicaciones en el reporte" -f $sorted.Count) -ForegroundColor Cyan

# ---------- 3. Guardar JSON ---------------------------------------------------
$outputFile = Join-Path $OutDir "duplications.json"

$output = [PSCustomObject]@{
    exportedAt = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    project    = $Project
    summary    = [PSCustomObject]@{
        total_lines              = [int](Coalesce $summary['ncloc'] 0)
        duplicated_lines         = [int](Coalesce $summary['duplicated_lines'] 0)
        duplicated_lines_density = Coalesce $summary['duplicated_lines_density'] "0"
        duplicated_blocks        = [int](Coalesce $summary['duplicated_blocks'] 0)
        duplicated_files         = [int](Coalesce $summary['duplicated_files'] 0)
    }
    files = @($sorted)
}

$output | ConvertTo-Json -Depth 6 | Out-File -FilePath $outputFile -Encoding UTF8
Write-Host ""
Write-Host ("Guardado -> {0}" -f $outputFile) -ForegroundColor Green

# ---------- 4. Top 10 ---------------------------------------------------------
Write-Host ""
Write-Host "=== Top 10 archivos con mas duplicacion ===" -ForegroundColor Green
$sorted | Select-Object -First 10 | ForEach-Object {
    Write-Host ("  {0,6} lineas dup  ({1,5}%)  {2}" -f $_.duplicated_lines, $_.duplicated_lines_density, $_.file)
}