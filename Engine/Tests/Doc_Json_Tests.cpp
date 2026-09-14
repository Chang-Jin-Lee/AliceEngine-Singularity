// SPDX-License-Identifier: MIT
#include "TestFramework.h"

#include "Doc/JsonParser.h"

using namespace alice;
using namespace alice::doc;

namespace {

bool HasCode(const DiagnosticBag& bag, std::string_view code) {
    for (const Diagnostic& d : bag.Items()) {
        if (d.code == code) return true;
    }
    return false;
}

Value ParseOk(const char* text, alice::test::Context& ctx) {
    DiagnosticBag bag;
    Value root;
    if (!ParseJson(text, root, bag)) {
        ctx.Fail("ParseJson", bag.ToPretty(), __FILE__, __LINE__);
    }
    return root;
}

} // namespace

ALICE_TEST(Json, Basics) {
    const Value v = ParseOk(
        "{\n"
        "  \"schema\": \"alice/actor/1\",\n"
        "  \"health\": 100,\n"
        "  \"speed\": 4.5,\n"
        "  \"alive\": true,\n"
        "  \"target\": null,\n"
        "  \"tags\": [\"a\", \"b\"],\n"
        "  \"nested\": { \"x\": 1 }\n"
        "}",
        aliceCtx);

    ALICE_REQUIRE(v.IsMap());
    ALICE_CHECK_STR(v["schema"].AsString(), "alice/actor/1");
    ALICE_CHECK(v["health"].IsInt());
    ALICE_CHECK(v["speed"].IsFloat());
    ALICE_CHECK(v["alive"].AsBool());
    ALICE_CHECK(v["target"].IsNull());
    ALICE_CHECK(v["tags"].Size() == 2);
    ALICE_CHECK(v["nested"]["x"].AsInt() == 1);
}

ALICE_TEST(Json, KeyOrderIsPreserved) {
    const Value v = ParseOk("{\"z\": 1, \"a\": 2, \"m\": 3}", aliceCtx);
    const auto keys = v.Keys();
    ALICE_REQUIRE(keys.size() == 3);
    ALICE_CHECK_STR(keys[0], "z");
    ALICE_CHECK_STR(keys[1], "a");
    ALICE_CHECK_STR(keys[2], "m");
}

ALICE_TEST(Json, CommentsAndTrailingCommasAreAllowedByDefault) {
    const Value v = ParseOk(
        "{\n"
        "  // 한 줄 주석\n"
        "  \"a\": 1,\n"
        "  /* 블록\n"
        "     주석 */\n"
        "  \"b\": [1, 2,],\n"
        "}",
        aliceCtx);

    ALICE_CHECK(v["a"].AsInt() == 1);
    ALICE_CHECK(v["b"].Size() == 2);
}

ALICE_TEST(Json, StrictModeRejectsTrailingComma) {
    JsonParseOptions opts;
    opts.allowTrailingComma = false;

    DiagnosticBag bag;
    Value root;
    ParseJson("{\"a\": 1,}", root, bag, opts);
    ALICE_CHECK(HasCode(bag, "doc.json.trailing_comma"));
}

ALICE_TEST(Json, UnicodeEscapes) {
    const Value v = ParseOk("{\"k\": \"\\uD55C\"}", aliceCtx);
    ALICE_CHECK_STR(v["k"].AsString(), "한");
}

ALICE_TEST(JsonDiagnostics, SingleQuotesGetHelpfulError) {
    DiagnosticBag bag;
    Value root;
    ParseJson("{'a': 1}", root, bag);

    ALICE_CHECK(bag.HasErrors());
    bool helpful = false;
    for (const Diagnostic& d : bag.Items()) {
        if (d.hint.find("겹따옴표") != std::string::npos) helpful = true;
    }
    ALICE_CHECK_MSG(helpful, "JSON 에 홑따옴표를 쓴 실수는 힌트로 잡아줘야 한다");
}

ALICE_TEST(JsonDiagnostics, PositionsArePrecise) {
    DiagnosticBag bag;
    Value root;
    ParseJson("{\n  \"a\": 1\n  \"b\": 2\n}", root, bag);

    ALICE_REQUIRE(bag.HasErrors());
    ALICE_CHECK_MSG(bag.Items()[0].mark.line == 3,
                    "에러 위치는 문제가 드러난 줄을 가리켜야 한다");
}

ALICE_TEST(JsonDiagnostics, UnterminatedStringPointsAtOpeningQuote) {
    DiagnosticBag bag;
    Value root;
    ParseJson("{\"a\": \"unterminated\n}", root, bag);
    ALICE_CHECK(HasCode(bag, "doc.json.unterminated_string"));
}

ALICE_TEST(JsonDiagnostics, DuplicateKeyIsAnError) {
    DiagnosticBag bag;
    Value root;
    ParseJson("{\"a\": 1, \"a\": 2}", root, bag);
    ALICE_CHECK(HasCode(bag, "doc.json.duplicate_key"));
}
