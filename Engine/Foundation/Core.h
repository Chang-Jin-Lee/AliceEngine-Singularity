// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Foundation/Core.h
//
// 엔진 전체가 공유하는 최소 기반. 여기에는 "모든 번역 단위가 포함해도 되는 것"만 둔다.
// 무거운 헤더(<vector>, <string>)조차 여기서는 최소화한다.
#pragma once

#include <cstddef>
#include <cstdint>

// ── 플랫폼 판별 ────────────────────────────────────────────────────────────
#if defined(_WIN32)
#  define ALICE_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
#  include <TargetConditionals.h>
#  if TARGET_OS_IPHONE
#    define ALICE_PLATFORM_IOS 1
#  else
#    define ALICE_PLATFORM_MACOS 1
#  endif
#elif defined(__ANDROID__)
#  define ALICE_PLATFORM_ANDROID 1
#elif defined(__linux__)
#  define ALICE_PLATFORM_LINUX 1
#endif

#if !defined(ALICE_PLATFORM_WINDOWS)
#  define ALICE_PLATFORM_WINDOWS 0
#endif
#if !defined(ALICE_PLATFORM_MACOS)
#  define ALICE_PLATFORM_MACOS 0
#endif
#if !defined(ALICE_PLATFORM_IOS)
#  define ALICE_PLATFORM_IOS 0
#endif
#if !defined(ALICE_PLATFORM_ANDROID)
#  define ALICE_PLATFORM_ANDROID 0
#endif
#if !defined(ALICE_PLATFORM_LINUX)
#  define ALICE_PLATFORM_LINUX 0
#endif

#define ALICE_PLATFORM_APPLE  (ALICE_PLATFORM_MACOS || ALICE_PLATFORM_IOS)
#define ALICE_PLATFORM_MOBILE (ALICE_PLATFORM_IOS || ALICE_PLATFORM_ANDROID)

// ── 컴파일러 부속 ──────────────────────────────────────────────────────────
#if defined(_MSC_VER)
#  define ALICE_FORCEINLINE __forceinline
#  define ALICE_NOINLINE    __declspec(noinline)
#  define ALICE_DEBUGBREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#  define ALICE_FORCEINLINE inline __attribute__((always_inline))
#  define ALICE_NOINLINE    __attribute__((noinline))
#  if defined(__clang__)
#    define ALICE_DEBUGBREAK() __builtin_debugtrap()
#  else
#    define ALICE_DEBUGBREAK() __builtin_trap()
#  endif
#else
#  define ALICE_FORCEINLINE inline
#  define ALICE_NOINLINE
#  define ALICE_DEBUGBREAK() ((void)0)
#endif

#define ALICE_UNUSED(x) ((void)(x))

// 토큰 결합 — 스코프 매크로가 고유 변수명을 만들 때 쓴다.
#define ALICE_CONCAT_IMPL(a, b) a##b
#define ALICE_CONCAT(a, b) ALICE_CONCAT_IMPL(a, b)
#define ALICE_UNIQUE(prefix) ALICE_CONCAT(prefix, __LINE__)

namespace alice {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;
using usize = std::size_t;
using isize = std::ptrdiff_t;

/// 소스 위치. std::source_location 은 툴체인 편차가 커서 직접 들고 다닌다.
/// 로그·진단 레코드에 그대로 실려 AI가 "어느 줄에서 났는지"를 파싱할 수 있게 한다.
struct SourceLoc {
    const char* file = "";
    const char* func = "";
    u32         line = 0;
};

#define ALICE_HERE (::alice::SourceLoc{__FILE__, static_cast<const char*>(__func__), static_cast<::alice::u32>(__LINE__)})

/// 경로에서 파일명만 뽑는다(로그 축약용). 컴파일 타임에 접히기를 기대한다.
constexpr const char* ShortFile(const char* path) noexcept {
    const char* last = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') last = p + 1;
    }
    return last;
}

/// 복사 금지 믹스인.
struct NonCopyable {
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

/// 스코프 종료 시 호출자를 실행. Defer 패턴.
template <typename F>
class ScopeGuard {
public:
    explicit ScopeGuard(F&& fn) : m_fn(static_cast<F&&>(fn)) {}
    ~ScopeGuard() { if (m_active) m_fn(); }
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    void Dismiss() noexcept { m_active = false; }
private:
    F    m_fn;
    bool m_active = true;
};

template <typename F>
ScopeGuard<F> MakeScopeGuard(F&& fn) { return ScopeGuard<F>(static_cast<F&&>(fn)); }

#define ALICE_DEFER(...) auto ALICE_UNIQUE(aliceDefer_) = ::alice::MakeScopeGuard([&]() { __VA_ARGS__; })

/// 어서션 실패 보고. 로그를 거치지 않는다(로거 자신이 쓸 수 있어야 하므로).
[[noreturn]] void AssertFailed(const char* expr, SourceLoc loc, const char* message) noexcept;

} // namespace alice

// ALICE_ASSERT 는 릴리스에서도 남는다. 성능 임계 경로에는 ALICE_ASSERT_DEBUG 를 쓴다.
#define ALICE_ASSERT(expr, msg)                                                    \
    do {                                                                           \
        if (!(expr)) { ::alice::AssertFailed(#expr, ALICE_HERE, (msg)); }          \
    } while (false)

#if defined(ALICE_DEBUG)
#  define ALICE_ASSERT_DEBUG(expr, msg) ALICE_ASSERT(expr, msg)
#else
#  define ALICE_ASSERT_DEBUG(expr, msg) ((void)0)
#endif
