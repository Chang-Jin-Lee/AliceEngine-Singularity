// SPDX-License-Identifier: MIT
#include "TestFramework.h"

#include "Doc/YamlParser.h"

using namespace alice;
using namespace alice::doc;

namespace {

Value Parse(const char* text, DiagnosticBag& bag) {
    Value root;
    ParseYaml(text, root, bag);
    return root;
}

Value ParseOk(const char* text, alice::test::Context& ctx) {
    DiagnosticBag bag;
    Value root;
    if (!ParseYaml(text, root, bag)) {
        ctx.Fail("ParseYaml", bag.ToPretty(), __FILE__, __LINE__);
    }
    return root;
}

/// 진단 목록에 특정 코드가 있는지.
bool HasCode(const DiagnosticBag& bag, std::string_view code) {
    for (const Diagnostic& d : bag.Items()) {
        if (d.code == code) return true;
    }
    return false;
}

} // namespace

// ── 기본 구조 ──────────────────────────────────────────────────────────────
ALICE_TEST(Yaml, SimpleMap) {
    const Value v = ParseOk(
        "schema: alice/actor/1\n"
        "name: Player\n"
        "health: 100\n"
        "speed: 4.5\n"
        "alive: true\n"
        "target: null\n",
        aliceCtx);

    ALICE_REQUIRE(v.IsMap());
    ALICE_CHECK_STR(v["schema"].AsString(), "alice/actor/1");
    ALICE_CHECK_STR(v["name"].AsString(), "Player");
    ALICE_CHECK(v["health"].IsInt() && v["health"].AsInt() == 100);
    ALICE_CHECK(v["speed"].IsFloat() && v["speed"].AsFloat() == 4.5);
    ALICE_CHECK(v["alive"].IsBool() && v["alive"].AsBool());
    ALICE_CHECK(v["target"].IsNull());
}

ALICE_TEST(Yaml, NestedMapsAndSequences) {
    const Value v = ParseOk(
        "actors:\n"
        "  - name: Player\n"
        "    transform:\n"
        "      position: [0, 1, 0]\n"
        "      scale: 1.0\n"
        "  - name: Enemy\n"
        "    transform:\n"
        "      position: [10, 0, 5]\n",
        aliceCtx);

    ALICE_REQUIRE(v["actors"].IsSeq());
    ALICE_REQUIRE(v["actors"].Size() == 2);
    ALICE_CHECK_STR(v["actors"].At(0)["name"].AsString(), "Player");
    ALICE_CHECK(v["actors"].At(0)["transform"]["position"].Size() == 3);
    ALICE_CHECK(v["actors"].At(0)["transform"]["position"].At(1).AsInt() == 1);
    ALICE_CHECK_STR(v["actors"].At(1)["name"].AsString(), "Enemy");
    ALICE_CHECK(v["actors"].At(1)["transform"]["position"].At(0).AsInt() == 10);
}

ALICE_TEST(Yaml, DeeplyNestedSequenceInSequence) {
    const Value v = ParseOk(
        "grid:\n"
        "  - - 1\n"
        "    - 2\n"
        "  - - 3\n"
        "    - 4\n",
        aliceCtx);

    ALICE_REQUIRE(v["grid"].Size() == 2);
    ALICE_REQUIRE(v["grid"].At(0).Size() == 2);
    ALICE_CHECK(v["grid"].At(0).At(1).AsInt() == 2);
    ALICE_CHECK(v["grid"].At(1).At(0).AsInt() == 3);
}

ALICE_TEST(Yaml, FlowCollections) {
    const Value v = ParseOk(
        "vec: [1, 2, 3]\n"
        "meta: {author: alice, version: 2}\n"
        "mixed: [{a: 1}, {b: 2}]\n"
        "empty_seq: []\n"
        "empty_map: {}\n",
        aliceCtx);

    ALICE_CHECK(v["vec"].Size() == 3);
    ALICE_CHECK(v["vec"].At(2).AsInt() == 3);
    ALICE_CHECK_STR(v["meta"]["author"].AsString(), "alice");
    ALICE_CHECK(v["meta"]["version"].AsInt() == 2);
    ALICE_CHECK(v["mixed"].At(1)["b"].AsInt() == 2);
    ALICE_CHECK(v["empty_seq"].IsSeq() && v["empty_seq"].Size() == 0);
    ALICE_CHECK(v["empty_map"].IsMap() && v["empty_map"].Size() == 0);
}

