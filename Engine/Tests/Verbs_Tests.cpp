// SPDX-License-Identifier: MIT
#include "TestFramework.h"

#include "Doc/JsonParser.h"
#include "Doc/YamlParser.h"
#include "Schema/Registry.h"
#include "Verbs/Expression.h"
#include "Verbs/SemanticCheck.h"
#include "Verbs/Verb.h"

using namespace alice;
using namespace alice::verbs;
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

std::string Roundtrip(const char* text, alice::test::Context& ctx) {
    DiagnosticBag bag;
    ExprPtr expr;
    if (!ParseExpression(text, expr, bag)) {
        ctx.Fail("ParseExpression", std::string(text) + "\n" + bag.ToPretty(), __FILE__, __LINE__);
        return {};
    }
    return expr->ToText();
}

} // namespace

// ── 식 파싱 ────────────────────────────────────────────────────────────────
ALICE_TEST(Expression, ParsesLiterals) {
    ALICE_CHECK_STR(Roundtrip("42", aliceCtx), "42");
    ALICE_CHECK_STR(Roundtrip("1.5", aliceCtx), "1.5");
    ALICE_CHECK_STR(Roundtrip("true", aliceCtx), "true");
    ALICE_CHECK_STR(Roundtrip("\"text\"", aliceCtx), "\"text\"");
}

ALICE_TEST(Expression, ParsesMemberAndCall) {
    ALICE_CHECK_STR(Roundtrip("physics.grounded", aliceCtx), "physics.grounded");
    ALICE_CHECK_STR(Roundtrip("input.pressed(\"Jump\")", aliceCtx), "input.pressed(\"Jump\")");
    ALICE_CHECK_STR(Roundtrip("math.clamp(a, 0, 1)", aliceCtx), "math.clamp(a, 0, 1)");
}

ALICE_TEST(Expression, RespectsPrecedence) {
    // and 가 or 보다 강하게 묶여야 한다.
    ALICE_CHECK_STR(Roundtrip("a or b and c", aliceCtx), "a or b and c");
    // 곱셈이 덧셈보다 강하다.
    ALICE_CHECK_STR(Roundtrip("1 + 2 * 3", aliceCtx), "1 + 2 * 3");
    // 비교가 논리보다 강하다.
    ALICE_CHECK_STR(Roundtrip("x > 1 and y < 2", aliceCtx), "x > 1 and y < 2");
}

ALICE_TEST(Expression, AcceptsSymbolicOperators) {
    // && || ! 도 받되 정규 이름으로 접는다. YAML 안에서 기호가 따옴표를 요구할 때가 있어서다.
    ALICE_CHECK_STR(Roundtrip("a && b", aliceCtx), "a and b");
    ALICE_CHECK_STR(Roundtrip("a || b", aliceCtx), "a or b");
    ALICE_CHECK_STR(Roundtrip("!a", aliceCtx), "not a");
}

ALICE_TEST(Expression, ParsesRealisticCondition) {
    const std::string text =
        Roundtrip("input.pressed(\"Jump\") and physics.grounded and not self.hasTag(\"stunned\")",
                  aliceCtx);
    ALICE_CHECK(text.find("input.pressed") != std::string::npos);
    ALICE_CHECK(text.find("not self.hasTag") != std::string::npos);
}

// ── 식 진단 ────────────────────────────────────────────────────────────────
ALICE_TEST(ExpressionDiagnostics, AssignmentIsRejectedWithHint) {
    DiagnosticBag bag;
    ExprPtr expr;
    ParseExpression("health = 10", expr, bag);

    const Diagnostic* d = Get(bag, "expr.assignment");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK_MSG(d->hint.find("==") != std::string::npos,
                    "= 를 쓴 실수에는 == 를 알려줘야 한다");
    ALICE_CHECK_MSG(d->hint.find("do:") != std::string::npos,
                    "값을 바꾸려면 동사를 쓰라는 안내가 있어야 한다");
}

ALICE_TEST(ExpressionDiagnostics, UnknownNameGetsSuggestion) {
    DiagnosticBag bag;
    ValidateExpression("physics.grouned", SymbolTable::Core(), bag);

    const Diagnostic* d = Get(bag, "expr.unknown_name");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK_MSG(d->hint.find("physics.grounded") != std::string::npos,
                    "오타에는 가장 가까운 심볼을 제안해야 한다");
}

ALICE_TEST(ExpressionDiagnostics, FunctionWithoutParensIsCaught) {
    DiagnosticBag bag;
    ValidateExpression("input.pressed", SymbolTable::Core(), bag);
    ALICE_CHECK(HasCode(bag, "expr.function_not_called"));
}

ALICE_TEST(ExpressionDiagnostics, PropertyCalledLikeFunctionIsCaught) {
    DiagnosticBag bag;
    ValidateExpression("physics.grounded()", SymbolTable::Core(), bag);
    ALICE_CHECK(HasCode(bag, "expr.property_called"));
}

