// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Foundation/Result.h
//
// 예외를 쓰지 않는다. 콘솔·모바일 타깃에서 예외를 끄는 경우가 흔하고,
// 무엇보다 "실패를 값으로 다뤄야" 진단(Diagnostic)을 잃지 않고 위로 전달할 수 있다.
#pragma once

#include "Diagnostic.h"

#include <new>
#include <type_traits>
#include <utility>

namespace alice {

/// 성공값 T 또는 실패(Diagnostic 하나)를 담는다.
/// 여러 개의 진단이 필요하면 DiagnosticBag 을 out 파라미터로 받는 API를 쓴다.
template <typename T>
class Result {
    static_assert(!std::is_reference_v<T>, "Result<T&> 는 지원하지 않는다");
public:
    Result(T value) : m_hasValue(true) { new (&m_storage) T(std::move(value)); }
    Result(Diagnostic error) : m_hasValue(false), m_error(std::move(error)) {}

    Result(const Result& o) : m_hasValue(o.m_hasValue), m_error(o.m_error) {
        if (m_hasValue) new (&m_storage) T(o.Ref());
    }
    Result(Result&& o) noexcept : m_hasValue(o.m_hasValue), m_error(std::move(o.m_error)) {
        if (m_hasValue) new (&m_storage) T(std::move(o.Ref()));
    }
    Result& operator=(Result o) noexcept { Swap(o); return *this; }
    ~Result() { if (m_hasValue) Ref().~T(); }

    explicit operator bool() const noexcept { return m_hasValue; }
    bool IsOk()  const noexcept { return m_hasValue; }
    bool IsErr() const noexcept { return !m_hasValue; }

    T&       Value()       { ALICE_ASSERT(m_hasValue, "Result 에 값이 없다"); return Ref(); }
    const T& Value() const { ALICE_ASSERT(m_hasValue, "Result 에 값이 없다"); return Ref(); }
    T*       operator->()       { return &Value(); }
    const T* operator->() const { return &Value(); }
    T&       operator*()        { return Value(); }
    const T& operator*()  const { return Value(); }

    T ValueOr(T fallback) const { return m_hasValue ? Ref() : std::move(fallback); }

    const Diagnostic& Error() const {
        ALICE_ASSERT(!m_hasValue, "Result 가 성공 상태다");
        return m_error;
    }

private:
    T&       Ref()       { return *std::launder(reinterpret_cast<T*>(&m_storage)); }
    const T& Ref() const { return *std::launder(reinterpret_cast<const T*>(&m_storage)); }

    void Swap(Result& o) noexcept {
        if (m_hasValue && o.m_hasValue) {
            std::swap(Ref(), o.Ref());
        } else if (m_hasValue) {
            new (&o.m_storage) T(std::move(Ref()));
            Ref().~T();
        } else if (o.m_hasValue) {
            new (&m_storage) T(std::move(o.Ref()));
            o.Ref().~T();
        }
        std::swap(m_hasValue, o.m_hasValue);
        std::swap(m_error, o.m_error);
    }

    alignas(T) unsigned char m_storage[sizeof(T)];
    bool       m_hasValue;
    Diagnostic m_error;
};

/// 값이 없는 성공/실패.
class Status {
public:
    Status() = default;
    Status(Diagnostic error) : m_ok(false), m_error(std::move(error)) {}

    static Status Ok() { return Status{}; }

    explicit operator bool() const noexcept { return m_ok; }
    bool IsOk()  const noexcept { return m_ok; }
    bool IsErr() const noexcept { return !m_ok; }
    const Diagnostic& Error() const {
        ALICE_ASSERT(!m_ok, "Status 가 성공 상태다");
        return m_error;
    }

private:
    bool       m_ok = true;
    Diagnostic m_error;
};

/// 실패 Diagnostic 을 간결하게 만든다.
inline Diagnostic MakeError(std::string code, std::string message, std::string hint = {}) {
    Diagnostic d;
    d.severity = Severity::Error;
    d.code     = std::move(code);
    d.message  = std::move(message);
    d.hint     = std::move(hint);
    return d;
}

/// 실패하면 즉시 반환. 함수 반환형이 Result/Status 일 때만 쓴다.
#define ALICE_TRY(expr)                                                          \
    do {                                                                         \
        auto&& aliceTryResult_ = (expr);                                         \
        if (aliceTryResult_.IsErr()) return aliceTryResult_.Error();             \
    } while (false)

/// 성공값을 변수에 받으면서 실패 시 반환.
#define ALICE_TRY_ASSIGN(decl, expr)                                             \
    auto ALICE_UNIQUE(aliceTry_) = (expr);                                       \
    if (ALICE_UNIQUE(aliceTry_).IsErr()) return ALICE_UNIQUE(aliceTry_).Error(); \
    decl = std::move(ALICE_UNIQUE(aliceTry_).Value())

} // namespace alice