ALICE_TEST(Yaml, QuotedStringsAndEscapes) {
    const Value v = ParseOk(
        "a: \"has: colon\"\n"
        "b: 'single ''quoted'''\n"
        "c: \"line\\nbreak\"\n"
        "d: \"tab\\there\"\n"
        "e: \"123\"\n",
        aliceCtx);

    ALICE_CHECK_STR(v["a"].AsString(), "has: colon");
    ALICE_CHECK_STR(v["b"].AsString(), "single 'quoted'");
    ALICE_CHECK_STR(v["c"].AsString(), "line\nbreak");
    ALICE_CHECK_STR(v["d"].AsString(), "tab\there");
    // 따옴표를 쓰면 숫자로 승격되지 않아야 한다.
    ALICE_CHECK(v["e"].IsString());
}

ALICE_TEST(Yaml, CommentsAreStripped) {
    const Value v = ParseOk(
        "# 맨 위 주석\n"
        "name: Player   # 뒤 주석\n"
        "url: \"http://x/y#z\"   # 따옴표 안 # 는 주석이 아니다\n"
        "count: 3\n",
        aliceCtx);

    ALICE_CHECK_STR(v["name"].AsString(), "Player");
    ALICE_CHECK_STR(v["url"].AsString(), "http://x/y#z");
    ALICE_CHECK(v["count"].AsInt() == 3);
}

ALICE_TEST(Yaml, BlockScalarLiteralAndFolded) {
    const Value v = ParseOk(
        "literal: |\n"
        "  첫 줄\n"
        "  둘째 줄\n"
        "folded: >\n"
        "  이어지는\n"
        "  문장\n"
        "stripped: |-\n"
        "  끝에 줄바꿈 없음\n"
        "after: 1\n",
        aliceCtx);

    ALICE_CHECK_STR(v["literal"].AsString(), "첫 줄\n둘째 줄\n");
    ALICE_CHECK_STR(v["folded"].AsString(), "이어지는 문장\n");
    ALICE_CHECK_STR(v["stripped"].AsString(), "끝에 줄바꿈 없음");
    ALICE_CHECK(v["after"].AsInt() == 1);
}

ALICE_TEST(Yaml, DocumentStartMarkerIsAccepted) {
    const Value v = ParseOk("---\nname: Player\n", aliceCtx);
    ALICE_CHECK_STR(v["name"].AsString(), "Player");
}

// ── 타입 판정 ──────────────────────────────────────────────────────────────
ALICE_TEST(Yaml, NorwayProblemIsAvoided) {
    // YAML 1.1 은 no 를 false 로 읽는다. 노르웨이 국가코드가 false 가 되는 유명한 사고.
    // 이 엔진은 true/false 만 불리언으로 본다.
    const Value v = ParseOk(
        "country: NO\n"
        "enabled: no\n"
        "toggled: on\n"
        "real: false\n",
        aliceCtx);

    ALICE_CHECK(v["country"].IsString());
    ALICE_CHECK(v["enabled"].IsString());
    ALICE_CHECK(v["toggled"].IsString());
    ALICE_CHECK(v["real"].IsBool());
}

ALICE_TEST(Yaml, VersionStringsStayStrings) {
    const Value v = ParseOk(
        "version: 1.0.0\n"
        "date: 2026-09-06\n"
        "build: 007\n"
        "ratio: 1.5\n",
        aliceCtx);

    ALICE_CHECK(v["version"].IsString());
    ALICE_CHECK(v["date"].IsString());
    ALICE_CHECK(v["build"].IsInt());     // YAML 규칙대로 007 은 정수 7 이다
    ALICE_CHECK(v["ratio"].IsFloat());
}

