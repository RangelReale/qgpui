<#
.SYNOPSIS
Builds the QGPUI installer from a configured and built CMake tree.

.DESCRIPTION
Installs the build into a staging folder (cmake --install), adds the catalog when one is given,
and runs Inno Setup's compiler on qgpui.iss. The version comes from the project() call in
CMakeLists.txt, as CMake cached it. Prints the installer's path.

.EXAMPLE
packaging/windows/build-installer.ps1 -BuildDir build/windows -Catalog local/qgpui-catalog.json
#>
param(
    [Parameter(Mandatory)] [string] $BuildDir,
    [string] $Config = 'Release',
    [string] $StageDir = 'dist/stage',
    [string] $OutputDir = 'dist',
    [string] $Catalog = ''
)

$ErrorActionPreference = 'Stop'

$cache = Join-Path $BuildDir 'CMakeCache.txt'
$line = Select-String -Path $cache -Pattern '^CMAKE_PROJECT_VERSION:STATIC=(.+)$' | Select-Object -First 1
if (-not $line) { throw "No project version in $cache. Configure the build first." }
$version = $line.Matches[0].Groups[1].Value

if (Test-Path $StageDir) { Remove-Item -Recurse -Force $StageDir }
cmake --install $BuildDir --config $Config --prefix $StageDir | Out-Host
if ($LASTEXITCODE -ne 0) { throw "cmake --install failed ($LASTEXITCODE)" }

foreach ($file in 'qgpui.exe', 'Qt6Core.dll', 'platforms/qwindows.dll', 'sqldrivers/qsqlite.dll',
                  'vcruntime140.dll', 'platform-tools/adb.exe') {
    if (-not (Test-Path (Join-Path $StageDir $file))) { throw "The staged app has no $file." }
}

if ($Catalog) {
    Copy-Item $Catalog (Join-Path $StageDir 'qgpui-catalog.json')
}

$iscc = (Get-Command iscc.exe -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
    $iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
              "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") |
        Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $iscc) { throw 'Inno Setup 6 (ISCC.exe) not found.' }

New-Item -ItemType Directory -Force $OutputDir | Out-Null
$script = Join-Path $PSScriptRoot 'qgpui.iss'
& $iscc /Q "/DAppVersion=$version" "/DStageDir=$((Resolve-Path $StageDir).Path)" `
    "/DOutputDir=$((Resolve-Path $OutputDir).Path)" $script | Out-Host
if ($LASTEXITCODE -ne 0) { throw "ISCC failed ($LASTEXITCODE)" }

Join-Path (Resolve-Path $OutputDir).Path "qgpui-$version-setup.exe"
