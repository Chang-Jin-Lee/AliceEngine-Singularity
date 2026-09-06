// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Doc/Value.h
//
// 이 엔진의 콘텐츠는 전부 텍스트 문서다. Value 는 그 문서를 메모리에 담는 단 하나의 표현이다.
//
// 설계 규칙 넷:
//
//  1) **맵의 키 순서를 보존한다.** 해시맵을 쓰면 저장할 때마다 키 순서가 흔들리고 diff 가
//     의미를 잃는다. AI 가 문서를 고치는 워크플로에서 diff 가 안 읽히면 리뷰가 불가능하다.
//     그래서 map 은 벡터다. 문서 하나의 키 개수는 수십 개 수준이라 선형 탐색으로 충분하다.
//
//  2) **정수와 실수를 구분해서 보관한다.** 3 과 3.0 이 왕복 후에도 그대로여야 한다.
//     스키마가 "int 를 기대"할 때 3.0 을 준 문서를 잡아낼 수 있어야 하기 때문이다.
//
//  3) **모든 값이 소스 위치(Mark)를 들고 다닌다.** 검증 실패는 "어느 줄"을 말해야 한다.
//     이 한 필드가 AI 자가수정의 성패를 가른다.
//
//  4) **YAML 과 JSON 이 같은 Value 로 들어온다.** 표기법이 다를 뿐 모델은 하나다.
//     사람과 AI 는 YAML 로 쓰고, 도구 사이에는 JSON 으로 흐른다.
#pragma once

#include "Foundation/Diagnostic.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace alice::doc {

enum class Kind : u8 {
    Null = 0,
    Bool,
    Int,
    Float,
    String,
    Seq,
    Map,
};

const char* ToString(Kind k) noexcept;

class Value;
struct MapEntry;

// 별칭 자체는 인스턴스화를 일으키지 않는다. Value 안에서는 unique_ptr 로만 잡으므로
// 여기서 Value/MapEntry 가 불완전해도 된다. 실제 정의는 Value 클래스 뒤에 온다.
using Seq = std::vector<Value>;
using Map = std::vector<MapEntry>;

class Value {
public:
    Value() noexcept;                        ///< null
    Value(std::nullptr_t) noexcept;
    Value(bool v) noexcept;
    Value(i32 v) noexcept;
    Value(i64 v) noexcept;
    Value(u32 v) noexcept;
    Value(u64 v) noexcept;
    Value(f32 v) noexcept;
    Value(f64 v) noexcept;
    Value(std::string v);
    Value(std::string_view v);
    Value(const char* v);

    Value(const Value& other);
    Value(Value&& other) noexcept;
    Value& operator=(const Value& other);
    Value& operator=(Value&& other) noexcept;
    ~Value();

    static Value MakeSeq();
    static Value MakeMap();
    static Value MakeSeq(std::vector<Value> items);

    // ── 종류 ───────────────────────────────────────────────────────────────
    Kind GetKind() const noexcept { return m_kind; }
    bool IsNull()   const noexcept { return m_kind == Kind::Null; }
    bool IsBool()   const noexcept { return m_kind == Kind::Bool; }
    bool IsInt()    const noexcept { return m_kind == Kind::Int; }
    bool IsFloat()  const noexcept { return m_kind == Kind::Float; }
    bool IsNumber() const noexcept { return m_kind == Kind::Int || m_kind == Kind::Float; }
    bool IsString() const noexcept { return m_kind == Kind::String; }
    bool IsSeq()    const noexcept { return m_kind == Kind::Seq; }
    bool IsMap()    const noexcept { return m_kind == Kind::Map; }
    bool IsScalar() const noexcept { return m_kind != Kind::Seq && m_kind != Kind::Map; }

    // ── 스칼라 읽기 ─────────────────────────────────────────────────────────
    bool               AsBool(bool fallback = false) const noexcept;
    i64                AsInt(i64 fallback = 0) const noexcept;
    /// Int 도 받아준다. 문서에서 1 과 1.0 을 구별해 쓰라고 강요하지 않기 위해서다.
    f64                AsFloat(f64 fallback = 0.0) const noexcept;
    const std::string& AsString() const noexcept;
    std::string        ToScalarText() const;   ///< 종류와 무관하게 사람이 읽을 한 줄

    // ── 시퀀스 ──────────────────────────────────────────────────────────────
    usize        Size() const noexcept;
    bool         Empty() const noexcept { return Size() == 0; }
    const Value& At(usize index) const noexcept;   ///< 범위를 벗어나면 null 참조
    Value&       At(usize index);
    void         Push(Value v);
    const Seq&   Items() const noexcept;
    Seq&         Items();

    // ── 맵 ─────────────────────────────────────────────────────────────────
    bool         Has(std::string_view key) const noexcept;
    const Value* Find(std::string_view key) const noexcept;
    Value*       Find(std::string_view key) noexcept;
    /// 없는 키면 공유 null 을 돌려준다. 널 체크 없이 체이닝할 수 있게.
    const Value& operator[](std::string_view key) const noexcept;
    /// 없으면 만든다.
    Value&       operator[](std::string_view key);
    void         Set(std::string key, Value v);
    bool         Remove(std::string_view key);
    const Map&   Entries() const noexcept;
    Map&         Entries();
    std::vector<std::string> Keys() const;

    // ── 경로 접근 ───────────────────────────────────────────────────────────
    /// "actors[0].components.mesh.asset" 형태. 없으면 nullptr.
    /// 진단의 path 필드와 같은 문법이라, AI가 에러에서 받은 경로를 그대로 되먹일 수 있다.
    const Value* AtPath(std::string_view path) const noexcept;

    // ── 비교 ───────────────────────────────────────────────────────────────
    /// Mark 는 무시하고 내용만 비교한다. 왕복(round-trip) 테스트가 이걸 쓴다.
    bool DeepEquals(const Value& other) const noexcept;

    /// 공유 null 싱글턴.
    static const Value& Null() noexcept;

    /// 소스 위치. 파서가 채운다. 손으로 만든 값에는 없다.
    Mark mark;

private:
    void Destroy() noexcept;
    void CopyFrom(const Value& other);

    Kind m_kind = Kind::Null;
    union Scalar {
        bool b;
        i64  i;
        f64  f;
        Scalar() : i(0) {}
    } m_scalar;
    std::string          m_string;
    std::unique_ptr<Seq> m_seq;
    std::unique_ptr<Map> m_map;
};

/// 맵의 한 항목. Value 가 완전해진 뒤에 정의된다.
struct MapEntry {
    std::string key;
    Value       value;
};

} // namespace alice::doc
