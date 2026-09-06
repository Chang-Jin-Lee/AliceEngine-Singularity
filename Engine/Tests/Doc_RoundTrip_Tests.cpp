// SPDX-License-Identifier: MIT
//
// 왕복(round-trip)은 이 엔진의 계약이다.
//
//   파싱 → 직렬화 → 다시 파싱  했을 때 값이 그대로여야 한다.
//
// 이게 보장돼야 AI 가 "현재 상태를 읽고 → 고치고 → 되쓰는" 고리를 돌릴 수 있다.
// 왕복이 깨지면 AI 가 한 번 저장할 때마다 콘텐츠가 조금씩 손상된다.
#include "TestFramework.h"

#include "Doc/Document.h"
#include "Doc/JsonParser.h"
#include "Doc/Writer.h"
#include "Doc/YamlParser.h"

using namespace alice;
using namespace alice::doc;

namespace {

/// YAML → Value → YAML → Value 가 동일한지 확인한다.
void CheckYamlRoundTrip(alice::test::Context& ctx, const char* source, const char* file, u32 line) {
    DiagnosticBag bag1;
    Value first;
    if (!ParseYaml(source, first, bag1)) {
        ctx.Fail("초기 파싱", bag1.ToPretty(), file, line);
        return;
    }

    const std::string written = ToYaml(first);

    DiagnosticBag bag2;
    Value second;
    if (!ParseYaml(written, second, bag2)) {
        ctx.Fail("재파싱", "직렬화 결과:\n" + written + "\n" + bag2.ToPretty(), file, line);
        return;
    }

    if (!first.DeepEquals(second)) {
        ctx.Fail("왕복 불일치",
                 "원본:\n" + std::string(source) +
                 "\n직렬화:\n" + written +
                 "\n재직렬화:\n" + ToYaml(second),
                 file, line);
    }
}

void CheckJsonRoundTrip(alice::test::Context& ctx, const char* source, const char* file, u32 line) {
    DiagnosticBag bag1;
    Value first;
    if (!ParseYaml(source, first, bag1)) {
        ctx.Fail("초기 YAML 파싱", bag1.ToPretty(), file, line);
        return;
    }

    const std::string json = ToJson(first);

    DiagnosticBag bag2;
    Value second;
    if (!ParseJson(json, second, bag2)) {
        ctx.Fail("JSON 재파싱", json + "\n" + bag2.ToPretty(), file, line);
        return;
    }

    if (!first.DeepEquals(second)) {
        ctx.Fail("YAML↔JSON 왕복 불일치", "JSON:\n" + json, file, line);
    }
}

} // namespace

#define CHECK_YAML_ROUNDTRIP(src) CheckYamlRoundTrip(aliceCtx, (src), __FILE__, __LINE__)
#define CHECK_JSON_ROUNDTRIP(src) CheckJsonRoundTrip(aliceCtx, (src), __FILE__, __LINE__)

ALICE_TEST(RoundTrip, Scalars) {
    CHECK_YAML_ROUNDTRIP(
        "int: 42\n"
        "negative: -7\n"
        "float: 4.5\n"
        "float_integral: 3.0\n"
        "bool_true: true\n"
        "bool_false: false\n"
        "nothing: null\n"
        "text: hello\n");
}

ALICE_TEST(RoundTrip, StringsThatLookLikeOtherTypes) {
    // 이것들은 반드시 따옴표가 붙어 나가야 한다. 안 그러면 재파싱에서 타입이 바뀐다.
    CHECK_YAML_ROUNDTRIP(
        "a: \"123\"\n"
        "b: \"true\"\n"
        "c: \"null\"\n"
        "d: \"4.5\"\n"
        "e: \"- not a list\"\n"
        "f: \"has: colon\"\n"
        "g: \"#ff8800\"\n"
        "h: \"\"\n"
        "i: \" leading space\"\n"
        "j: \"trailing space \"\n");
}

