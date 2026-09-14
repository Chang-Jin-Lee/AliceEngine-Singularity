// SPDX-License-Identifier: MIT
//
// 이 파일의 절반은 "검증이 되는가"가 아니라 **"진단이 쓸 만한가"**를 본다.
// 오타 제안이 사라지거나 힌트가 비면 그건 기능 회귀다. 테스트로 못 박아 둔다.
#include "TestFramework.h"

#include "Doc/Document.h"
#include "Doc/JsonParser.h"
#include "Doc/YamlParser.h"
#include "Schema/JsonSchemaEmitter.h"
#include "Schema/Registry.h"
#include "Schema/SchemaBuilder.h"
#include "Schema/Validator.h"

using namespace alice;
using namespace alice::schema;
using alice::doc::Value;

namespace {

Value ParseYamlOrFail(const char* text, alice::test::Context& ctx) {
    DiagnosticBag bag;
    Value root;
    if (!doc::ParseYaml(text, root, bag)) {
        ctx.Fail("ParseYaml", bag.ToPretty(), __FILE__, __LINE__);
    }
    return root;
}

bool HasCode(const DiagnosticBag& bag, std::string_view code) {
    for (const Diagnostic& d : bag.Items()) {
        if (d.code == code) return true;
    }
    return false;
}

const Diagnostic* Get(const DiagnosticBag& bag, std::string_view code) {
    for (const Diagnostic& d : bag.Items()) {
        if (d.code == code) return &d;
    }
    return nullptr;
}

/// 테스트 전용 작은 스키마.
SchemaPtr TestTransform() {
    return SchemaBuilder::Map("test/transform/1")
        .Title("Transform")
        .Field("position", SchemaBuilder::Vec3(), "위치").Require()
        .Field("rotation", SchemaBuilder::Vec3(), "회전")
        .Field("scale", SchemaBuilder::Vec3(), "크기")
        .Build();
}

} // namespace

// ── 기본 검증 ──────────────────────────────────────────────────────────────
ALICE_TEST(Schema, ValidDocumentPasses) {
    Registry registry;
    registry.Register(TestTransform());

    const Value v = ParseYamlOrFail(
        "position: [1, 2, 3]\n"
        "scale: [1, 1, 1]\n", aliceCtx);

    DiagnosticBag bag;
    ALICE_CHECK(Validate(v, *registry.Find("test/transform/1"), registry, bag));
    ALICE_CHECK_MSG(!bag.HasErrors(), bag.ToPretty());
}

ALICE_TEST(Schema, MissingRequiredFieldIsReported) {
    Registry registry;
    registry.Register(TestTransform());

    const Value v = ParseYamlOrFail("scale: [1, 1, 1]\n", aliceCtx);

    DiagnosticBag bag;
    Validate(v, *registry.Find("test/transform/1"), registry, bag);

    const Diagnostic* d = Get(bag, "schema.missing_field");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK(d->message.find("position") != std::string::npos);
    ALICE_CHECK_MSG(!d->hint.empty(), "필수 필드 누락에는 추가 방법이 힌트로 나와야 한다");
}

ALICE_TEST(Schema, TypeMismatchIsReported) {
    Registry registry;
    registry.Register(TestTransform());

    const Value v = ParseYamlOrFail("position: 5\n", aliceCtx);

    DiagnosticBag bag;
    Validate(v, *registry.Find("test/transform/1"), registry, bag);

    const Diagnostic* d = Get(bag, "schema.type_mismatch");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK_STR(d->path, "position");
    ALICE_CHECK_MSG(d->hint.find("[0, 0, 0]") != std::string::npos,
                    "스칼라를 배열 자리에 쓴 실수에는 배열 예시를 보여줘야 한다");
}

ALICE_TEST(Schema, WrongArrayLengthIsReported) {
    Registry registry;
    registry.Register(TestTransform());

    const Value v = ParseYamlOrFail("position: [1, 2]\n", aliceCtx);

    DiagnosticBag bag;
    Validate(v, *registry.Find("test/transform/1"), registry, bag);
    ALICE_CHECK(HasCode(bag, "schema.too_few_items"));
}

// ── 진단 품질 ──────────────────────────────────────────────────────────────
ALICE_TEST(SchemaDiagnostics, TypoGetsSuggestion) {
    Registry registry;
    registry.Register(TestTransform());

    const Value v = ParseYamlOrFail(
        "position: [0, 0, 0]\n"
        "positon: [1, 1, 1]\n", aliceCtx);

    DiagnosticBag bag;
    Validate(v, *registry.Find("test/transform/1"), registry, bag);

    const Diagnostic* d = Get(bag, "schema.unknown_field");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK_MSG(d->hint.find("position") != std::string::npos,
                    "오타에는 가장 가까운 이름을 제안해야 한다");
    ALICE_CHECK_MSG(d->hint.find("rotation") != std::string::npos,
                    "제안과 함께 쓸 수 있는 필드 목록도 보여줘야 한다");
    ALICE_CHECK_MSG(d->mark.line == 2, "진단은 문제가 있는 줄을 가리켜야 한다");
}

