# SPDX-License-Identifier: MIT
#
# 커밋 전에 돌리는 단 하나의 명령.
#
# CI 가 검사하는 것과 같은 것을 돌립니다. 여기서 통과하면 CI 에서도 통과합니다.
# 반대로 여기서 실패하는 것을 밀어 올리면 다른 사람의 시간을 씁니다.
#
#   .\Scripts\verify.ps1              전체
#   .\Scripts\verify.ps1 -Fix         정리 가능한 것은 고쳐가며
#   .\Scripts\verify.ps1 -SkipBuild   이미 빌드했다면

[CmdletBinding()]
param(
    [string]$BuildDir = 'build',
    [switch]$SkipBuild,
    [switch]$Fix,
    [switch]$Json
)

$ErrorActionPreference = 'Continue'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

$script:results = @()
$script:failed = 0

# 한글은 터미널에서 2칸을 차지하지만 .NET 은 1문자로 센다. 표시 폭으로 보정한다.
function Get-DisplayWidth {
    param([string]$Text)
    $width = 0
    foreach ($ch in $Text.ToCharArray()) {
        $width += if ([int]$ch -gt 0x1100) { 2 } else { 1 }
    }
    return $width
}

function Step {
    param([string]$Name, [scriptblock]$Body, [string]$Hint = '')

    if (-not $Json) {
        $pad = [Math]::Max(1, 26 - (Get-DisplayWidth $Name))
        Write-Host ("  $Name" + (' ' * $pad)) -NoNewline
    }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $ok = $false
    $detail = ''
    try {
        $detail = & $Body
        $ok = $?  -and ($LASTEXITCODE -eq 0 -or $null -eq $LASTEXITCODE)
    } catch {
        $ok = $false
        $detail = $_.Exception.Message
    }
    $sw.Stop()

    if (-not $Json) {
        if ($ok) {
            Write-Host ("ok    {0,6:N0} ms" -f $sw.ElapsedMilliseconds) -ForegroundColor Green
        } else {
            Write-Host ("FAIL  {0,6:N0} ms" -f $sw.ElapsedMilliseconds) -ForegroundColor Red
            if ($detail) { $detail | Select-Object -First 25 | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkGray } }
            if ($Hint)   { Write-Host "      → $Hint" -ForegroundColor Yellow }
        }
    }

    $script:results += [pscustomobject]@{ name = $Name; ok = $ok; ms = $sw.ElapsedMilliseconds }
    if (-not $ok) { $script:failed++ }
}

if (-not $Json) {
    Write-Host ''
    Write-Host '  AliceEngine-Singularity — 커밋 전 검증' -ForegroundColor Cyan
    Write-Host ''
}

# ── 1. 빌드 ────────────────────────────────────────────────────────────────
if (-not $SkipBuild) {
    Step '빌드 (경고 0)' {
        & "$PSScriptRoot\build.ps1" -BuildDir $BuildDir -WarningsAsErrors 2>&1
    } '경고를 고치십시오. -DALICE_WARNINGS_AS_ERRORS=ON 으로 재현됩니다.'
}

$alice = Join-Path $repo "$BuildDir/bin/alice.exe"
$tests = Join-Path $repo "$BuildDir/bin/Alice.Tests.exe"

if (-not (Test-Path $alice)) {
    Write-Host ''
    Write-Host "  $alice 가 없습니다. 먼저 빌드하십시오." -ForegroundColor Red
    exit 1
}

# ── 2. 테스트 ──────────────────────────────────────────────────────────────
Step '테스트' {
    & $tests 2>&1
} '실패한 케이스 이름으로 --filter 를 걸어 좁히십시오.'

# ── 3. 콘텐츠 검증 ─────────────────────────────────────────────────────────
Step '샘플 콘텐츠' {
    & $alice check Samples 2>&1
} 'alice check Samples 로 자세히 보십시오. 진단에 고치는 법이 붙어 있습니다.'

# ── 4. 문서 정규화 ─────────────────────────────────────────────────────────
if ($Fix) {
    & $alice fmt Samples | Out-Null
}
Step '문서 정규화' {
    & $alice fmt Samples --check 2>&1
} 'alice fmt Samples 를 돌리고 커밋하십시오. (-Fix 로 자동 처리)'

# ── 5. 생성물이 최신인지 ───────────────────────────────────────────────────
Step 'JSON Schema 최신' {
    & $alice schema emit Schemas | Out-Null
    $diff = git diff --name-only -- Schemas/ 2>&1
    if ($diff) { $LASTEXITCODE = 1; $diff } else { $LASTEXITCODE = 0 }
} 'alice schema emit Schemas 결과를 커밋하십시오.'

# ── 6. 백로그 표 ───────────────────────────────────────────────────────────
Step '백로그 표' {
    python Scripts/backlog.py --check 2>&1
} 'Agents/README.md 의 표를 실제 상태에 맞추십시오.'

# ── 결과 ───────────────────────────────────────────────────────────────────
if ($Json) {
    [pscustomobject]@{
        ok      = ($script:failed -eq 0)
        failed  = $script:failed
        steps   = $script:results
    } | ConvertTo-Json -Depth 4
} else {
    Write-Host ''
    if ($script:failed -eq 0) {
        Write-Host '  전부 통과. 커밋해도 됩니다.' -ForegroundColor Green
        Write-Host ''
        Write-Host '  커밋 메시지에 Agent: 와 Task: 를 넣는 것을 잊지 마십시오.' -ForegroundColor DarkGray
    } else {
        Write-Host "  $($script:failed)개 실패." -ForegroundColor Red
    }
    Write-Host ''
}

exit $(if ($script:failed -eq 0) { 0 } else { 1 })
