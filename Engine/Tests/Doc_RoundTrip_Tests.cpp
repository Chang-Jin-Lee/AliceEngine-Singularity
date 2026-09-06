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

// ── 주석 보존 ──────────────────────────────────────────────────────────────
//
// 주석을 잃는 포맷터는 아무도 쓰지 않는다. 그리고 이 엔진의 핵심 주장은
// "읽고 → 고치고 → 되쓰는" 고리가 닫힌다는 것인데, 그 고리를 한 번 돌 때마다
// 사람이 남긴 의도가 지워지면 그 주장이 무의미해진다.

ALICE_TEST(Comments, LeadingCommentsSurviveRoundTrip) {
    const char* src =
        "# 이 문서가 무엇인지\n"
        "schema: alice/material/1\n"
        "\n"
        "# 표면 파라미터\n"
        "params:\n"
        "  # 금속성. 0이면 비금속\n"
        "  metallic: 1.0\n";

    DiagnosticBag bag;
    Value v;
    ALICE_REQUIRE(ParseYaml(src, v, bag));

    const std::string out = ToYaml(v);
    ALICE_CHECK_MSG(out.find("이 문서가 무엇인지") != std::string::npos,
                    "문서 머리말 주석이 사라졌다:\n" + out);
    ALICE_CHECK_MSG(out.find("표면 파라미터") != std::string::npos,
                    "필드 주석이 사라졌다:\n" + out);
    ALICE_CHECK_MSG(out.find("금속성") != std::string::npos,
                    "중첩된 필드의 주석이 사라졌다:\n" + out);
}

ALICE_TEST(Comments, TrailingCommentsSurvive) {
    DiagnosticBag bag;
    Value v;
    ALICE_REQUIRE(ParseYaml("speed: 4.5  # m/s\nname: X\n", v, bag));

    const std::string out = ToYaml(v);
    ALICE_CHECK_MSG(out.find("# m/s") != std::string::npos,
                    "줄 끝 주석이 사라졌다:\n" + out);
}

ALICE_TEST(Comments, SequenceItemCommentsSurvive) {
    const char* src =
        "rules:\n"
        "  # 첫 번째 규칙\n"
        "  - when: a\n"
        "  # 두 번째 규칙\n"
        "  - when: b\n";

    DiagnosticBag bag;
    Value v;
    ALICE_REQUIRE(ParseYaml(src, v, bag));

    const std::string out = ToYaml(v);
    ALICE_CHECK_MSG(out.find("첫 번째 규칙") != std::string::npos, out);
    ALICE_CHECK_MSG(out.find("두 번째 규칙") != std::string::npos, out);
}

ALICE_TEST(Comments, FormattingIsIdempotent) {
    // 두 번 정리하면 첫 번째와 같은 결과가 나와야 한다.
    // 그렇지 않으면 `fmt --check` 를 CI 게이트로 쓸 수 없다.
    const char* src =
        "# 머리말\n"
        "schema: alice/behavior/1\n"
        "rules:\n"
        "  # 규칙 하나\n"
        "  - when: physics.grounded\n"
        "    do:\n"
        "      - var.set: { name: x, value: 0 }\n"
        "      - audio.play:\n"
        "          sound: sounds/a.sound.yaml\n";

    DiagnosticBag bag1;
    Value first;
    ALICE_REQUIRE(ParseYaml(src, first, bag1));
    const std::string once = ToYaml(first);

    DiagnosticBag bag2;
    Value second;
    ALICE_REQUIRE_MSG(ParseYaml(once, second, bag2), once + "\n" + bag2.ToPretty());
    const std::string twice = ToYaml(second);

    ALICE_CHECK_STR(twice, once);
    ALICE_CHECK_MSG(once.find("머리말") != std::string::npos, once);
    ALICE_CHECK_MSG(once.find("규칙 하나") != std::string::npos, once);
}

ALICE_TEST(Comments, CommentsDoNotAffectValueEquality) {
    // DeepEquals 는 값만 본다. 주석이 달라도 같은 문서다.
    DiagnosticBag bagA, bagB;
    Value a, b;
    ALICE_REQUIRE(ParseYaml("# 주석 있음\nx: 1\n", a, bagA));
    ALICE_REQUIRE(ParseYaml("x: 1\n", b, bagB));
    ALICE_CHECK(a.DeepEquals(b));
}

ALICE_TEST(Comments, ShortArgMapsStayOnOneLine) {
    // 동작 인자가 두 줄씩 차지하면 규칙 목록이 세 배로 길어져 눈으로 훑을 수 없다.
    DiagnosticBag bag;
    Value v;
    ALICE_REQUIRE(ParseYaml("do:\n  - var.set: { name: x, value: 0 }\n", v, bag));

    const std::string out = ToYaml(v);
    ALICE_CHECK_MSG(out.find("{ name: x, value: 0 }") != std::string::npos,
                    "짧은 인자 맵은 한 줄로 유지되어야 한다:\n" + out);
}