ALICE_TEST(SchemaDiagnostics, EnumMismatchListsCandidates) {
    Registry registry;
    SchemaPtr s = SchemaBuilder::Map("test/light/1")
        .Field("type", SchemaBuilder::Enum({"directional", "point", "spot"}), "광원 종류")
        .Build();
    registry.Register(s);

    const Value v = ParseYamlOrFail("type: pointt\n", aliceCtx);

    DiagnosticBag bag;
    Validate(v, *s, registry, bag);

    const Diagnostic* d = Get(bag, "schema.enum_mismatch");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK(d->hint.find("point") != std::string::npos);
    ALICE_CHECK(d->hint.find("directional") != std::string::npos);
}

ALICE_TEST(SchemaDiagnostics, QuotedNumberGetsUnquoteHint) {
    Registry registry;
    SchemaPtr s = SchemaBuilder::Map("test/num/1")
        .Field("count", SchemaBuilder::Int(), "개수")
        .Build();
    registry.Register(s);

    const Value v = ParseYamlOrFail("count: \"12\"\n", aliceCtx);

    DiagnosticBag bag;
    Validate(v, *s, registry, bag);

    const Diagnostic* d = Get(bag, "schema.type_mismatch");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK_MSG(d->hint.find("따옴표") != std::string::npos,
                    "따옴표 때문에 문자열이 된 숫자는 그 사실을 직접 말해줘야 한다");
}

ALICE_TEST(SchemaDiagnostics, RenamedFieldIsAWarningNotAnError) {
    Registry registry;
    SchemaPtr s = SchemaBuilder::Map("test/mesh/1")
        .Field("asset", SchemaBuilder::String(), "메시 경로").Alias("model").Alias("meshPath")
        .Build();
    registry.Register(s);

    const Value v = ParseYamlOrFail("model: meshes/x.mesh\n", aliceCtx);

    DiagnosticBag bag;
    Validate(v, *s, registry, bag);

    const Diagnostic* d = Get(bag, "schema.renamed_field");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK(d->severity == Severity::Warning);
    ALICE_CHECK(d->hint.find("asset") != std::string::npos);
}

ALICE_TEST(SchemaDiagnostics, ReportsEveryProblemAtOnce) {
    Registry registry;
    registry.Register(TestTransform());

    const Value v = ParseYamlOrFail(
        "positon: [0, 0, 0]\n"
        "rotaton: [0, 0, 0]\n"
        "scale: 1\n", aliceCtx);

    DiagnosticBag bag;
    Validate(v, *registry.Find("test/transform/1"), registry, bag);

    // 오타 2 + 타입 1 + position 누락 1 = 4
    ALICE_CHECK_MSG(bag.ErrorCount() >= 4,
                    Fmt("한 번에 전부 보고해야 한다. 실제: {}", static_cast<u64>(bag.ErrorCount())));
}

// ── 형식 ───────────────────────────────────────────────────────────────────
ALICE_TEST(SchemaFormat, AssetPathRules) {
    std::string reason;
    ALICE_CHECK(CheckFormat(TextFormat::AssetPath, "meshes/player.mesh", reason));
    ALICE_CHECK(!CheckFormat(TextFormat::AssetPath, "meshes\\player.mesh", reason));
    ALICE_CHECK(reason.find("슬래시") != std::string::npos);
    ALICE_CHECK(!CheckFormat(TextFormat::AssetPath, "/abs/path", reason));
    ALICE_CHECK(!CheckFormat(TextFormat::AssetPath, "../outside", reason));
}

ALICE_TEST(SchemaFormat, ColorHexRules) {
    std::string reason;
    ALICE_CHECK(CheckFormat(TextFormat::ColorHex, "#ff8800", reason));
    ALICE_CHECK(CheckFormat(TextFormat::ColorHex, "#f80", reason));
    ALICE_CHECK(CheckFormat(TextFormat::ColorHex, "#ff8800cc", reason));
    ALICE_CHECK(!CheckFormat(TextFormat::ColorHex, "ff8800", reason));
    ALICE_CHECK(!CheckFormat(TextFormat::ColorHex, "#gg8800", reason));
}

ALICE_TEST(SchemaFormat, SchemaIdRules) {
    std::string reason;
    ALICE_CHECK(CheckFormat(TextFormat::SchemaId, "alice/actor/1", reason));
    ALICE_CHECK(!CheckFormat(TextFormat::SchemaId, "alice/actor", reason));
    ALICE_CHECK(!CheckFormat(TextFormat::SchemaId, "actor", reason));
}

