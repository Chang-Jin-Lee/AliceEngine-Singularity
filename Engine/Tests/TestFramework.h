// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Tests/TestFramework.h
//
// 외부 의존성 없는 최소 테스트 러너.
// 왜 직접 두는가: 이 엔진은 "클론하면 바로 빌드된다"를 지켜야 한다. 참고한 엔진 대부분이
// 서드파티 하나 때문에 첫 빌드에서 막혔다. 테스트 프레임워크는 200줄이면 충분하다.
//
// 출력은 두 가지다.
//   • 사람  : "ok / FAILED" 목록
//   • CI/AI : --json 을 주면 결과가 JSON 으로 나온다
#pragma once

#include "Foundation/StringUtil.h"

#include <functional>
#include <string>
#include <vector>

namespace alice::test {

struct Failure {
    std::string expression;
    std::string detail;
    std::string file;
    u32         line = 0;
};

struct CaseResult {
    std::string          suite;
    std::string          name;
    bool                 passed = true;
    f64                  ms = 0.0;
    std::vector<Failure> failures;
};

/// 테스트 본문에서 실패를 기록하는 창구.
class Context {
public:
    void Fail(std::string expression, std::string detail, const char* file, u32 line);
    bool Failed() const noexcept { return !m_failures.empty(); }
    const std::vector<Failure>& Failures() const noexcept { return m_failures; }

private:
    std::vector<Failure> m_failures;
};

using CaseFn = std::function<void(Context&)>;

/// 정적 등록. 링커가 전역 초기화 순서를 어떻게 잡든 안전하도록 함수 지역 정적을 쓴다.
class Registry {
public:
    struct Entry {
        std::string suite;
        std::string name;
        CaseFn      fn;
    };

    static Registry& Instance();
    void Add(std::string suite, std::string name, CaseFn fn);
    const std::vector<Entry>& Entries() const noexcept { return m_entries; }

private:
    std::vector<Entry> m_entries;
};

struct Registrar {
    Registrar(const char* suite, const char* name, CaseFn fn) {
        Registry::Instance().Add(suite, name, std::move(fn));
    }
};

/// 전체 실행. filter 가 비어있지 않으면 "suite.name" 에 부분일치하는 것만 돌린다.
/// 반환값은 프로세스 종료 코드(0 = 전부 통과).
int RunAll(const std::string& filter, bool json);

/// argv 를 해석해 RunAll 을 호출한다. main 에서 그대로 쓰면 된다.
int Main(int argc, char** argv);

} // namespace alice::test

// ── 매크로 ─────────────────────────────────────────────────────────────────
#define ALICE_TEST(suite, name)                                                        \
    static void ALICE_CONCAT(aliceTest_, __LINE__)(::alice::test::Context&);            \
    static ::alice::test::Registrar ALICE_CONCAT(aliceTestReg_, __LINE__)(              \
        #suite, #name, &ALICE_CONCAT(aliceTest_, __LINE__));                            \
    static void ALICE_CONCAT(aliceTest_, __LINE__)(::alice::test::Context& aliceCtx)

#define ALICE_CHECK(expr)                                                              \
    do {                                                                               \
        if (!(expr)) aliceCtx.Fail(#expr, "거짓으로 평가됨", __FILE__, __LINE__);         \
    } while (false)

#define ALICE_CHECK_MSG(expr, msg)                                                     \
    do {                                                                               \
        if (!(expr)) aliceCtx.Fail(#expr, (msg), __FILE__, __LINE__);                  \
    } while (false)

#define ALICE_CHECK_EQ(a, b)                                                           \
    do {                                                                               \
        const auto& aliceLhs_ = (a);                                                   \
        const auto& aliceRhs_ = (b);                                                   \
        if (!(aliceLhs_ == aliceRhs_)) {                                               \
            std::string aliceDetail_;                                                  \
            ::alice::detail::FormatAppend(aliceDetail_, aliceLhs_);                    \
            aliceDetail_ += "  !=  ";                                                  \
            ::alice::detail::FormatAppend(aliceDetail_, aliceRhs_);                    \
            aliceCtx.Fail(#a " == " #b, aliceDetail_, __FILE__, __LINE__);             \
        }                                                                              \
    } while (false)

/// 문자열 비교 전용. FormatAppend 오버로드가 없는 타입에 쓰지 않는다.
#define ALICE_CHECK_STR(a, b)                                                          \
    do {                                                                               \
        const std::string aliceLhs_ = (a);                                             \
        const std::string aliceRhs_ = (b);                                             \
        if (aliceLhs_ != aliceRhs_) {                                                  \
            aliceCtx.Fail(#a " == " #b,                                                \
                          "\n    실제: " + aliceLhs_ + "\n    기대: " + aliceRhs_,      \
                          __FILE__, __LINE__);                                         \
        }                                                                              \
    } while (false)

/// 실패 시 이후 검사를 이어가면 의미가 없을 때. 케이스를 즉시 끝낸다.
#define ALICE_REQUIRE(expr)                                                            \
    do {                                                                               \
        if (!(expr)) {                                                                 \
            aliceCtx.Fail(#expr, "필수 조건 실패 — 케이스 중단", __FILE__, __LINE__);     \
            return;                                                                    \
        }                                                                              \
    } while (false)

/// 메시지를 붙인 REQUIRE. 한 줄로 둔다 (줄 이음 백슬래시는 편집 도구를 자주 탄다).
#define ALICE_REQUIRE_MSG(expr, msg) do { if (!(expr)) { aliceCtx.Fail(#expr, (msg), __FILE__, __LINE__); return; } } while (false)
