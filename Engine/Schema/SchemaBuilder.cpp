// SPDX-License-Identifier: MIT
#include "SchemaBuilder.h"

namespace alice::schema {
namespace {

SchemaPtr MakeScalar(Type type, TextFormat format = TextFormat::None) {
    Schema s;
    s.type   = type;
    s.format = format;
    return std::make_shared<const Schema>(std::move(s));
}

} // namespace

// ── 시작점 ─────────────────────────────────────────────────────────────────
SchemaBuilder SchemaBuilder::Map(std::string id) {
    SchemaBuilder b;
    b.m_schema.type = Type::Map;
    b.m_schema.id   = std::move(id);
    return b;
}

SchemaBuilder SchemaBuilder::Seq(SchemaPtr items) {
    SchemaBuilder b;
    b.m_schema.type  = Type::Seq;
    b.m_schema.items = std::move(items);
    return b;
}

SchemaBuilder SchemaBuilder::Scalar(Type type) {
    SchemaBuilder b;
    b.m_schema.type = type;
    return b;
}

SchemaBuilder SchemaBuilder::Ref(std::string schemaId) {
    SchemaBuilder b;
    b.m_schema.type  = Type::Ref;
    b.m_schema.refId = std::move(schemaId);
    return b;
}

SchemaBuilder SchemaBuilder::Union(std::vector<SchemaPtr> options) {
    SchemaBuilder b;
    b.m_schema.type    = Type::Union;
    b.m_schema.options = std::move(options);
    return b;
}

// ── 자주 쓰는 모양 ─────────────────────────────────────────────────────────
SchemaPtr SchemaBuilder::Bool()   { return MakeScalar(Type::Bool); }
SchemaPtr SchemaBuilder::Int()    { return MakeScalar(Type::Int); }
SchemaPtr SchemaBuilder::Float()  { return MakeScalar(Type::Float); }
SchemaPtr SchemaBuilder::Number() { return MakeScalar(Type::Number); }
SchemaPtr SchemaBuilder::String() { return MakeScalar(Type::String); }

SchemaPtr SchemaBuilder::Text(TextFormat format) { return MakeScalar(Type::String, format); }

SchemaPtr SchemaBuilder::Enum(std::vector<std::string> values) {
    Schema s;
    s.type       = Type::String;
    s.enumValues = std::move(values);
    return std::make_shared<const Schema>(std::move(s));
}

SchemaPtr SchemaBuilder::NumberArray(usize length) {
    Schema s;
    s.type     = Type::Seq;
    s.items    = Number();
    s.minItems = length;
    s.maxItems = length;
    return std::make_shared<const Schema>(std::move(s));
}

SchemaPtr SchemaBuilder::Vec2() { return NumberArray(2); }
SchemaPtr SchemaBuilder::Vec3() { return NumberArray(3); }
SchemaPtr SchemaBuilder::Vec4() { return NumberArray(4); }

SchemaPtr SchemaBuilder::Color() {
    // 두 표기를 모두 받는다. 아티스트는 "#ff8800" 을, 코드는 [1,0.53,0,1] 을 쓴다.
    Schema hex;
    hex.type        = Type::String;
    hex.format      = TextFormat::ColorHex;
    hex.description = "16진 색상. 예) \"#ff8800\"";

    std::vector<SchemaPtr> options;
    options.push_back(std::make_shared<const Schema>(std::move(hex)));
    options.push_back(NumberArray(4));
    options.push_back(NumberArray(3));

    Schema s;
    s.type        = Type::Union;
    s.options     = std::move(options);
    s.description = "색상. \"#rrggbb\" 문자열 또는 [r, g, b, a] 배열(0..1)";
    return std::make_shared<const Schema>(std::move(s));
}

// ── 메타 ───────────────────────────────────────────────────────────────────
SchemaBuilder& SchemaBuilder::Title(std::string title) {
    m_schema.title = std::move(title);
    return *this;
}

SchemaBuilder& SchemaBuilder::Describe(std::string description) {
    // 필드를 추가한 뒤에 부르면 그 필드의 설명으로 간다. 체인이 자연스러워진다.
    if (m_lastField >= 0) {
        m_schema.fields[static_cast<usize>(m_lastField)].description = std::move(description);
    } else {
        m_schema.description = std::move(description);
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::Example(Value example) {
    m_schema.examples.push_back(std::move(example));
    return *this;
}

SchemaBuilder& SchemaBuilder::Pitfall(std::string wrong, std::string right, std::string why) {
    m_schema.pitfalls.push_back(
        Schema::Pitfall{std::move(wrong), std::move(right), std::move(why)});
    return *this;
}

// ── 맵 ─────────────────────────────────────────────────────────────────────
SchemaBuilder& SchemaBuilder::Field(std::string name, SchemaPtr type, std::string description) {
    // 멤버 함수 이름 Field 가 struct Field 를 가린다. 명시적으로 이름을 푼다.
    alice::schema::Field f;
    f.name        = std::move(name);
    f.type        = std::move(type);
    f.description = std::move(description);
    m_schema.fields.push_back(std::move(f));
    m_lastField      = static_cast<isize>(m_schema.fields.size()) - 1;
    m_lastFieldOwned = nullptr;   // 아직 이 필드 타입을 복제하지 않았다
    return *this;
}

SchemaBuilder& SchemaBuilder::Require() {
    ALICE_ASSERT(m_lastField >= 0, "Require() 앞에 Field() 가 있어야 한다");
    m_schema.fields[static_cast<usize>(m_lastField)].required = true;
    return *this;
}

SchemaBuilder& SchemaBuilder::Default(Value value) {
    ALICE_ASSERT(m_lastField >= 0, "Default() 앞에 Field() 가 있어야 한다");
    m_schema.fields[static_cast<usize>(m_lastField)].defaultValue = std::move(value);
    return *this;
}

SchemaBuilder& SchemaBuilder::Alias(std::string oldName) {
    ALICE_ASSERT(m_lastField >= 0, "Alias() 앞에 Field() 가 있어야 한다");
    m_schema.fields[static_cast<usize>(m_lastField)].aliases.push_back(std::move(oldName));
    return *this;
}

SchemaBuilder& SchemaBuilder::Deprecate(std::string useInstead) {
    ALICE_ASSERT(m_lastField >= 0, "Deprecate() 앞에 Field() 가 있어야 한다");
    m_schema.fields[static_cast<usize>(m_lastField)].deprecatedBy = std::move(useInstead);
    return *this;
}

SchemaBuilder& SchemaBuilder::AllowExtraFields(SchemaPtr valueType) {
    m_schema.additionalFields    = true;
    m_schema.additionalFieldType = std::move(valueType);
    return *this;
}

// ── 시퀀스 ─────────────────────────────────────────────────────────────────
SchemaBuilder& SchemaBuilder::Items(SchemaPtr items) {
    m_schema.items = std::move(items);
    return *this;
}

SchemaBuilder& SchemaBuilder::Length(usize minItems, usize maxItems) {
    Schema& t = ConstraintTarget();
    t.minItems = minItems;
    t.maxItems = maxItems;
    return *this;
}

SchemaBuilder& SchemaBuilder::MinItems(usize n) {
    ConstraintTarget().minItems = n;
    return *this;
}

SchemaBuilder& SchemaBuilder::MaxItems(usize n) {
    ConstraintTarget().maxItems = n;
    return *this;
}

// ── 제약 대상 ──────────────────────────────────────────────────────────────
Schema& SchemaBuilder::ConstraintTarget() {
    if (m_lastField < 0) return m_schema;

    alice::schema::Field& field = m_schema.fields[static_cast<usize>(m_lastField)];
    if (!m_lastFieldOwned) {
        // 필드 타입은 B::Float() 처럼 공유될 수 있는 SchemaPtr 이다.
        // 제자리에서 고치면 남의 스키마까지 바뀐다. 그래서 복제 후 교체한다.
        m_lastFieldOwned = field.type ? std::make_shared<Schema>(*field.type)
                                      : std::make_shared<Schema>();
        field.type = m_lastFieldOwned;
    }
    return *m_lastFieldOwned;
}

// ── 스칼라 ─────────────────────────────────────────────────────────────────
SchemaBuilder& SchemaBuilder::Range(f64 minValue, f64 maxValue) {
    Schema& t = ConstraintTarget();
    t.minValue = minValue;
    t.maxValue = maxValue;
    return *this;
}

SchemaBuilder& SchemaBuilder::Min(f64 minValue) {
    ConstraintTarget().minValue = minValue;
    return *this;
}

SchemaBuilder& SchemaBuilder::Max(f64 maxValue) {
    ConstraintTarget().maxValue = maxValue;
    return *this;
}

SchemaBuilder& SchemaBuilder::OneOf(std::vector<std::string> values) {
    ConstraintTarget().enumValues = std::move(values);
    return *this;
}

SchemaBuilder& SchemaBuilder::StringLength(usize minLength, usize maxLength) {
    Schema& t = ConstraintTarget();
    t.minLength = minLength;
    t.maxLength = maxLength;
    return *this;
}

SchemaBuilder& SchemaBuilder::WithFormat(TextFormat format) {
    ConstraintTarget().format = format;
    return *this;
}

SchemaPtr SchemaBuilder::Build() {
    return std::make_shared<const Schema>(std::move(m_schema));
}

Value MakeNumberSeq(std::initializer_list<f64> values) {
    Value v = Value::MakeSeq();
    for (const f64 x : values) v.Push(Value{x});
    return v;
}

} // namespace alice::schema
