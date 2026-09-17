# SPDX-License-Identifier: MIT
# Launch the native editor from any working directory, building on first use.
[CmdletBinding()]
param(
    [string]$Project = '',
    [switch]$Build,
    [switch]$SmokeTest
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$editor = Join-Path $repo 'build/bin/Alice.Editor.exe'
if ($env:OS -ne 'Windows_NT') { throw 'The native editor currently requires Windows.' }
if ($Build -or -not (Test-Path -LiteralPath $editor)) {
    & "$PSScriptRoot/build.ps1" -WarningsAsErrors
    if ($LASTEXITCODE -ne 0) { throw 'Editor build failed.' }
}
if (-not $Project) { $Project = Join-Path $repo 'Samples/CubePlayground' }
$projectPath = (Resolve-Path -LiteralPath $Project).Path
if (-not (Test-Path -LiteralPath $projectPath -PathType Container)) { throw 'Project must be a folder.' }
$editorArgs = @('--project', ('"{0}"' -f $projectPath))
if ($SmokeTest) { $editorArgs += '--smoke-test' }
# A visible window is the requested product; the smoke test also exercises real native controls.
$process = Start-Process -FilePath $editor -ArgumentList $editorArgs -WorkingDirectory $repo -PassThru
if ($SmokeTest) {
    if (-not $process.WaitForExit(20000)) {
        Stop-Process -Id $process.Id
        throw 'Editor smoke test timed out.'
    }
    $process.Refresh()
    Get-Content -LiteralPath (Join-Path (Split-Path $editor) 'editor-smoke.json') -Encoding UTF8
    if ($process.ExitCode -ne 0) { throw 'Editor smoke test failed.' }
}
