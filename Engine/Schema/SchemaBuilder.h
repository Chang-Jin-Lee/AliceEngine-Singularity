// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Schema/SchemaBuilder.h
//
// 스키마를 코드로 선언하는 유창한 빌더.
//
// 스키마를 YAML 로 쓰지 않고 C++ 로 두는 이유:
// 스키마는 런타임 구조체와 반드시 함께 움직여야 한다. 구조체를 고치고 스키마 파일을
// 안 고치면 그 즉시 거짓말이 된다. 코드에 두면 최소한 같은 파일에서 눈에 띈다.
// 대신 여기서 만든 스키마를 JSON Schema 와 위키 페이지로 **자동 생성**해서
// 바깥 도구와 사람이 볼 문서는 항상 코드에서 파생되게 한다.
//
//   auto transform = SchemaBuilder::Map("alice/transform/1")
//       .Title("Transform")
//       .Describe("월드 공간에서의 위치·회전·크기")
//       .Field("position", SchemaBuilder::Vec3(), "미터 단위 위치")
//           .Default(Vec3Value(0, 0, 0))
//       .Field("rotation", SchemaBuilder::Vec3(), "오일러 각(도)")
//       .Example(...)
//       .Build();
#pragma once

#include "Schema.h"

namespace alice::schema {

class SchemaBuilder {
public:
    // ── 시작점 ─────────────────────────────────────────────────────────────
    static SchemaBuilder Map(std::string id = {});
    static SchemaBuilder Seq(SchemaPtr items);
    static SchemaBuilder Scalar(Type type);
    static SchemaBuilder Ref(std::string schemaId);
    static SchemaBuilder Union(std::vector<SchemaPtr> options);

    // ── 자주 쓰는 모양 ─────────────────────────────────────────────────────
    static SchemaPtr Bool();
    static SchemaPtr Int();
    static SchemaPtr Float();
    static SchemaPtr Number();
    static SchemaPtr String();
    static SchemaPtr Text(TextFormat format);
    static SchemaPtr Enum(std::vector<std::string> values);
    /// 길이 N의 숫자 배열. 벡터·색상·쿼터니언에 쓴다.
    static SchemaPtr NumberArray(usize length);
    static SchemaPtr Vec2();
    static SchemaPtr Vec3();
    static SchemaPtr Vec4();
    static SchemaPtr Color();      ///< [r,g,b,a] 0..1 또는 "#rrggbb"

    // ── 메타 ───────────────────────────────────────────────────────────────
    SchemaBuilder& Title(std::string title);
    SchemaBuilder& Describe(std::string description);
    SchemaBuilder& Example(Value example);
    SchemaBuilder& Pitfall(std::string wrong, std::string right, std::string why);

    // ── 맵 ─────────────────────────────────────────────────────────────────
    /// 필드를 추가한다. 이후 Default/Require/Alias/Deprecate 는 이 필드에 적용된다.
    SchemaBuilder& Field(std::string name, SchemaPtr type, std::string description = {});
    SchemaBuilder& Require();                       ///< 직전 필드를 필수로
    SchemaBuilder& Default(Value value);            ///< 직전 필드의 기본값
    SchemaBuilder& Alias(std::string oldName);      ///< 직전 필드의 옛 이름
    SchemaBuilder& Deprecate(std::string useInstead);
    /// 스키마에 없는 필드를 허용한다. 값 타입을 주면 그 타입으로 검사한다.
    SchemaBuilder& AllowExtraFields(SchemaPtr valueType = nullptr);

    // ── 시퀀스 ─────────────────────────────────────────────────────────────
    SchemaBuilder& Items(SchemaPtr items);
    SchemaBuilder& Length(usize minItems, usize maxItems);
    SchemaBuilder& MinItems(usize n);
    SchemaBuilder& MaxItems(usize n);

    // ── 스칼라 ─────────────────────────────────────────────────────────────
    SchemaBuilder& Range(f64 minValue, f64 maxValue);
    SchemaBuilder& Min(f64 minValue);
    SchemaBuilder& Max(f64 maxValue);
    SchemaBuilder& OneOf(std::vector<std::string> values);
    SchemaBuilder& StringLength(usize minLength, usize maxLength);
    SchemaBuilder& WithFormat(TextFormat format);

    SchemaPtr Build();

private:
    /// 제약 메서드(Min/Max/Range/OneOf/...)가 적용될 대상.
    /// Field() 뒤라면 그 필드의 타입, 아니면 빌더 자신의 스키마다.
    ///
    /// 왜 이렇게: `.Field("metallic", Float(), "...").Range(0, 1)` 이 자연스럽게 읽히는데,
    /// 순진하게 구현하면 Range 가 **부모 맵**에 걸려서 조용히 아무 일도 하지 않는다.
    /// 그런 종류의 버그는 테스트에도 안 잡히고 몇 달 뒤에 발견된다. 그래서 대상 해석을
    /// 한곳에 모으고, 필드 타입은 공유될 수 있으므로 쓰기 직전에 복제한다.
    Schema& ConstraintTarget();

    Schema                  m_schema;
    isize                   m_lastField = -1;
    std::shared_ptr<Schema> m_lastFieldOwned;   ///< 복제해서 소유 중인 필드 타입
};

/// 숫자 시퀀스 값을 만드는 보조 함수 (예시 작성용).
Value MakeNumberSeq(std::initializer_list<f64> values);

} // namespace alice::schema