// ── 제약이 필드에 걸리는지 ────────────────────────────────────────────────
ALICE_TEST(SchemaBuilderTest, ConstraintsApplyToTheFieldNotTheParent) {
    // .Field(...).Range(0,1) 이 부모 맵이 아니라 그 필드에 걸려야 한다.
    // 순진하게 구현하면 조용히 아무 데도 안 걸린다.
    Registry registry;
    SchemaPtr s = SchemaBuilder::Map("test/mat/1")
        .Field("metallic", SchemaBuilder::Float(), "0..1").Range(0.0, 1.0)
        .Field("roughness", SchemaBuilder::Float(), "0..1").Range(0.0, 1.0)
        .Build();
    registry.Register(s);

    DiagnosticBag bag;
    Validate(ParseYamlOrFail("metallic: 5.0\nroughness: 0.5\n", aliceCtx), *s, registry, bag);

    const Diagnostic* d = Get(bag, "schema.out_of_range");
    ALICE_REQUIRE_MSG(d != nullptr, "필드 제약이 실제로 걸려야 한다");
    ALICE_CHECK_STR(d->path, "metallic");
}

ALICE_TEST(SchemaBuilderTest, SharedScalarSchemasAreNotMutated) {
    // Float() 는 매번 새 객체를 주지만, 만약 캐시하게 되더라도
    // 한 필드의 제약이 다른 필드로 새면 안 된다.
    Registry registry;
    SchemaPtr s = SchemaBuilder::Map("test/two/1")
        .Field("bounded", SchemaBuilder::Float(), "제한 있음").Range(0.0, 1.0)
        .Field("free", SchemaBuilder::Float(), "제한 없음")
        .Build();
    registry.Register(s);

    DiagnosticBag bag;
    Validate(ParseYamlOrFail("bounded: 0.5\nfree: 9999.0\n", aliceCtx), *s, registry, bag);
    ALICE_CHECK_MSG(!bag.HasErrors(), bag.ToPretty());
}

// ── 기본값 ─────────────────────────────────────────────────────────────────
ALICE_TEST(Schema, ApplyDefaultsFillsMissingFields) {
    Registry registry;
    SchemaPtr s = SchemaBuilder::Map("test/cam/1")
        .Field("fov", SchemaBuilder::Float(), "화각").Default(Value{60.0})
        .Field("near", SchemaBuilder::Float(), "근평면").Default(Value{0.1})
        .Field("name", SchemaBuilder::String(), "이름")
        .Build();
    registry.Register(s);

    const Value input = ParseYamlOrFail("name: Main\n", aliceCtx);
    const Value full  = ApplyDefaults(input, *s, registry);

    ALICE_CHECK(full["fov"].AsFloat() == 60.0);
    ALICE_CHECK(full["near"].AsFloat() == 0.1);
    ALICE_CHECK_STR(full["name"].AsString(), "Main");

    // 필드 순서는 스키마 순서를 따라야 한다. 생성된 문서의 diff 가 안정되려면 필요하다.
    const auto keys = full.Keys();
    ALICE_REQUIRE(keys.size() == 3);
    ALICE_CHECK_STR(keys[0], "fov");
    ALICE_CHECK_STR(keys[2], "name");
}

ALICE_TEST(Schema, ApplyDefaultsPreservesUnknownFields) {
    Registry registry;
    SchemaPtr s = SchemaBuilder::Map("test/keep/1")
        .Field("known", SchemaBuilder::Int(), "아는 필드")
        .Build();
    registry.Register(s);

    const Value input = ParseYamlOrFail("known: 1\nfuture: 2\n", aliceCtx);
    const Value full  = ApplyDefaults(input, *s, registry);

    // 모르는 필드를 버리면 데이터가 사라진다. 뒤에 보존해야 한다.
    ALICE_CHECK(full.Has("future"));
    ALICE_CHECK(full["future"].AsInt() == 2);
}

// ── 코어 스키마 ────────────────────────────────────────────────────────────
ALICE_TEST(CoreSchemas, EngineContentTypesAreRegistered) {
    const Registry& registry = Registry::Global();

    // 이 목록이 "이 엔진으로 만들 수 있는 것"이다. 빠지면 곧바로 잡힌다.
    const char* required[] = {
        "alice/project/1", "alice/scene/1", "alice/actor/1", "alice/behavior/1",
        "alice/material/1", "alice/animation/1", "alice/effect/1", "alice/sound/1",
        "alice/physics/1", "alice/input/1",
        "alice/component/transform/1", "alice/component/mesh/1",
        "alice/component/camera/1", "alice/component/light/1",
        "alice/component/rigidbody/1", "alice/component/collider/1",
        "alice/component/audioSource/1", "alice/component/particles/1",
        "alice/component/animator/1",
    };
    for (const char* id : required) {
        ALICE_CHECK_MSG(registry.Has(id), Fmt("코어 스키마 '{}' 가 등록되지 않았다", id));
    }
}