// ── 진단 품질 (이 엔진의 핵심) ─────────────────────────────────────────────
ALICE_TEST(YamlDiagnostics, TabIndentIsRejectedWithHint) {
    DiagnosticBag bag;
    Parse("root:\n\tchild: 1\n", bag);

    ALICE_CHECK(HasCode(bag, "doc.parse.tab_indent"));
    ALICE_REQUIRE(!bag.Items().empty());
    ALICE_CHECK_MSG(!bag.Items()[0].hint.empty(), "탭 에러에는 반드시 수정 힌트가 있어야 한다");
    ALICE_CHECK(bag.Items()[0].mark.line == 2);
}

ALICE_TEST(YamlDiagnostics, DuplicateKeyIsAnError) {
    DiagnosticBag bag;
    Parse("name: A\nhealth: 1\nname: B\n", bag);

    ALICE_CHECK(HasCode(bag, "doc.parse.duplicate_key"));
    for (const Diagnostic& d : bag.Items()) {
        if (d.code == "doc.parse.duplicate_key") {
            ALICE_CHECK(d.mark.line == 3);
        }
    }
}

ALICE_TEST(YamlDiagnostics, HexColorEatenByCommentIsCaught) {
    // 이 엔진에서 가장 흔한 사고. 조용히 null 이 되면 콘텐츠 버그가 된다.
    DiagnosticBag bag;
    Parse("material:\n  tint: #ff8800\n", bag);

    ALICE_CHECK(HasCode(bag, "doc.parse.value_eaten_by_comment"));
    for (const Diagnostic& d : bag.Items()) {
        if (d.code == "doc.parse.value_eaten_by_comment") {
            ALICE_CHECK_MSG(d.hint.find("\"#ff8800\"") != std::string::npos,
                            "힌트가 따옴표로 감싼 정답을 직접 보여줘야 한다");
        }
    }
}

ALICE_TEST(YamlDiagnostics, AnchorsAndAliasesAreRejectedClearly) {
    DiagnosticBag anchors;
    Parse("base: &common\n  a: 1\n", anchors);
    ALICE_CHECK(HasCode(anchors, "doc.parse.anchor_unsupported"));

    DiagnosticBag aliases;
    Parse("copy: *common\n", aliases);
    ALICE_CHECK(HasCode(aliases, "doc.parse.alias_unsupported"));

    DiagnosticBag tags;
    Parse("value: !!str 3\n", tags);
    ALICE_CHECK(HasCode(tags, "doc.parse.tag_unsupported"));
}

ALICE_TEST(YamlDiagnostics, UnterminatedFlowIsReported) {
    DiagnosticBag bag;
    Parse("vec: [1, 2\n", bag);
    ALICE_CHECK(HasCode(bag, "doc.parse.flow_unterminated"));
}

ALICE_TEST(YamlDiagnostics, MissingColonIsReported) {
    DiagnosticBag bag;
    Parse("name Player\nhealth: 1\n", bag);
    ALICE_CHECK(HasCode(bag, "doc.parse.expected_key"));
}

ALICE_TEST(YamlDiagnostics, ParserRecoversAndReportsEverything) {
    // 한 번에 여러 개를 보고해야 AI 가 왕복을 줄일 수 있다.
    DiagnosticBag bag;
    Parse(
        "name: A\n"
        "name: B\n"
        "bad line here\n"
        "vec: [1, 2\n",
        bag);

    ALICE_CHECK_MSG(bag.ErrorCount() >= 3,
                    "첫 에러에서 멈추지 말고 가능한 모든 문제를 모아야 한다");
}

ALICE_TEST(YamlDiagnostics, DiagnosticsCarrySnippet) {
    DiagnosticBag bag;
    Parse("a: 1\nb: 2\nb: 3\n", bag);

    bool checked = false;
    for (const Diagnostic& d : bag.Items()) {
        if (d.code == "doc.parse.duplicate_key") {
            ALICE_CHECK_STR(d.snippet, "b: 3");
            checked = true;
        }
    }
    ALICE_CHECK(checked);
}
