[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$BuildType = "Release",

    [switch]$SkipBuild,

    [string]$SourceExePath,

    [string]$InstallDir = "$env:LOCALAPPDATA\Programs\MiFutbolC",

    [ValidateSet("user", "none")]
    [string]$PathMode = "user",

    [switch]$RunAfterInstall
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Step {
    param([string]$Message)
    Write-Output "`n==> $Message"
}

function New-DirectoryIfMissing {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Resolve-MakeCommand {
    $candidates = @("mingw32-make", "make")
    foreach ($candidate in $candidates) {
        $cmd = Get-Command -Name $candidate -ErrorAction SilentlyContinue
        if ($null -ne $cmd) {
            return $cmd.Source
        }
    }
    return $null
}

function Invoke-Build {
    param(
        [string]$RepoRoot,
        [string]$RequestedBuildType
    )

    $makeExe = Resolve-MakeCommand
    if ([string]::IsNullOrWhiteSpace($makeExe)) {
        throw "No se encontro 'mingw32-make' ni 'make' en PATH. Instale MinGW/CodeBlocks o use -SkipBuild con un .exe ya compilado."
    }

    Write-Step "Compilando proyecto con $makeExe (BUILD_TYPE=$RequestedBuildType)"
    Push-Location $RepoRoot
    try {
        & $makeExe "BUILD_TYPE=$RequestedBuildType"
        if ($LASTEXITCODE -ne 0) {
            throw "La compilacion finalizo con codigo $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

function Resolve-ExePath {
    param(
        [string]$RepoRoot,
        [string]$CustomPath
    )

    if (-not [string]::IsNullOrWhiteSpace($CustomPath)) {
        $resolvedCustom = Resolve-Path -LiteralPath $CustomPath -ErrorAction Stop
        return $resolvedCustom.Path
    }

    $candidates = @(
        (Join-Path $RepoRoot "MiFutbolC.exe"),
        (Join-Path $RepoRoot "bin\Debug\MiFutbolC.exe")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "No se encontro MiFutbolC.exe. Compile primero o indique -SourceExePath."
}

function Add-UserPathEntry {
    param([string]$Directory)

    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ([string]::IsNullOrWhiteSpace($currentPath)) {
        [Environment]::SetEnvironmentVariable("Path", $Directory, "User")
        return $true
    }

    $normalizedTarget = $Directory.TrimEnd('\\')
    $parts = $currentPath.Split(';') | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    foreach ($part in $parts) {
        if ($part.TrimEnd('\\') -ieq $normalizedTarget) {
            return $false
        }
    }

    $newPath = ($parts + $Directory) -join ';'
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    return $true
}

function Copy-IfExists {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (Test-Path -LiteralPath $Source) {
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
    }
}

if ($env:OS -ne "Windows_NT") {
    throw "Este script esta pensado para Windows. En Linux/macOS use Instalador-Linux.sh."
}

$scriptPath = Resolve-Path -LiteralPath $PSCommandPath
$repoRoot = Split-Path -Parent $scriptPath.Path

Write-Step "Instalando MiFutbolC desde $repoRoot"

if (-not $SkipBuild) {
    Invoke-Build -RepoRoot $repoRoot -RequestedBuildType $BuildType
}
else {
    Write-Step "Compilacion omitida (-SkipBuild)"
}

$exePath = Resolve-ExePath -RepoRoot $repoRoot -CustomPath $SourceExePath
Write-Step "Binario origen: $exePath"

$resolvedInstallDir = [IO.Path]::GetFullPath($InstallDir)
$installExePath = Join-Path $resolvedInstallDir "MiFutbolC.exe"
$installCmdPath = Join-Path $resolvedInstallDir "MiFutbolC.cmd"

Write-Step "Copiando archivos a $resolvedInstallDir"
New-DirectoryIfMissing -Path $resolvedInstallDir
New-DirectoryIfMissing -Path (Join-Path $resolvedInstallDir "Musica")

Copy-Item -LiteralPath $exePath -Destination $installExePath -Force

Copy-IfExists -Source (Join-Path $repoRoot "MiFutbolC.ico") -Destination (Join-Path $resolvedInstallDir "MiFutbolC.ico")
Copy-IfExists -Source (Join-Path $repoRoot "Manual_Usuario_MiFutbolC.pdf") -Destination (Join-Path $resolvedInstallDir "Manual_Usuario_MiFutbolC.pdf")
Copy-IfExists -Source (Join-Path $repoRoot "README.pdf") -Destination (Join-Path $resolvedInstallDir "README.pdf")
Copy-IfExists -Source (Join-Path $repoRoot "LICENSE") -Destination (Join-Path $resolvedInstallDir "LICENSE")
Copy-IfExists -Source (Join-Path $repoRoot "README.md") -Destination (Join-Path $resolvedInstallDir "README.md")

# Copiar archivos de idioma
$langsSourceDir = Join-Path $repoRoot "langs"
$langsTargetDir = Join-Path $resolvedInstallDir "langs"
if (Test-Path -LiteralPath $langsSourceDir) {
    New-DirectoryIfMissing -Path $langsTargetDir
    Copy-Item -Path (Join-Path $langsSourceDir "*") -Destination $langsTargetDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Output "Archivos de idioma copiados a $langsTargetDir"
}

$musicSourceDir = Join-Path $repoRoot "Musica"
$musicTargetDir = Join-Path $resolvedInstallDir "Musica"
if (Test-Path -LiteralPath $musicSourceDir) {
    Copy-Item -Path (Join-Path $musicSourceDir "*") -Destination $musicTargetDir -Recurse -Force -ErrorAction SilentlyContinue
}

# Launcher para ejecutar desde consola y forzar working directory del instalador.
$launcherContent = @(
    "@echo off",
    "setlocal",
    "cd /d %~dp0",
    '"%~dp0MiFutbolC.exe" %*'
)
Set-Content -LiteralPath $installCmdPath -Value $launcherContent -Encoding Ascii

$dataDir = Join-Path $env:LOCALAPPDATA "MiFutbolC\data"
New-DirectoryIfMissing -Path $dataDir

if ($PathMode -eq "user") {
    $pathUpdated = Add-UserPathEntry -Directory $resolvedInstallDir
    if ($pathUpdated) {
        Write-Output "Se agrego al PATH de usuario: $resolvedInstallDir"
        Write-Output "Abra una nueva consola para usar el comando: MiFutbolC"
    }
    else {
        Write-Output "El PATH de usuario ya contiene: $resolvedInstallDir"
    }
}
else {
    Write-Output "No se modifico PATH (PathMode=none)."
}

Write-Output "`nInstalacion completada."
Write-Output "Ejecutable: $installExePath"
Write-Output "Launcher consola: $installCmdPath"

if ($RunAfterInstall) {
    Write-Step "Iniciando MiFutbolC"
    Start-Process -FilePath $installCmdPath -WorkingDirectory $resolvedInstallDir
}