ALICE_TEST(CoreSchemas, EverySchemaHasDescriptionAndFieldDocs) {
    // 설명 없는 스키마는 AI 에게 쓸모가 없다. 문서화를 테스트로 강제한다.
    for (const SchemaPtr& s : Registry::Global().All()) {
        ALICE_CHECK_MSG(!s->description.empty() || !s->title.empty(),
                        Fmt("스키마 '{}' 에 설명이 없다", s->id));
        for (const Field& f : s->fields) {
            if (f.name == "schema") continue;   // 자명하다
            ALICE_CHECK_MSG(!f.description.empty(),
                            Fmt("'{}' 의 필드 '{}' 에 설명이 없다", s->id, f.name));
        }
    }
}

ALICE_TEST(CoreSchemas, ActorDocumentValidates) {
    const Registry& registry = Registry::Global();

    const Value v = ParseYamlOrFail(
        "schema: alice/actor/1\n"
        "name: Player\n"
        "tags: [player, damageable]\n"
        "transform:\n"
        "  position: [0, 1.5, 0]\n"
        "components:\n"
        "  mesh:\n"
        "    asset: meshes/player.mesh\n"
        "    materials: [materials/skin.mat]\n"
        "  rigidbody:\n"
        "    mass: 70\n"
        "  collider:\n"
        "    shape: capsule\n"
        "    radius: 0.35\n"
        "    height: 1.8\n", aliceCtx);

    DiagnosticBag bag;
    Validate(v, *registry.Find("alice/actor/1"), registry, bag);
    ALICE_CHECK_MSG(!bag.HasErrors(), bag.ToPretty());
}

ALICE_TEST(CoreSchemas, UnknownComponentGetsSuggestion) {
    const Registry& registry = Registry::Global();

    const Value v = ParseYamlOrFail(
        "schema: alice/actor/1\n"
        "name: X\n"
        "components:\n"
        "  rigidBody:\n"     // 실제 키는 rigidbody
        "    mass: 1\n", aliceCtx);

    DiagnosticBag bag;
    Validate(v, *registry.Find("alice/actor/1"), registry, bag);

    const Diagnostic* d = Get(bag, "schema.unknown_field");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK_MSG(d->hint.find("rigidbody") != std::string::npos,
                    "대소문자만 다른 컴포넌트 이름도 제안해야 한다");
}

ALICE_TEST(CoreSchemas, MaterialColorPitfallIsDocumented) {
    SchemaPtr material = Registry::Global().Find("alice/material/1");
    ALICE_REQUIRE(material != nullptr);

    bool found = false;
    for (const Schema::Pitfall& p : material->pitfalls) {
        if (p.wrong.find("#") != std::string::npos) found = true;
    }
    ALICE_CHECK_MSG(found, "따옴표 없는 색상 실수는 스키마에 pitfall 로 기록되어 있어야 한다");
}

// ── JSON Schema 생성 ───────────────────────────────────────────────────────
ALICE_TEST(JsonSchema, EmitsValidStructure) {
    Registry registry;
    registry.Register(TestTransform());

    const std::string json = EmitJsonSchema(*registry.Find("test/transform/1"), registry);

    ALICE_CHECK(json.find("\"$schema\"") != std::string::npos);
    ALICE_CHECK(json.find("\"type\": \"object\"") != std::string::npos);
    ALICE_CHECK(json.find("\"properties\"") != std::string::npos);
    ALICE_CHECK(json.find("\"required\"") != std::string::npos);
    ALICE_CHECK(json.find("\"additionalProperties\": false") != std::string::npos);

    // 생성된 것이 다시 파싱되어야 한다. 안 그러면 바깥 도구가 못 읽는다.
    DiagnosticBag bag;
    Value parsed;
    ALICE_CHECK_MSG(doc::ParseJson(json, parsed, bag), bag.ToPretty());
}

ALICE_TEST(JsonSchema, EveryCoreSchemaEmitsParseableJson) {
    const Registry& registry = Registry::Global();
    for (const SchemaPtr& s : registry.All()) {
        const std::string json = EmitJsonSchema(*s, registry);
        DiagnosticBag bag;
        Value parsed;
        ALICE_CHECK_MSG(doc::ParseJson(json, parsed, bag),
                        Fmt("'{}' 의 JSON Schema 가 파싱되지 않는다:\n{}", s->id, bag.ToPretty()));
    }
}

ALICE_TEST(JsonSchema, FileNameMapping) {
    ALICE_CHECK_STR(SchemaFileName("alice/actor/1"), "alice-actor-1.schema.json");
    ALICE_CHECK_STR(SchemaFileName("alice/component/mesh/1"),
                    "alice-component-mesh-1.schema.json");
}
