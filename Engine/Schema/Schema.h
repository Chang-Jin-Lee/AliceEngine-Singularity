// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Schema/Schema.h
//
// 스키마는 "문서가 올바른가"를 판정하는 규칙이자, **AI에게 주는 API 문서**다.
//
// 언리얼의 UHT 가 C++ 헤더에서 리플렉션을 뽑아 블루프린트를 만들듯,
// 여기서는 스키마가 텍스트 문서와 엔진 런타임 사이의 계약을 정의한다. 차이는 방향이다.
//   UHT : 코드 → 메타데이터 → 에디터
//   여기 : 스키마 → 검증 + 자동완성 + JSON Schema + 위키 페이지  (전부 한 소스에서)
//
// 스키마가 반드시 담아야 하는 것 셋:
//   1) 구조     — 어떤 필드가 있고 타입이 무엇인가
//   2) 설명     — 사람과 AI 가 읽을 한 문장
//   3) **예시** — AI 는 설명 열 줄보다 올바른 예시 한 개에서 훨씬 많이 배운다
//
// 그래서 Field 에 description 과 Schema 에 examples 가 선택이 아니라 기본이다.
#pragma once

#include "Doc/Value.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace alice::schema {

using doc::Value;

enum class Type : u8 {
    Any = 0,   ///< 무엇이든. 확장 지점에만 쓴다
    Null,
    Bool,
    Int,
    Float,
    Number,    ///< Int 또는 Float
    String,
    Seq,
    Map,
    Ref,       ///< 다른 스키마를 가리킨다
    Union,     ///< 여러 스키마 중 하나
};

const char* ToString(Type t) noexcept;

class Schema;
using SchemaPtr = std::shared_ptr<const Schema>;

/// 맵 스키마의 필드 하나.
struct Field {
    std::string              name;
    SchemaPtr                type;
    bool                     required = false;
    Value                    defaultValue;          ///< required 가 false 일 때의 값
    std::string              description;
    std::vector<std::string> aliases;               ///< 옛 이름. 마이그레이션 안내에 쓴다
    std::string              deprecatedBy;          ///< 비었으면 살아있는 필드
};

/// 문자열 값의 의미 태그. 정규식 대신 이름 있는 형식을 쓴다.
/// 왜: 정규식은 사람도 AI 도 못 읽고, 에러 메시지로 만들 수가 없다.
/// "asset-path 형식이어야 한다" 는 설명이 되지만 "^[a-z/]+$" 는 설명이 안 된다.
enum class TextFormat : u8 {
    None = 0,
    Identifier,   ///< [A-Za-z_][A-Za-z0-9_]*  — 이름, 키
    AssetPath,    ///< "meshes/player.mesh" — 슬래시 구분, 역슬래시 금지
    SchemaId,     ///< "alice/actor/1"
    VerbId,       ///< "audio.play"
    ColorHex,     ///< "#rrggbb" 또는 "#rrggbbaa"
    Expression,   ///< 조건식. 문법 검사는 Verbs 모듈이 한다
};

const char* ToString(TextFormat f) noexcept;

class Schema {
public:
    // ── 정체 ───────────────────────────────────────────────────────────────
    std::string id;            ///< "alice/actor/1". 최상위 스키마만 가진다
    std::string title;         ///< 사람이 읽는 이름
    std::string description;   ///< 한두 문장

    Type   type   = Type::Any;
    TextFormat format = TextFormat::None;

    // ── 맵 ─────────────────────────────────────────────────────────────────
    std::vector<Field> fields;
    /// 스키마에 없는 필드를 허용할지. **기본은 금지**다.
    /// 오타를 조용히 통과시키면 AI 가 자기 실수를 영원히 모른다.
    bool      additionalFields = false;
    SchemaPtr additionalFieldType;   ///< additionalFields 가 true 일 때 값 타입

    // ── 시퀀스 ─────────────────────────────────────────────────────────────
    SchemaPtr            items;
    std::optional<usize> minItems;
    std::optional<usize> maxItems;

    // ── 스칼라 제약 ────────────────────────────────────────────────────────
    std::optional<f64>       minValue;
    std::optional<f64>       maxValue;
    std::vector<std::string> enumValues;   ///< 비어 있지 않으면 이 중 하나여야 한다
    std::optional<usize>     minLength;
    std::optional<usize>     maxLength;

    // ── 참조 / 합집합 ──────────────────────────────────────────────────────
    std::string            refId;      ///< Type::Ref
    std::vector<SchemaPtr> options;    ///< Type::Union

    // ── 문서화 ─────────────────────────────────────────────────────────────
    /// 올바른 값의 예시. AI 프롬프트와 위키 페이지에 그대로 실린다.
    std::vector<Value> examples;
    /// 흔한 실수와 그 정답. 진단 힌트로 승격된다.
    struct Pitfall {
        std::string wrong;
        std::string right;
        std::string why;
    };
    std::vector<Pitfall> pitfalls;

    // ── 조회 ───────────────────────────────────────────────────────────────
    const Field* FindField(std::string_view name) const noexcept;
    /// alias 로도 찾는다. 옛 이름을 쓴 문서에 "이름이 바뀌었다"고 알려줄 수 있다.
    const Field* FindFieldByAlias(std::string_view name) const noexcept;
    std::vector<std::string> FieldNames() const;
    std::vector<std::string> RequiredFieldNames() const;
};

} // namespace alice::schema