ALICE_TEST(ExpressionDiagnostics, WrongArityIsCaught) {
    DiagnosticBag bag;
    ValidateExpression("math.clamp(1)", SymbolTable::Core(), bag);

    const Diagnostic* d = Get(bag, "expr.wrong_arity");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK(d->message.find("3") != std::string::npos);
}

ALICE_TEST(ExpressionDiagnostics, ValidExpressionsPass) {
    const char* valid[] = {
        "input.pressed(\"Jump\") and physics.grounded",
        "physics.speed > 0.1",
        "math.clamp(physics.speed, 0, 10) > 5",
        "not self.active",
        "time.since(time.now) < 1.5",
        "actor.count(\"enemy\") == 0",
    };
    for (const char* text : valid) {
        DiagnosticBag bag;
        ValidateExpression(text, SymbolTable::Core(), bag);
        ALICE_CHECK_MSG(!bag.HasErrors(), Fmt("'{}' 는 통과해야 한다:\n{}", text, bag.ToPretty()));
    }
}

ALICE_TEST(ExpressionDiagnostics, UnclosedParenIsCaught) {
    DiagnosticBag bag;
    ExprPtr expr;
    ParseExpression("math.abs(1", expr, bag);
    ALICE_CHECK(HasCode(bag, "expr.unclosed_call") || HasCode(bag, "expr.unclosed_paren"));
}

// ── 동사 레지스트리 ────────────────────────────────────────────────────────
ALICE_TEST(VerbRegistryTest, CoreVerbsAreRegistered) {
    const VerbRegistry& verbs = VerbRegistry::Global();

    const char* required[] = {
        "transform.setPosition", "physics.impulse", "audio.play",
        "anim.play", "vfx.spawn", "actor.spawn", "scene.load",
        "camera.shake", "var.set", "wait", "log.write", "profile.zone",
    };
    for (const char* id : required) {
        ALICE_CHECK_MSG(verbs.Has(id), Fmt("코어 동사 '{}' 가 등록되지 않았다", id));
    }
    ALICE_CHECK(verbs.Size() >= 25);
}

ALICE_TEST(VerbRegistryTest, EveryVerbHasSummaryAndTags) {
    const VerbRegistry& verbs = VerbRegistry::Global();
    for (const std::string& id : verbs.Ids()) {
        const Verb* v = verbs.Find(id);
        ALICE_REQUIRE(v != nullptr);
        ALICE_CHECK_MSG(!v->summary.empty(), Fmt("동사 '{}' 에 설명이 없다", id));
        ALICE_CHECK_MSG(!v->tags.empty(), Fmt("동사 '{}' 에 태그가 없다", id));
    }
}

ALICE_TEST(VerbRegistryTest, JsonReferenceIsMachineReadable) {
    const std::string json =
        VerbRegistry::Global().ToJson(schema::Registry::Global());

    ALICE_CHECK(json.find("\"verbs\"") != std::string::npos);
    ALICE_CHECK(json.find("audio.play") != std::string::npos);
    ALICE_CHECK(json.find("\"deterministic\"") != std::string::npos);

    DiagnosticBag bag;
    Value parsed;
    ALICE_CHECK_MSG(doc::ParseJson(json, parsed, bag),
                    "동사 레퍼런스는 반드시 파싱 가능한 JSON 이어야 한다:\n" + bag.ToPretty());
}

ALICE_TEST(VerbRegistryTest, NonDeterministicVerbsAreMarked) {
    const VerbRegistry& verbs = VerbRegistry::Global();
    // 난수·변주가 들어가는 것들은 반드시 비결정적으로 표시되어야 한다.
    // 리플레이·결정적 테스트가 이 플래그를 믿고 동작한다.
    const Verb* play = verbs.Find("audio.play");
    ALICE_REQUIRE(play != nullptr);
    ALICE_CHECK(!play->deterministic);

    const Verb* shake = verbs.Find("camera.shake");
    ALICE_REQUIRE(shake != nullptr);
    ALICE_CHECK(!shake->deterministic);

    const Verb* setPos = verbs.Find("transform.setPosition");
    ALICE_REQUIRE(setPos != nullptr);
    ALICE_CHECK(setPos->deterministic);
}

// ── 2차 검사 ───────────────────────────────────────────────────────────────
ALICE_TEST(SemanticCheckTest, ValidBehaviorPasses) {
    const Value doc = ParseYamlOrFail(
        "schema: alice/behavior/1\n"
        "name: Jump\n"
        "rules:\n"
        "  - when: input.pressed(\"Jump\") and physics.grounded\n"
        "    do:\n"
        "      - physics.impulse: { direction: [0, 1, 0], force: 5.0 }\n"
        "      - audio.play: { sound: sounds/jump.sound.yaml }\n", aliceCtx);

    DiagnosticBag bag;
    CheckDocumentSemantics(doc, VerbRegistry::Global(), schema::Registry::Global(),
                           bag, "jump.behavior.yaml");
    ALICE_CHECK_MSG(!bag.HasErrors(), bag.ToPretty());
}

