// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Foundation/Profiler.h
//
// 프로파일러는 나중에 붙이는 도구가 아니라 **처음부터 켜져 있는 계측 계층**이다.
//
// 두 종류의 소비자가 있고, 둘이 원하는 모양이 정반대다.
//   • 사람  : 프레임 하나를 눈으로 훑고 싶다 → 플레임 그래프. 클릭하면 아래로 내려간다.
//   • AI   : "지금 뭐가 제일 비싼가"를 한 번의 질의로 알고 싶다 → 정렬된 JSON.
// 그래서 같은 이벤트 스트림에서 두 표현을 모두 뽑는다. 계측을 두 번 하지 않는다.
//
// 예산(Budget) 이 이 설계의 핵심이다. 존마다 프레임 예산을 선언해두면, 초과하는 순간
// 구조화 로그 "perf.budget.exceeded" 가 스택과 함께 나간다. 그러면 "게임이 느려요" 라는
// 사람의 말이 기계가 처리할 수 있는 이벤트로 바뀐다. AI 는 로그만 보고 범인을 짚는다.
//
//   ALICE_PROFILE_ZONE("Render.Shadow");            // 스코프 존
//   Profiler::SetBudgetMs("Render.Shadow", 2.0);    // 예산 선언
//
#pragma once

#include "Core.h"

#include <string>
#include <string_view>
#include <vector>

namespace alice {

/// 존 하나의 원시 기록. name 은 **정적 문자열 리터럴**이어야 한다(복사하지 않는다).
struct ZoneRecord {
    const char* name     = "";
    u64         beginNs  = 0;
    u64         endNs    = 0;
    u32         depth    = 0;
    i32         parent   = -1;   ///< 같은 프레임/스레드 버퍼 안의 인덱스
    u32         threadId = 0;
};

/// 한 프레임의 전체 기록.
struct FrameData {
    u64                     frameIndex = 0;
    u64                     beginNs    = 0;
    u64                     endNs      = 0;
    std::vector<ZoneRecord> zones;      ///< 스레드별 버퍼를 이어 붙인 것

    u64 DurationNs() const noexcept { return endNs > beginNs ? endNs - beginNs : 0; }
};

/// 이름 단위로 접은 집계. AI 질의와 회귀 게이트가 이걸 본다.
struct ZoneAggregate {
    std::string name;
    u64  totalNs = 0;   ///< 자식 포함
    u64  selfNs  = 0;   ///< 자식 제외 — 진짜 범인을 찾을 때 보는 값
    u64  maxNs   = 0;
    u32  calls   = 0;

    f64 TotalMs() const noexcept { return static_cast<f64>(totalNs) * 1e-6; }
    f64 SelfMs()  const noexcept { return static_cast<f64>(selfNs)  * 1e-6; }
};

/// 최근 N프레임의 프레임 시간 통계.
struct FrameStats {
    u32 sampleCount = 0;
    f64 avgMs = 0.0;
    f64 minMs = 0.0;
    f64 maxMs = 0.0;
    f64 p50Ms = 0.0;
    f64 p95Ms = 0.0;
    f64 p99Ms = 0.0;   ///< "1% low" 와 같은 이야기. 체감 끊김은 평균이 아니라 여기서 온다.
};

class Profiler {
public:
    /// 프로파일러를 켠다. 끄면 존 매크로의 비용이 사실상 0이 된다.
    static void SetEnabled(bool enabled) noexcept;
    static bool IsEnabled() noexcept;

    /// 보관할 프레임 수. 기본 240 (4초 @60fps).
    static void SetHistoryFrames(u32 count);

    static void BeginFrame(u64 frameIndex);
    static void EndFrame();

    // 존 — 직접 호출보다 ALICE_PROFILE_ZONE 매크로를 쓴다.
    static void BeginZone(const char* name) noexcept;
    static void EndZone() noexcept;

    /// RHI 가 해석 완료된 GPU 타임스탬프를 되먹인다.
    /// GPU 는 몇 프레임 뒤에야 결과가 나오므로 프레임 인덱스를 함께 받는다.
    static void SubmitGpuZone(u64 frameIndex, const char* name, u64 durationNs);

    // ── 질의 ───────────────────────────────────────────────────────────────
    /// 마지막으로 완료된 프레임.
    static bool LatestFrame(FrameData& out);
    static bool FrameByIndex(u64 frameIndex, FrameData& out);

    /// 최근 프레임들을 이름으로 접어 selfNs 내림차순 정렬.
    static std::vector<ZoneAggregate> Aggregate(u32 frameCount = 1);
    static FrameStats                 Stats(u32 frameCount = 0);   ///< 0 = 보관 중 전부

    // ── 예산 ───────────────────────────────────────────────────────────────
    /// 존의 프레임 예산(밀리초). 초과하면 "perf.budget.exceeded" 로그가 나간다.
    static void SetBudgetMs(std::string_view zoneName, f64 budgetMs);
    static void ClearBudgets();
    /// 이번 실행에서 예산을 넘긴 횟수. CI 회귀 게이트가 읽는다.
    static u64  BudgetViolationCount() noexcept;

    // ── 내보내기 ───────────────────────────────────────────────────────────
    /// chrome://tracing / Perfetto 가 여는 형식. **사람이 플레임 그래프로 본다.**
    static bool WriteChromeTrace(const std::string& path, u32 frameCount = 60);

    /// **AI가 읽는 형식.** 한 방에 "무엇이 비싼가"를 답한다.
    /// { "frame": n, "frameMs": .., "stats": {..}, "hotspots": [ {name, selfMs, totalMs, calls}, .. ] }
    static std::string HotspotsJson(u32 topN = 20, u32 frameCount = 1);

    /// 전체 초기화. 테스트에서 상태 격리에 쓴다.
    static void Reset();
};

/// 스코프 존. 생성/소멸에서 Begin/End 를 부른다.
class ScopedZone {
public:
    ALICE_FORCEINLINE explicit ScopedZone(const char* name) noexcept { Profiler::BeginZone(name); }
    ALICE_FORCEINLINE ~ScopedZone() noexcept { Profiler::EndZone(); }
    ScopedZone(const ScopedZone&) = delete;
    ScopedZone& operator=(const ScopedZone&) = delete;
};

} // namespace alice

// 존 이름은 반드시 리터럴이어야 한다. 런타임 문자열을 넣으면 수명이 깨진다.
#define ALICE_PROFILE_ZONE(nameLiteral) \
    ::alice::ScopedZone ALICE_UNIQUE(aliceZone_)(nameLiteral)

/// 함수 이름을 그대로 쓰는 축약형.
#define ALICE_PROFILE_FUNC() ALICE_PROFILE_ZONE(__func__)