ALICE_TEST(RoundTrip, NestedStructures) {
    CHECK_YAML_ROUNDTRIP(
        "schema: alice/scene/1\n"
        "name: MainLevel\n"
        "actors:\n"
        "  - name: Player\n"
        "    components:\n"
        "      transform:\n"
        "        position: [0, 1.5, 0]\n"
        "        rotation: [0, 0, 0]\n"
        "      mesh:\n"
        "        asset: meshes/player.mesh\n"
        "        materials: [materials/skin.mat]\n"
        "  - name: Ground\n"
        "    components:\n"
        "      transform:\n"
        "        scale: [50, 1, 50]\n");
}

ALICE_TEST(RoundTrip, EmptyCollections) {
    CHECK_YAML_ROUNDTRIP(
        "empty_seq: []\n"
        "empty_map: {}\n"
        "nested:\n"
        "  inner_seq: []\n");
}

ALICE_TEST(RoundTrip, MultilineStrings) {
    CHECK_YAML_ROUNDTRIP(
        "description: |\n"
        "  첫 줄\n"
        "  둘째 줄\n"
        "stripped: |-\n"
        "  줄바꿈 없음\n"
        "after: 1\n");
}

ALICE_TEST(RoundTrip, SequenceOfSequences) {
    CHECK_YAML_ROUNDTRIP(
        "grid:\n"
        "  - [1, 2, 3]\n"
        "  - [4, 5, 6]\n");
}

ALICE_TEST(RoundTrip, YamlToJsonAndBack) {
    CHECK_JSON_ROUNDTRIP(
        "schema: alice/material/1\n"
        "name: Steel\n"
        "params:\n"
        "  baseColor: [0.6, 0.6, 0.65, 1.0]\n"
        "  metallic: 1.0\n"
        "  roughness: 0.35\n"
        "  useNormalMap: true\n"
        "textures:\n"
        "  - slot: normal\n"
        "    asset: textures/steel_n.png\n");
}

ALICE_TEST(RoundTrip, DeepStructureStaysStable) {
    // 두 번 직렬화하면 완전히 같은 텍스트가 나와야 한다(안정점).
    const char* src =
        "a:\n"
        "  b:\n"
        "    c: [1, 2]\n"
        "    d: text\n";

    DiagnosticBag bag;
    Value v;
    ALICE_REQUIRE(ParseYaml(src, v, bag));

    const std::string once = ToYaml(v);

    DiagnosticBag bag2;
    Value v2;
    ALICE_REQUIRE(ParseYaml(once, v2, bag2));
    const std::string twice = ToYaml(v2);

    ALICE_CHECK_STR(twice, once);
}

ALICE_TEST(Document, SchemaIdIsExposed) {
    DiagnosticBag bag;
    Document doc;
    ALICE_REQUIRE(ParseDocument("schema: alice/actor/3\nname: X\n",
                                Syntax::Yaml, "memory.yaml", doc, bag));

    ALICE_CHECK_STR(doc.SchemaId(), "alice/actor/3");

    std::string base;
    u32 version = 0;
    ALICE_CHECK(Document::SplitSchemaId(doc.SchemaId(), base, version));
    ALICE_CHECK_STR(base, "alice/actor");
    ALICE_CHECK(version == 3);
}

ALICE_TEST(Document, FormatDetection) {
    ALICE_CHECK(DetectSyntax("a.json", "") == Syntax::Json);
    ALICE_CHECK(DetectSyntax("a.yaml", "") == Syntax::Yaml);
    ALICE_CHECK(DetectSyntax("a.yml", "") == Syntax::Yaml);
    // 확장자가 없으면 내용으로 본다.
    ALICE_CHECK(DetectSyntax("noext", "  {\"a\":1}") == Syntax::Json);
    ALICE_CHECK(DetectSyntax("noext", "a: 1") == Syntax::Yaml);
}