ALICE_TEST(SemanticCheckTest, UnknownVerbGetsSuggestion) {
    const Value doc = ParseYamlOrFail(
        "rules:\n"
        "  - when: physics.grounded\n"
        "    do:\n"
        "      - audio.paly: { sound: sounds/x.sound.yaml }\n", aliceCtx);

    DiagnosticBag bag;
    CheckDocumentSemantics(doc, VerbRegistry::Global(), schema::Registry::Global(), bag);

    const Diagnostic* d = Get(bag, "verb.unknown");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK_MSG(d->hint.find("audio.play") != std::string::npos,
                    "동사 오타에는 가장 가까운 동사를 제안해야 한다");
}

ALICE_TEST(SemanticCheckTest, MissingVerbArgumentIsCaught) {
    const Value doc = ParseYamlOrFail(
        "rules:\n"
        "  - when: physics.grounded\n"
        "    do:\n"
        "      - physics.impulse: { direction: [0, 1, 0] }\n", aliceCtx);

    DiagnosticBag bag;
    CheckDocumentSemantics(doc, VerbRegistry::Global(), schema::Registry::Global(), bag);

    const Diagnostic* d = Get(bag, "schema.missing_field");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK_MSG(d->message.find("physics.impulse") != std::string::npos,
                    "인자 진단에는 어느 동사의 인자인지가 들어가야 한다");
    ALICE_CHECK(d->message.find("force") != std::string::npos);
}

ALICE_TEST(SemanticCheckTest, ActionWithTwoKeysIsRejected) {
    const Value doc = ParseYamlOrFail(
        "rules:\n"
        "  - when: physics.grounded\n"
        "    do:\n"
        "      - anim.trigger: { parameter: Jump }\n"
        "        wait: { seconds: 1 }\n", aliceCtx);

    DiagnosticBag bag;
    CheckDocumentSemantics(doc, VerbRegistry::Global(), schema::Registry::Global(), bag);
    ALICE_CHECK(HasCode(bag, "verb.action_multiple_keys"));
}

ALICE_TEST(SemanticCheckTest, LocalVariablesAreVisibleToExpressions) {
    const Value doc = ParseYamlOrFail(
        "variables:\n"
        "  comboCount: 0\n"
        "rules:\n"
        "  - when: comboCount > 3\n"
        "    do:\n"
        "      - var.set: { name: comboCount, value: 0 }\n", aliceCtx);

    DiagnosticBag bag;
    CheckDocumentSemantics(doc, VerbRegistry::Global(), schema::Registry::Global(), bag);
    ALICE_CHECK_MSG(!bag.HasErrors(), bag.ToPretty());
}

ALICE_TEST(SemanticCheckTest, UndeclaredVariableIsCaught) {
    const Value doc = ParseYamlOrFail(
        "rules:\n"
        "  - when: comboCount > 3\n"
        "    do:\n"
        "      - wait: { seconds: 1 }\n", aliceCtx);

    DiagnosticBag bag;
    CheckDocumentSemantics(doc, VerbRegistry::Global(), schema::Registry::Global(), bag);
    ALICE_CHECK(HasCode(bag, "expr.unknown_name"));
}

ALICE_TEST(SemanticCheckTest, DiagnosticCarriesDocumentPath) {
    const Value doc = ParseYamlOrFail(
        "rules:\n"
        "  - when: nope.nothing\n"
        "    do:\n"
        "      - wait: { seconds: 1 }\n", aliceCtx);

    DiagnosticBag bag;
    CheckDocumentSemantics(doc, VerbRegistry::Global(), schema::Registry::Global(),
                           bag, "x.behavior.yaml");

    const Diagnostic* d = Get(bag, "expr.unknown_name");
    ALICE_REQUIRE(d != nullptr);
    ALICE_CHECK_STR(d->file, "x.behavior.yaml");
    ALICE_CHECK_MSG(d->path.find("rules[0].when") != std::string::npos,
                    Fmt("문서 안 경로가 있어야 한다. 실제: '{}'", d->path));
}

ALICE_TEST(SemanticCheckTest, AnimationTransitionsAreChecked) {
    // animation 문서의 전이 조건도 같은 규칙으로 검사되어야 한다.
    const Value doc = ParseYamlOrFail(
        "parameters:\n"
        "  speed: { type: float, default: 0 }\n"
        "states:\n"
        "  - name: Idle\n"
        "    clip: Idle\n"
        "    transitions:\n"
        "      - to: Run\n"
        "        when: speed > 0.1\n"
        "      - to: Fall\n"
        "        when: spede < 0\n", aliceCtx);

    DiagnosticBag bag;
    CheckDocumentSemantics(doc, VerbRegistry::Global(), schema::Registry::Global(), bag);

    const Diagnostic* d = Get(bag, "expr.unknown_name");
    ALICE_REQUIRE_MSG(d != nullptr, "오타 난 파라미터를 잡아야 한다");
    ALICE_CHECK(d->hint.find("speed") != std::string::npos);
}
