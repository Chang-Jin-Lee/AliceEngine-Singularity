// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Foundation/Time.h
#pragma once

#include "Core.h"

#include <chrono>

namespace alice {

/// 단조 증가 시계. 나노초. 프로파일링·프레임 타이밍에 쓴다.
/// system_clock 은 NTP 보정으로 뒤로 갈 수 있어 계측에 쓰면 안 된다.
ALICE_FORCEINLINE u64 SteadyNanos() noexcept {
    using namespace std::chrono;
    return static_cast<u64>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

ALICE_FORCEINLINE f64 NanosToMillis(u64 ns) noexcept {
    return static_cast<f64>(ns) * 1e-6;
}

ALICE_FORCEINLINE u64 MillisToNanos(f64 ms) noexcept {
    return static_cast<u64>(ms * 1e6);
}

/// 경과 시간 측정용 스톱워치.
class Stopwatch {
public:
    Stopwatch() noexcept : m_start(SteadyNanos()) {}
    void Reset() noexcept { m_start = SteadyNanos(); }
    u64  ElapsedNanos() const noexcept { return SteadyNanos() - m_start; }
    f64  ElapsedMillis() const noexcept { return NanosToMillis(ElapsedNanos()); }
    f64  ElapsedSeconds() const noexcept { return static_cast<f64>(ElapsedNanos()) * 1e-9; }

private:
    u64 m_start;
};

} // namespace alice
