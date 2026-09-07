#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# 커밋 전에 돌리는 단 하나의 명령.
#
# CI 가 검사하는 것과 같은 것을 돌립니다. 여기서 통과하면 CI 에서도 통과합니다.
#
#   ./Scripts/verify.sh              전체
#   ./Scripts/verify.sh --fix        정리 가능한 것은 고쳐가며
#   ./Scripts/verify.sh --skip-build 이미 빌드했다면
#   ./Scripts/verify.sh --json       기계용 결과

set -uo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="build"
SKIP_BUILD=0
FIX=0
JSON=0

while [ $# -gt 0 ]; do
  case "$1" in
    --skip-build) SKIP_BUILD=1 ;;
    --fix)        FIX=1 ;;
    --json)       JSON=1 ;;
    --build-dir)  shift; BUILD_DIR="$1" ;;
    -h|--help)
      echo "사용법: $0 [--skip-build] [--fix] [--json] [--build-dir <경로>]"
      exit 0 ;;
    *) echo "알 수 없는 인자: $1" >&2; exit 2 ;;
  esac
  shift
done

if [ -t 1 ] && [ "$JSON" = "0" ]; then
  R=$'\033[31m'; G=$'\033[32m'; Y=$'\033[33m'; C=$'\033[36m'; D=$'\033[90m'; N=$'\033[0m'
else
  R=""; G=""; Y=""; C=""; D=""; N=""
fi

FAILED=0
JSON_STEPS=""

# UTF-8 한글은 3바이트지만 터미널에서 2칸을 차지한다.
# printf 의 %-28s 는 바이트를 세므로 그대로 쓰면 열이 어긋난다.
#   바이트 = 3k + a,  표시폭 = 2k + a   (k=한글 수, a=ASCII 수)
#   => 표시폭 = 바이트 - k
display_width() {
  local bytes ascii
  bytes=$(printf '%s' "$1" | wc -c)
  # 인쇄 가능한 ASCII(공백~물결)만 남겨 개수를 센다. 역슬래시 이스케이프를 피한 표현이다.
  ascii=$(printf '%s' "$1" | sed 's/[^ -~]//g' | wc -c)
  echo $(( bytes - (bytes - ascii) / 3 ))
}

step() {
  local name="$1"; shift
  local hint="$1"; shift

  if [ "$JSON" = "0" ]; then
    local w pad
    w=$(display_width "$name")
    pad=$(( 26 - w ))
    [ "$pad" -lt 1 ] && pad=1
    printf "  %s%*s" "$name" "$pad" ""
  fi
  local start
  start=$(date +%s%3N 2>/dev/null || echo 0)

  local out ok
  if out=$("$@" 2>&1); then ok=1; else ok=0; fi

  local end elapsed
  end=$(date +%s%3N 2>/dev/null || echo 0)
  elapsed=$((end - start))

  if [ "$JSON" = "0" ]; then
    if [ "$ok" = "1" ]; then
      printf "%sok%s    %6d ms\n" "$G" "$N" "$elapsed"
    else
      printf "%sFAIL%s  %6d ms\n" "$R" "$N" "$elapsed"
      echo "$out" | head -25 | sed "s/^/      ${D}/;s/$/${N}/"
      [ -n "$hint" ] && printf "      %s→ %s%s\n" "$Y" "$hint" "$N"
    fi
  fi

  [ -n "$JSON_STEPS" ] && JSON_STEPS="$JSON_STEPS,"
  JSON_STEPS="$JSON_STEPS{\"name\":\"$name\",\"ok\":$([ "$ok" = "1" ] && echo true || echo false),\"ms\":$elapsed}"
  [ "$ok" = "0" ] && FAILED=$((FAILED + 1))
  return 0
}

if [ "$JSON" = "0" ]; then
  echo
  printf "  %sAliceEngine-Singularity — 커밋 전 검증%s\n\n" "$C" "$N"
fi

# ── 1. 빌드 ────────────────────────────────────────────────────────────────
if [ "$SKIP_BUILD" = "0" ]; then
  step "빌드 (경고 0)" "경고를 고치십시오. --werror 로 재현됩니다." \
    ./Scripts/build.sh --build-dir "$BUILD_DIR" --werror
fi

ALICE="$BUILD_DIR/bin/alice"
TESTS="$BUILD_DIR/bin/Alice.Tests"

if [ ! -x "$ALICE" ]; then
  printf "\n  %s%s 가 없습니다. 먼저 빌드하십시오.%s\n\n" "$R" "$ALICE" "$N"
  exit 1
fi

# ── 2. 테스트 ──────────────────────────────────────────────────────────────
step "테스트" "실패한 케이스 이름으로 --filter 를 걸어 좁히십시오." \
  "$TESTS"

# ── 3. 콘텐츠 검증 ─────────────────────────────────────────────────────────
step "샘플 콘텐츠" "alice check Samples 로 자세히 보십시오. 진단에 고치는 법이 붙어 있습니다." \
  "$ALICE" check Samples

# ── 4. 문서 정규화 ─────────────────────────────────────────────────────────
[ "$FIX" = "1" ] && "$ALICE" fmt Samples >/dev/null 2>&1
step "문서 정규화" "alice fmt Samples 를 돌리고 커밋하십시오. (--fix 로 자동 처리)" \
  "$ALICE" fmt Samples --check

# ── 5. 생성물이 최신인지 ───────────────────────────────────────────────────
check_schemas() {
  "$ALICE" schema emit Schemas >/dev/null 2>&1
  local diff
  diff=$(git diff --name-only -- Schemas/ 2>/dev/null)
  if [ -n "$diff" ]; then echo "$diff"; return 1; fi
  return 0
}
step "JSON Schema 최신" "alice schema emit Schemas 결과를 커밋하십시오." check_schemas

# ── 6. 백로그 표 ───────────────────────────────────────────────────────────
step "백로그 표" "Agents/README.md 의 표를 실제 상태에 맞추십시오." \
  python Scripts/backlog.py --check

# ── 결과 ───────────────────────────────────────────────────────────────────
if [ "$JSON" = "1" ]; then
  printf '{"ok":%s,"failed":%d,"steps":[%s]}\n' \
    "$([ "$FAILED" = "0" ] && echo true || echo false)" "$FAILED" "$JSON_STEPS"
else
  echo
  if [ "$FAILED" = "0" ]; then
    printf "  %s전부 통과. 커밋해도 됩니다.%s\n\n" "$G" "$N"
    printf "  %s커밋 메시지에 Agent: 와 Task: 를 넣는 것을 잊지 마십시오.%s\n\n" "$D" "$N"
  else
    printf "  %s%d개 실패.%s\n\n" "$R" "$FAILED" "$N"
  fi
fi

[ "$FAILED" = "0" ] && exit 0 || exit 1
