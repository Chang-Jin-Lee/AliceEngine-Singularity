// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Verbs/CoreVerbs.cpp
//
// 엔진 기본 동사와 식 심볼의 목록.
//
// 이 파일이 곧 "이 엔진에서 게임이 할 수 있는 일"의 정의다. 목록이 유한하고,
// 각 항목이 스키마와 예시를 들고 있으므로 AI 는 추측할 필요가 없다.
//
// 새 기능을 엔진에 추가한다는 것은 여기에 동사를 하나 더 등록한다는 뜻이다.
// 등록하는 순간 검증·자동완성·위키·`alice verbs` 가 전부 따라온다.
#include "Expression.h"
#include "Schema/SchemaBuilder.h"
#include "Verb.h"

namespace alice::verbs {
namespace {

using schema::SchemaBuilder;
using schema::SchemaPtr;
using schema::TextFormat;
using B = SchemaBuilder;

SchemaPtr TargetField() {
    schema::Schema s;
    s.type        = schema::Type::String;
    s.description = "대상 액터 이름. 비우면 이 행동이 붙은 액터 자신";
    return std::make_shared<const schema::Schema>(std::move(s));
}

Verb Make(std::string id, std::string summary, SchemaPtr args,
          std::vector<std::string> tags,
          Verb::Cost cost = Verb::Cost::Cheap,
          bool deterministic = true) {
    Verb v;
    v.id            = std::move(id);
    v.summary       = std::move(summary);
    v.args          = std::move(args);
    v.tags          = std::move(tags);
    v.cost          = cost;
    v.deterministic = deterministic;
    return v;
}

Value ExampleOf(std::string verbId, Value args) {
    Value v = Value::MakeMap();
    v.Set(std::move(verbId), std::move(args));
    return v;
}

// ── 트랜스폼 ───────────────────────────────────────────────────────────────
void RegisterTransformVerbs(VerbRegistry& r) {
    r.Register(Make("transform.setPosition", "액터를 특정 위치로 옮긴다",
        B::Map()
            .Field("target", TargetField())
            .Field("position", B::Vec3(), "월드 좌표 [x, y, z]").Require()
            .Build(),
        {"transform"}, Verb::Cost::Trivial));

    r.Register(Make("transform.translate", "현재 위치에서 상대적으로 움직인다",
        B::Map()
            .Field("target", TargetField())
            .Field("delta", B::Vec3(), "이동량 [x, y, z]").Require()
            .Field("space", B::Enum({"world", "local"}), "기준 좌표계")
                .Default(Value{"world"})
            .Build(),
        {"transform"}, Verb::Cost::Trivial));

    r.Register(Make("transform.setRotation", "회전을 설정한다",
        B::Map()
            .Field("target", TargetField())
            .Field("rotation", B::Vec3(), "오일러 각(도) [pitch, yaw, roll]").Require()
            .Build(),
        {"transform"}, Verb::Cost::Trivial));

    r.Register(Make("transform.rotate", "현재 회전에서 상대적으로 돌린다",
        B::Map()
            .Field("target", TargetField())
            .Field("delta", B::Vec3(), "회전량(도)").Require()
            .Field("space", B::Enum({"world", "local"}), "기준 좌표계")
                .Default(Value{"local"})
            .Build(),
        {"transform"}, Verb::Cost::Trivial));

    r.Register(Make("transform.lookAt", "대상을 바라보게 회전시킨다",
        B::Map()
            .Field("target", TargetField())
            .Field("at", B::Union({B::Vec3(), B::String()}).Build(),
                   "바라볼 좌표 또는 액터 이름").Require()
            .Field("up", B::Vec3(), "위쪽 방향")
            .Build(),
        {"transform"}, Verb::Cost::Trivial));

    r.Register(Make("transform.setScale", "크기를 설정한다",
        B::Map()
            .Field("target", TargetField())
            .Field("scale", B::Vec3(), "축별 배율").Require()
            .Build(),
        {"transform"}, Verb::Cost::Trivial));
}

// ── 물리 ───────────────────────────────────────────────────────────────────
void RegisterPhysicsVerbs(VerbRegistry& r) {
    Verb impulse = Make("physics.impulse", "강체에 순간적인 충격을 준다",
        B::Map()
            .Field("target", TargetField())
            .Field("direction", B::Vec3(), "방향 벡터. 자동으로 정규화된다").Require()
            .Field("force", B::Float(), "충격 크기(N·s)").Require()
            .Build(),
        {"physics"}, Verb::Cost::Cheap);
    {
        Value args = Value::MakeMap();
        args.Set("direction", schema::MakeNumberSeq({0, 1, 0}));
        args.Set("force", Value{5.0});
        impulse.examples.push_back(ExampleOf("physics.impulse", std::move(args)));
    }
    r.Register(std::move(impulse));

    r.Register(Make("physics.force", "매 프레임 지속되는 힘을 가한다",
        B::Map()
            .Field("target", TargetField())
            .Field("direction", B::Vec3(), "방향 벡터").Require()
            .Field("force", B::Float(), "힘의 크기(N)").Require()
            .Build(),
        {"physics"}, Verb::Cost::Cheap));

    r.Register(Make("physics.setVelocity", "속도를 직접 지정한다",
        B::Map()
            .Field("target", TargetField())
            .Field("velocity", B::Vec3(), "m/s").Require()
            .Build(),
        {"physics"}, Verb::Cost::Trivial));

    r.Register(Make("physics.teleport",
        "물리 보간 없이 즉시 이동시킨다. 순간이동에는 setPosition 대신 이것을 쓴다",
        B::Map()
            .Field("target", TargetField())
            .Field("position", B::Vec3(), "월드 좌표").Require()
            .Field("resetVelocity", B::Bool(), "속도를 0 으로 만들지")
                .Default(Value{true})
            .Build(),
        {"physics"}, Verb::Cost::Cheap));
}

// ── 오디오 ─────────────────────────────────────────────────────────────────
void RegisterAudioVerbs(VerbRegistry& r) {
    Verb play = Make("audio.play", "sound 문서를 재생한다",
        B::Map()
            .Field("sound", B::Text(TextFormat::AssetPath), "sound 문서 경로").Require()
            .Field("target", TargetField())
            .Field("volume", B::Float(), "0..1. 문서의 값에 곱해진다")
                .Default(Value{1.0}).Range(0.0, 1.0)
            .Field("pitch", B::Float(), "재생 속도 배율").Default(Value{1.0})
            .Build(),
        {"audio"}, Verb::Cost::Cheap, /*deterministic=*/false);
    play.description =
        "sound 문서의 pickMode 에 따라 변주를 고르므로 결과가 매번 다를 수 있다. "
        "결정적 재생이 필요하면 변주가 하나뿐인 문서를 쓰라";
    {
        Value args = Value::MakeMap();
        args.Set("sound", Value{"sounds/jump.sound.yaml"});
        play.examples.push_back(ExampleOf("audio.play", std::move(args)));
    }
    r.Register(std::move(play));

    r.Register(Make("audio.stop", "재생 중인 소리를 멈춘다",
        B::Map()
            .Field("sound", B::Text(TextFormat::AssetPath), "멈출 sound 문서. 비우면 대상의 모든 소리")
            .Field("target", TargetField())
            .Field("fadeOut", B::Float(), "페이드아웃 시간(초)").Default(Value{0.0})
            .Build(),
        {"audio"}, Verb::Cost::Trivial));

    r.Register(Make("audio.setBusVolume", "믹서 버스의 음량을 바꾼다",
        B::Map()
            .Field("bus", B::Enum({"master", "music", "sfx", "voice", "ambient", "ui"}),
                   "대상 버스").Require()
            .Field("volume", B::Float(), "0..1").Require().Range(0.0, 1.0)
            .Field("fade", B::Float(), "변화에 걸릴 시간(초)").Default(Value{0.0})
            .Build(),
        {"audio"}, Verb::Cost::Trivial));
}

// ── 애니메이션 ─────────────────────────────────────────────────────────────
void RegisterAnimationVerbs(VerbRegistry& r) {
    r.Register(Make("anim.play", "상태 그래프의 특정 상태로 즉시 전환한다",
        B::Map()
            .Field("target", TargetField())
            .Field("state", B::String(), "animation 문서의 상태 이름").Require()
            .Field("blend", B::Float(), "블렌드 시간(초)").Default(Value{0.15})
            .Build(),
        {"animation"}, Verb::Cost::Cheap));

    r.Register(Make("anim.set", "애니메이션 파라미터 값을 설정한다",
        B::Map()
            .Field("target", TargetField())
            .Field("parameter", B::Text(TextFormat::Identifier), "파라미터 이름").Require()
            .Field("value", B::Union({B::Number(), B::Bool()}).Build(), "새 값").Require()
            .Build(),
        {"animation"}, Verb::Cost::Trivial));

    r.Register(Make("anim.trigger", "trigger 타입 파라미터를 한 프레임 켠다",
        B::Map()
            .Field("target", TargetField())
            .Field("parameter", B::Text(TextFormat::Identifier), "트리거 이름").Require()
            .Build(),
        {"animation"}, Verb::Cost::Trivial));

    r.Register(Make("anim.setSpeed", "재생 속도 배율을 바꾼다",
        B::Map()
            .Field("target", TargetField())
            .Field("speed", B::Float(), "1 이 원래 속도").Require()
            .Build(),
        {"animation"}, Verb::Cost::Trivial));
}

// ── 이펙트 ─────────────────────────────────────────────────────────────────
void RegisterEffectVerbs(VerbRegistry& r) {
    r.Register(Make("vfx.spawn", "effect 문서를 한 번 재생한다",
        B::Map()
            .Field("effect", B::Text(TextFormat::AssetPath), "effect 문서 경로").Require()
            .Field("position", B::Vec3(), "월드 좌표. 비우면 target 위치")
            .Field("target", TargetField())
            .Field("attach", B::Bool(), "대상을 따라다닐지").Default(Value{false})
            .Field("scale", B::Float(), "전체 크기 배율").Default(Value{1.0})
            .Build(),
        {"vfx"}, Verb::Cost::Moderate, /*deterministic=*/false));

    r.Register(Make("vfx.stop", "재생 중인 이펙트를 멈춘다",
        B::Map()
            .Field("effect", B::Text(TextFormat::AssetPath), "멈출 effect 문서")
            .Field("target", TargetField())
            .Field("immediate", B::Bool(), "남은 파티클까지 즉시 지울지")
                .Default(Value{false})
            .Build(),
        {"vfx"}, Verb::Cost::Cheap));
}

// ── 액터 / 씬 ──────────────────────────────────────────────────────────────
void RegisterActorVerbs(VerbRegistry& r) {
    r.Register(Make("actor.spawn", "actor 문서로부터 새 액터를 만든다",
        B::Map()
            .Field("actor", B::Text(TextFormat::AssetPath), "actor 문서 경로").Require()
            .Field("name", B::String(), "새 액터의 이름. 비우면 자동 생성")
            .Field("position", B::Vec3(), "월드 좌표")
            .Field("rotation", B::Vec3(), "오일러 각(도)")
            .Field("parent", B::String(), "부모 액터 이름")
            .Build(),
        {"actor", "scene"}, Verb::Cost::Expensive));

    r.Register(Make("actor.destroy", "액터를 제거한다",
        B::Map()
            .Field("target", TargetField())
            .Field("delay", B::Float(), "지연 시간(초)").Default(Value{0.0})
            .Build(),
        {"actor"}, Verb::Cost::Cheap));

    r.Register(Make("actor.setActive", "액터를 켜거나 끈다",
        B::Map()
            .Field("target", TargetField())
            .Field("active", B::Bool(), "켤지 끌지").Require()
            .Build(),
        {"actor"}, Verb::Cost::Trivial));

    r.Register(Make("actor.setTag", "액터에 태그를 붙이거나 뗀다",
        B::Map()
            .Field("target", TargetField())
            .Field("tag", B::Text(TextFormat::Identifier), "태그 이름").Require()
            .Field("value", B::Bool(), "true 면 붙이고 false 면 뗀다").Default(Value{true})
            .Build(),
        {"actor"}, Verb::Cost::Trivial));

    r.Register(Make("scene.load", "다른 씬으로 전환한다",
        B::Map()
            .Field("scene", B::Text(TextFormat::AssetPath), "scene 문서 경로").Require()
            .Field("mode", B::Enum({"replace", "additive"}), "기존 씬을 대체할지 겹칠지")
                .Default(Value{"replace"})
            .Build(),
        {"scene"}, Verb::Cost::Expensive));
}

// ── 카메라 ─────────────────────────────────────────────────────────────────
void RegisterCameraVerbs(VerbRegistry& r) {
    r.Register(Make("camera.activate", "이 카메라를 화면 담당으로 만든다",
        B::Map()
            .Field("target", TargetField())
            .Field("blend", B::Float(), "전환에 걸릴 시간(초)").Default(Value{0.0})
            .Build(),
        {"camera"}, Verb::Cost::Cheap));

    r.Register(Make("camera.shake", "카메라를 흔든다",
        B::Map()
            .Field("amplitude", B::Float(), "흔들림 크기(미터)").Require()
            .Field("duration", B::Float(), "지속 시간(초)").Require()
            .Field("frequency", B::Float(), "초당 흔들림 횟수").Default(Value{20.0})
            .Build(),
        {"camera"}, Verb::Cost::Cheap, /*deterministic=*/false));
}

// ── 변수 / 흐름 ────────────────────────────────────────────────────────────
void RegisterFlowVerbs(VerbRegistry& r) {
    Verb setVar = Make("var.set", "행동의 지역 변수 값을 바꾼다",
        B::Map()
            .Field("name", B::Text(TextFormat::Identifier), "변수 이름").Require()
            .Field("value", B::Union({B::Number(), B::Bool(), B::String()}).Build(), "새 값")
                .Require()
            .Build(),
        {"flow"}, Verb::Cost::Trivial);
    setVar.description =
        "식(when:)에서는 값을 바꿀 수 없다. 상태를 바꾸는 일은 전부 동사가 한다. "
        "그 규칙이 있어야 규칙들의 평가 순서가 결과에 영향을 주지 않는다";
    r.Register(std::move(setVar));

    r.Register(Make("var.add", "숫자 변수에 값을 더한다",
        B::Map()
            .Field("name", B::Text(TextFormat::Identifier), "변수 이름").Require()
            .Field("amount", B::Number(), "더할 값. 음수면 뺀다").Require()
            .Field("min", B::Number(), "결과의 하한")
            .Field("max", B::Number(), "결과의 상한")
            .Build(),
        {"flow"}, Verb::Cost::Trivial));

    r.Register(Make("wait", "다음 동작까지 기다린다",
        B::Map()
            .Field("seconds", B::Float(), "대기 시간(초)").Require().Min(0.0)
            .Build(),
        {"flow"}, Verb::Cost::Trivial));
}

// ── 진단 (요구 2: 처음부터 프로파일링과 로깅) ──────────────────────────────
void RegisterDiagnosticVerbs(VerbRegistry& r) {
    Verb log = Make("log.write", "구조화 로그를 남긴다",
        B::Map()
            .Field("level", B::Enum({"trace", "debug", "info", "warn", "error"}), "로그 레벨")
                .Default(Value{"info"})
            .Field("event", B::Text(TextFormat::VerbId),
                   "안정 이벤트 id. 예) gameplay.player.died").Require()
            .Field("message", B::String(), "사람이 읽을 한 문장")
            .Field("fields", B::Map().AllowExtraFields().Build(),
                   "기계가 읽을 키-값. 숫자는 숫자로 남겨라")
            .Build(),
        {"diagnostics"}, Verb::Cost::Trivial);
    log.description =
        "콘텐츠가 남긴 로그도 엔진 로그와 같은 NDJSON 스트림으로 나간다. "
        "그래서 AI 가 '이 규칙이 언제 몇 번 발동했나'를 엔진 이벤트와 나란히 놓고 볼 수 있다. "
        "게임플레이 버그를 쫓을 때 이게 가장 빠른 길이다";
    {
        Value args = Value::MakeMap();
        args.Set("event", Value{"gameplay.player.jumped"});
        Value fields = Value::MakeMap();
        fields.Set("height", Value{2.4});
        args.Set("fields", std::move(fields));
        log.examples.push_back(ExampleOf("log.write", std::move(args)));
    }
    r.Register(std::move(log));

    Verb mark = Make("profile.zone", "이 동작 묶음을 프로파일러 존으로 감싼다",
        B::Map()
            .Field("name", B::String(), "프로파일러에 표시될 이름").Require()
            .Field("budgetMs", B::Float(), "프레임 예산(밀리초). 넘으면 경고 로그가 나간다")
            .Build(),
        {"diagnostics"}, Verb::Cost::Trivial);
    mark.description =
        "콘텐츠가 만든 비용도 프로파일러에서 엔진 비용과 같은 축에 보이게 한다. "
        "budgetMs 를 주면 초과 시 perf.budget.exceeded 가 나가므로 회귀를 CI 에서 잡을 수 있다";
    r.Register(std::move(mark));

    r.Register(Make("debug.draw", "디버그 도형을 한 프레임 그린다",
        B::Map()
            .Field("shape", B::Enum({"line", "sphere", "box", "arrow", "text"}), "도형").Require()
            .Field("from", B::Vec3(), "시작점 또는 중심")
            .Field("to", B::Vec3(), "끝점. line/arrow 전용")
            .Field("size", B::Vec3(), "box 크기")
            .Field("radius", B::Float(), "sphere 반지름")
            .Field("text", B::String(), "text 도형의 내용")
            .Field("color", B::Color(), "색").Default(Value{"#00ff00"})
            .Field("duration", B::Float(), "표시 시간(초). 0 이면 한 프레임")
                .Default(Value{0.0})
            .Build(),
        {"diagnostics"}, Verb::Cost::Cheap));
}

} // namespace

void RegisterCoreVerbs(VerbRegistry& registry) {
    RegisterTransformVerbs(registry);
    RegisterPhysicsVerbs(registry);
    RegisterAudioVerbs(registry);
    RegisterAnimationVerbs(registry);
    RegisterEffectVerbs(registry);
    RegisterActorVerbs(registry);
    RegisterCameraVerbs(registry);
    RegisterFlowVerbs(registry);
    RegisterDiagnosticVerbs(registry);
}

// ── 식에서 읽을 수 있는 이름들 ─────────────────────────────────────────────
void RegisterCoreSymbols(SymbolTable& t) {
    // 입력
    t.AddFunction("input.pressed", 1, 1, "bool",
                  "이번 프레임에 눌리기 시작한 행동인지. 인자는 input 문서의 행동 이름");
    t.AddFunction("input.released", 1, 1, "bool", "이번 프레임에 떼어진 행동인지");
    t.AddFunction("input.held", 1, 1, "bool", "지금 눌려 있는 행동인지");
    t.AddFunction("input.axis", 1, 1, "float", "축 행동의 값 (-1..1)");

    // 물리
    t.AddProperty("physics.grounded", "bool", "이 액터가 바닥에 닿아 있는지");
    t.AddProperty("physics.speed", "float", "속도의 크기(m/s)");
    t.AddProperty("physics.velocityY", "float", "수직 속도(m/s). 낙하 판정에 쓴다");
    t.AddFunction("physics.touching", 1, 1, "bool", "인자로 준 태그를 가진 것과 닿아 있는지");

    // 시간
    t.AddProperty("time.now", "float", "게임 시작으로부터의 초");
    t.AddProperty("time.delta", "float", "이전 프레임과의 간격(초)");
    t.AddProperty("time.frame", "int", "프레임 번호");
    t.AddFunction("time.since", 1, 1, "float", "인자로 준 시각 이후 흐른 초");

    // 자기 자신
    t.AddProperty("self.name", "string", "이 액터의 이름");
    t.AddProperty("self.active", "bool", "이 액터가 켜져 있는지");
    t.AddProperty("self.position", "vec3", "이 액터의 월드 위치");
    t.AddFunction("self.hasTag", 1, 1, "bool", "이 액터가 해당 태그를 가졌는지");

    // 액터 질의
    t.AddFunction("actor.exists", 1, 1, "bool", "그 이름의 액터가 씬에 있는지");
    t.AddFunction("actor.count", 1, 1, "int", "해당 태그를 가진 액터 수");
    t.AddFunction("actor.distance", 1, 1, "float", "이 액터와 그 이름의 액터 사이 거리(미터)");

    // 애니메이션
    t.AddProperty("anim.state", "string", "현재 애니메이션 상태 이름");
    t.AddProperty("anim.normalizedTime", "float", "현재 클립의 진행도 0..1");
    t.AddFunction("anim.finished", 1, 1, "bool", "그 이름의 클립이 끝났는지");

    // 씬
    t.AddProperty("scene.name", "string", "현재 씬 이름");

    // 수학
    t.AddFunction("math.abs", 1, 1, "float", "절댓값");
    t.AddFunction("math.min", 2, 2, "float", "둘 중 작은 값");
    t.AddFunction("math.max", 2, 2, "float", "둘 중 큰 값");
    t.AddFunction("math.clamp", 3, 3, "float", "값을 [최소, 최대] 안으로 자른다");
    t.AddFunction("math.lerp", 3, 3, "float", "선형 보간");
    t.AddFunction("math.sin", 1, 1, "float", "사인(라디안)");
    t.AddFunction("math.cos", 1, 1, "float", "코사인(라디안)");
    t.AddFunction("math.sqrt", 1, 1, "float", "제곱근");
    t.AddFunction("math.floor", 1, 1, "int", "내림");
    t.AddFunction("math.ceil", 1, 1, "int", "올림");
    t.AddFunction("math.round", 1, 1, "int", "반올림");
    // random 은 결정적이지 않다. 리플레이가 필요한 곳에서는 쓰지 말라고 설명에 못 박는다.
    t.AddFunction("math.random", 0, 2, "float",
                  "난수. 인자 없으면 0..1, 둘이면 그 범위. **비결정적이라 리플레이가 어긋난다**");

    t.AddProperty("math.pi", "float", "원주율");
}

} // namespace alice::verbs
