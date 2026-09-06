# SPDX-License-Identifier: MIT
# Windows 빌드. Visual Studio 개발자 환경을 알아서 찾아 들어간다.
#
# 이 스크립트가 흡수하는 이 환경의 함정들:
#   - Ninja + MSVC 는 vcvars 환경이 필요하다
#   - Windows SDK 28000 의 DirectXCollision 개명 → 26100 으로 고정 (CMakeLists 에서)
#   - CP949 로케일에서 C4819 → /utf-8 (AliceModule.cmake 에서)

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'RelWithDebInfo',

    [string]$BuildDir = 'build',
    [switch]$Clean,
    [switch]$Test,
    [switch]$NoTools,
    [switch]$WarningsAsErrors
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

function Find-VsDevShell {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        $vswhere = "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
    }
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe 를 찾을 수 없다. Visual Studio 가 설치되어 있는지 확인하라."
    }
    $install = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $install) {
        throw "C++ 워크로드가 설치된 Visual Studio 를 찾을 수 없다."
    }
    return Join-Path $install 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
}

if ($IsWindows -or $env:OS -eq 'Windows_NT') {
    if (-not $env:VSCMD_VER) {
        Write-Host '  Visual Studio 개발자 환경으로 들어간다...' -ForegroundColor DarkGray
        Import-Module (Find-VsDevShell)
        Enter-VsDevShell -VsInstallPath (Split-Path -Parent (Split-Path -Parent (Find-VsDevShell))) `
            -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
        Set-Location $repo
    }
}

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "  $BuildDir 를 지운다" -ForegroundColor DarkGray
    Remove-Item -Recurse -Force $BuildDir
}

$cmakeArgs = @(
    '-S', '.', '-B', $BuildDir,
    '-G', 'Ninja',
    "-DCMAKE_BUILD_TYPE=$Config"
)
if ($NoTools)          { $cmakeArgs += '-DALICE_BUILD_TOOLS=OFF' }
if ($WarningsAsErrors) { $cmakeArgs += '-DALICE_WARNINGS_AS_ERRORS=ON' }

cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw 'CMake 설정 실패' }

cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) { throw '빌드 실패' }

Write-Host ''
Write-Host '  빌드 완료' -ForegroundColor Green
Write-Host "    $BuildDir/bin/alice.exe" -ForegroundColor DarkGray
Write-Host "    $BuildDir/bin/Alice.Tests.exe" -ForegroundColor DarkGray

if ($Test) {
    Write-Host ''
    & "$BuildDir/bin/Alice.Tests.exe"
    if ($LASTEXITCODE -ne 0) { throw '테스트 실패' }

    & "$BuildDir/bin/alice.exe" check Samples
    if ($LASTEXITCODE -ne 0) { throw '샘플 검증 실패' }
}
