// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Schema/CoreSchemas.cpp
//
// **이 파일이 엔진의 콘텐츠 모델이다.**
//
// 언리얼이라면 UCLASS/UPROPERTY 로 흩어져 있고, 유니티라면 MonoBehaviour 수백 개에
// 흩어져 있을 정보가 여기 한곳에 모여 있다. 그렇게 한 이유는 하나다:
// **AI 가 "이 엔진으로 무엇을 만들 수 있는가"를 한 번에 읽을 수 있어야 한다.**
//
// 여기 등록된 것이 곧 alice CLI 가 답하는 목록이고, 위키 페이지가 되고,
// IDE 자동완성이 되고, 검증 규칙이 된다. 소스는 하나뿐이다.
//
// 새 콘텐츠 타입을 추가한다는 것은 이 파일에 스키마를 하나 더 등록한다는 뜻이다.
// 그 순간 검증·문서·자동완성이 전부 따라온다.
#include "Foundation/StringUtil.h"
#include "Registry.h"
#include "SchemaBuilder.h"

namespace alice::schema {
namespace {

using B = SchemaBuilder;

// ── 공통 조각 ──────────────────────────────────────────────────────────────

SchemaPtr SchemaField() {
    return B::Text(TextFormat::SchemaId);
}

SchemaPtr AssetRef() {
    return B::Text(TextFormat::AssetPath);
}

SchemaPtr Name() {
    Schema s;
    s.type        = Type::String;
    s.minLength   = 1;
    s.description = "사람이 읽는 이름. 문서 안에서 참조 대상이 된다";
    return std::make_shared<const Schema>(std::move(s));
}

Value Vec3Value(f64 x, f64 y, f64 z) { return MakeNumberSeq({x, y, z}); }

// ── 트랜스폼 ───────────────────────────────────────────────────────────────
SchemaPtr Transform() {
    return B::Map("alice/component/transform/1")
        .Title("Transform")
        .Describe("월드 공간에서의 위치·회전·크기. 모든 액터가 암묵적으로 하나 가진다")
        .Field("position", B::Vec3(), "미터 단위 위치 [x, y, z]")
            .Default(Vec3Value(0, 0, 0))
        .Field("rotation", B::Vec3(), "오일러 각(도) [pitch, yaw, roll]")
            .Default(Vec3Value(0, 0, 0))
        .Field("scale", B::Vec3(), "축별 배율. 1 이 원본 크기")
            .Default(Vec3Value(1, 1, 1))
        .Pitfall("position: 0",
                 "position: [0, 0, 0]",
                 "위치는 항상 3개짜리 배열이다. 숫자 하나로 줄여 쓸 수 없다")
        .Build();
}

// ── 컴포넌트들 ─────────────────────────────────────────────────────────────
SchemaPtr MeshComponent() {
    return B::Map("alice/component/mesh/1")
        .Title("Mesh")
        .Describe("정적/스키닝 메시를 그린다")
        .Field("asset", AssetRef(), "메시 애셋 경로").Require()
        .Field("materials", B::Seq(AssetRef()).Build(),
               "서브메시 순서대로의 머티리얼. 개수가 모자라면 마지막 것이 반복된다")
        .Field("castShadow", B::Bool(), "그림자를 드리울지").Default(Value{true})
        .Field("receiveShadow", B::Bool(), "그림자를 받을지").Default(Value{true})
        .Field("visible", B::Bool(), "화면에 보일지").Default(Value{true})
        .Field("lodBias", B::Float(), "LOD 전환 거리 배율. 1 이 기본")
            .Default(Value{1.0})
        .Build();
}

SchemaPtr CameraComponent() {
    return B::Map("alice/component/camera/1")
        .Title("Camera")
        .Describe("장면을 촬영한다. priority 가 가장 높은 카메라가 화면을 담당한다")
        .Field("projection", B::Enum({"perspective", "orthographic"}), "투영 방식")
            .Default(Value{"perspective"})
        .Field("fieldOfView", B::Float(), "수직 화각(도). perspective 일 때만 쓴다")
            .Default(Value{60.0})
        .Field("orthographicSize", B::Float(), "세로 절반 크기(미터). orthographic 일 때만")
            .Default(Value{5.0})
        .Field("nearClip", B::Float(), "근평면 거리(미터)").Default(Value{0.1})
        .Field("farClip", B::Float(), "원평면 거리(미터)").Default(Value{1000.0})
        .Field("priority", B::Int(), "여러 카메라 중 우선순위. 큰 쪽이 이긴다")
            .Default(Value{static_cast<i64>(0)})
        .Field("clearColor", B::Color(), "배경색")
        .Pitfall("fieldOfView: 120",
                 "fieldOfView: 60",
                 "화각을 100도 이상으로 올리면 원근 왜곡이 심해지고 컬링 효율이 급락한다")
        .Build();
}

SchemaPtr LightComponent() {
    return B::Map("alice/component/light/1")
        .Title("Light")
        .Describe("장면을 밝힌다")
        .Field("type", B::Enum({"directional", "point", "spot"}), "광원 종류").Require()
        .Field("color", B::Color(), "빛의 색").Default(Value{"#ffffff"})
        .Field("intensity", B::Float(), "세기. directional 은 lux, 나머지는 lumen 기준")
            .Default(Value{1.0})
        .Field("range", B::Float(), "도달 거리(미터). point/spot 전용")
            .Default(Value{10.0})
        .Field("spotAngle", B::Float(), "스팟 원뿔 각도(도). spot 전용")
            .Default(Value{45.0})
        .Field("castShadow", B::Bool(), "그림자를 만들지. 켤수록 비싸다")
            .Default(Value{false})
        .Field("shadowBias", B::Float(), "그림자 여드름을 줄이는 오프셋")
            .Default(Value{0.005})
        .Build();
}

SchemaPtr Character2DComponent() {
    Value example = Value::MakeMap();
    example.Set("speed", Value{4.0});
    example.Set("jumpSpeed", Value{7.0});
    example.Set("gravityScale", Value{1.0});
    return B::Map("alice/component/character2d/1")
        .Title("Platformer Character 2D")
        .Describe("XY 평면의 비회전 사각형 캐릭터. box collider와 함께 사용하며 고정 지형과 충돌한다")
        .Field("speed", B::Float(), "좌우 이동 속도(m/s)").Default(Value{4.0}).Range(0.0, 100.0)
        .Field("jumpSpeed", B::Float(), "접지 상태에서 점프할 때 위쪽 초기 속도(m/s)").Default(Value{7.0}).Range(0.0, 100.0)
        .Field("gravityScale", B::Float(), "씬의 수직 중력 배율").Default(Value{1.0}).Range(0.0, 10.0)
        .Example(std::move(example))
        .Pitfall("rigidbody: {}", "character2d: { speed: 4, jumpSpeed: 7 }", "플랫폼어 컨트롤러는 일반 강체가 아니다. rigidbody와 동시에 사용하지 않는다")
        .Build();
}

SchemaPtr RigidbodyComponent() {
    return B::Map("alice/component/rigidbody/1")
        .Title("Rigidbody")
        .Describe("물리 시뮬레이션에 참여한다. Collider 가 함께 있어야 의미가 있다")
        .Field("mass", B::Float(), "질량(kg). kinematic 이면 무시된다")
            .Default(Value{1.0}).Min(0.0)
        .Field("kinematic", B::Bool(), "직접 움직이고 힘을 받지 않을지")
            .Default(Value{false})
        .Field("useGravity", B::Bool(), "중력을 받을지").Default(Value{true})
        .Field("linearDamping", B::Float(), "선형 감쇠").Default(Value{0.0})
        .Field("angularDamping", B::Float(), "각 감쇠").Default(Value{0.05})
        .Field("freezePosition", B::Seq(B::Bool()).Length(3, 3).Build(),
               "축별 위치 고정 [x, y, z]")
        .Field("freezeRotation", B::Seq(B::Bool()).Length(3, 3).Build(),
               "축별 회전 고정 [x, y, z]")
        .Field("physicsMaterial", AssetRef(), "마찰·반발 계수를 담은 physics 문서")
        .Pitfall("mass: 0",
                 "kinematic: true",
                 "질량 0 은 물리적으로 정의되지 않는다. 힘을 받지 않게 하려면 kinematic 을 쓰라")
        .Build();
}

SchemaPtr ColliderComponent() {
    return B::Map("alice/component/collider/1")
        .Title("Collider")
        .Describe("충돌 형상. 모양에 따라 쓰는 필드가 다르다")
        .Field("shape", B::Enum({"box", "sphere", "capsule", "mesh"}), "충돌 형상").Require()
        .Field("size", B::Vec3(), "box 의 각 축 크기(미터)")
            .Default(Vec3Value(1, 1, 1))
        .Field("radius", B::Float(), "sphere/capsule 반지름(미터)").Default(Value{0.5})
        .Field("height", B::Float(), "capsule 전체 높이(미터)").Default(Value{2.0})
        .Field("asset", AssetRef(), "mesh 형상일 때의 충돌 메시")
        .Field("center", B::Vec3(), "액터 원점 기준 오프셋")
            .Default(Vec3Value(0, 0, 0))
        .Field("isTrigger", B::Bool(), "겹침만 감지하고 밀어내지 않을지")
            .Default(Value{false})
        .Field("layer", B::String(), "충돌 레이어 이름").Default(Value{"default"})
        .Build();
}

SchemaPtr AudioSourceComponent() {
    return B::Map("alice/component/audioSource/1")
        .Title("Audio Source")
        .Describe("이 액터의 위치에서 소리를 낸다")
        .Field("sound", AssetRef(), "재생할 sound 문서").Require()
        .Field("autoPlay", B::Bool(), "액터가 살아날 때 바로 재생할지")
            .Default(Value{false})
        .Field("loop", B::Bool(), "반복 재생").Default(Value{false})
        .Field("volume", B::Float(), "0..1").Default(Value{1.0}).Range(0.0, 1.0)
        .Field("pitch", B::Float(), "재생 속도 배율").Default(Value{1.0})
        .Field("spatial", B::Bool(), "3D 위치 기반으로 들릴지").Default(Value{true})
        .Field("minDistance", B::Float(), "이 거리 안에서는 감쇠 없음(미터)")
            .Default(Value{1.0})
        .Field("maxDistance", B::Float(), "이 거리 밖에서는 들리지 않음(미터)")
            .Default(Value{50.0})
        .Build();
}

SchemaPtr ParticlesComponent() {
    return B::Map("alice/component/particles/1")
        .Title("Particles")
        .Describe("effect 문서를 이 액터 위치에서 재생한다")
        .Field("effect", AssetRef(), "effect 문서 경로").Require()
        .Field("playOnStart", B::Bool(), "액터가 살아날 때 바로 재생")
            .Default(Value{true})
        .Field("loop", B::Bool(), "끝나면 다시 시작").Default(Value{false})
        .Build();
}

SchemaPtr AnimatorComponent() {
    return B::Map("alice/component/animator/1")
        .Title("Animator")
        .Describe("animation 문서의 상태 그래프를 이 액터의 스키닝 메시에 적용한다")
        .Field("animation", AssetRef(), "animation 문서 경로").Require()
        .Field("playbackSpeed", B::Float(), "재생 속도 배율").Default(Value{1.0})
        .Field("applyRootMotion", B::Bool(), "루트 본의 이동을 액터 트랜스폼에 반영할지")
            .Default(Value{false})
        .Build();
}

// ── 행동(스크립트 대체) ────────────────────────────────────────────────────
SchemaPtr Action() {
    // 동작 하나. { "<동사>": { 인자... } } 형태의 맵이다.
    // 동사 목록과 인자 스키마는 Verbs 모듈이 들고 있고, 2차 검증에서 확인한다.
    Schema s;
    s.type             = Type::Map;
    s.title            = "Action";
    s.description      = "동사 하나와 그 인자. 예) audio.play: { sound: sounds/jump.sound.yaml }";
    s.additionalFields = true;
    return std::make_shared<const Schema>(std::move(s));
}

SchemaPtr Rule() {
    return B::Map()
        .Title("Rule")
        .Describe("조건이 참이 되는 순간 동작들을 순서대로 실행한다")
        .Field("when", B::Text(TextFormat::Expression),
               "조건식. 예) input.pressed(\"Jump\") and physics.grounded").Require()
        .Field("do", B::Seq(Action()).MinItems(1).Build(), "실행할 동작 목록").Require()
        .Field("once", B::Bool(), "한 번만 실행하고 다시는 하지 않을지")
            .Default(Value{false})
        .Field("cooldown", B::Float(), "재실행까지의 최소 간격(초)")
            .Default(Value{0.0})
        .Build();
}

SchemaPtr Behavior() {
    Value example = Value::MakeMap();
    example.Set("when", Value{"input.pressed(\"Jump\") and physics.grounded"});

    return B::Map("alice/behavior/1")
        .Title("Behavior")
        .Describe(
            "액터의 동작을 규칙으로 적는다. **이 엔진에는 게임플레이 스크립트 언어가 없다.** "
            "언리얼의 C++/블루프린트, 유니티의 C# 이 있던 자리를 이 문서가 대신한다. "
            "규칙은 선언적이고, 순서에 의존하지 않으며, 조건이 참이 될 때만 실행된다")
        .Field("schema", SchemaField(), "항상 alice/behavior/1").Require()
        .Field("name", Name(), "이 행동 묶음의 이름").Require()
        .Field("rules", B::Seq(Rule()).MinItems(1).Build(), "규칙 목록").Require()
        .Field("variables", B::Map().AllowExtraFields(B::Number()).Build(),
               "이 행동이 쓰는 지역 변수와 초깃값")
        .Example(std::move(example))
        .Pitfall("do: audio.play",
                 "do:\n  - audio.play: { sound: ... }",
                 "do 는 항상 목록이고, 각 항목은 동사 하나를 키로 갖는 맵이다")
        .Build();
}

// ── 액터 ───────────────────────────────────────────────────────────────────
SchemaPtr Components() {
    // 알려진 컴포넌트만 허용한다. 오타를 조용히 통과시키지 않기 위해서다.
    // 새 컴포넌트를 만들면 여기에 한 줄 추가하면 되고, 그 즉시 자동완성과 검증이 붙는다.
    return B::Map()
        .Title("Components")
        .Describe("액터에 붙은 기능들. 키가 컴포넌트 종류다")
        .Field("mesh",        B::Ref("alice/component/mesh/1").Build())
        .Field("camera",      B::Ref("alice/component/camera/1").Build())
        .Field("light",       B::Ref("alice/component/light/1").Build())
        .Field("rigidbody",   B::Ref("alice/component/rigidbody/1").Build())
        .Field("character2d", B::Ref("alice/component/character2d/1").Build())
        .Field("collider",    B::Ref("alice/component/collider/1").Build())
        .Field("audioSource", B::Ref("alice/component/audioSource/1").Build())
        .Field("particles",   B::Ref("alice/component/particles/1").Build())
        .Field("animator",    B::Ref("alice/component/animator/1").Build())
        .Build();
}

SchemaPtr Actor() {
    return B::Map("alice/actor/1")
        .Title("Actor")
        .Describe("장면에 놓이는 개체 하나. 트랜스폼과 컴포넌트, 그리고 행동으로 이루어진다")
        .Field("schema", SchemaField(), "독립 파일일 때는 alice/actor/1. 씬에 인라인이면 생략")
        .Field("name", Name(), "액터 이름. 같은 씬 안에서 유일해야 한다").Require()
        .Field("tags", B::Seq(B::String()).Build(),
               "질의용 태그. 예) [player, damageable]")
        .Field("active", B::Bool(), "처음부터 켜져 있을지").Default(Value{true})
        .Field("transform", B::Ref("alice/component/transform/1").Build(), "위치·회전·크기")
        .Field("components", Components(), "붙은 기능들")
        .Field("behaviors", B::Seq(AssetRef()).Build(),
               "적용할 behavior 문서 경로 목록")
        .Field("children", B::Seq(B::Ref("alice/actor/1").Build()).Build(),
               "부모-자식 계층. 자식 트랜스폼은 부모 기준이다")
        .Build();
}

// ── 씬 ─────────────────────────────────────────────────────────────────────
SchemaPtr Environment() {
    return B::Map()
        .Title("Environment")
        .Describe("씬 전체에 걸리는 설정")
        .Field("skybox", AssetRef(), "HDR 스카이박스 텍스처")
        .Field("ambientColor", B::Color(), "전역 앰비언트 색")
            .Default(Value{"#202028"})
        .Field("ambientIntensity", B::Float(), "앰비언트 세기").Default(Value{1.0})
        .Field("gravity", B::Vec3(), "중력 가속도(m/s²)")
            .Default(Vec3Value(0, -9.81, 0))
        .Field("fogEnabled", B::Bool(), "안개를 쓸지").Default(Value{false})
        .Field("fogColor", B::Color(), "안개 색").Default(Value{"#8090a0"})
        .Field("fogDensity", B::Float(), "안개 밀도").Default(Value{0.02})
        .Build();
}

SchemaPtr Scene() {
    return B::Map("alice/scene/1")
        .Title("Scene")
        .Describe("액터들이 놓인 하나의 세계. 게임은 씬의 연속이다")
        .Field("schema", SchemaField(), "항상 alice/scene/1").Require()
        .Field("name", Name(), "씬 이름").Require()
        .Field("environment", Environment(), "하늘·중력·안개 같은 전역 설정")
        .Field("actors", B::Seq(B::Ref("alice/actor/1").Build()).Build(), "이 씬의 액터들")
        .Field("preload", B::Seq(AssetRef()).Build(),
               "씬 진입 전에 미리 올려둘 애셋. 첫 프레임 히칭을 막는다")
        .Build();
}

// ── 머티리얼 ───────────────────────────────────────────────────────────────
SchemaPtr TextureBinding() {
    return B::Map()
        .Title("Texture Binding")
        .Field("slot", B::Enum({"baseColor", "normal", "metallicRoughness",
                                "occlusion", "emissive", "opacity", "custom0", "custom1"}),
               "셰이더의 어느 슬롯에 꽂을지").Require()
        .Field("asset", AssetRef(), "텍스처 애셋 경로").Require()
        .Field("wrap", B::Enum({"repeat", "clamp", "mirror"}), "UV 반복 방식")
            .Default(Value{"repeat"})
        .Field("filter", B::Enum({"linear", "point", "anisotropic"}), "샘플링 방식")
            .Default(Value{"linear"})
        .Field("tiling", B::Vec2(), "UV 배율")
        .Field("offset", B::Vec2(), "UV 오프셋")
        .Build();
}

SchemaPtr Material() {
    return B::Map("alice/material/1")
        .Title("Material")
        .Describe("표면이 빛에 어떻게 반응하는지. 셰이더 코드를 쓰지 않고 값으로 기술한다")
        .Field("schema", SchemaField(), "항상 alice/material/1").Require()
        .Field("name", Name(), "머티리얼 이름").Require()
        .Field("shader", B::Enum({"pbr", "unlit", "toon", "custom"}), "셰이딩 모델")
            .Default(Value{"pbr"})
        .Field("customShader", AssetRef(), "shader 가 custom 일 때의 셰이더 문서")
        .Field("blend", B::Enum({"opaque", "masked", "translucent", "additive"}),
               "합성 방식. opaque 가 가장 빠르다").Default(Value{"opaque"})
        .Field("doubleSided", B::Bool(), "뒷면도 그릴지. 켜면 드로우 비용이 는다")
            .Default(Value{false})
        .Field("params", B::Map()
                   .Title("Material Params")
                   .Field("baseColor", B::Color(), "기본 색")
                   .Field("metallic", B::Float(), "0=비금속, 1=금속").Range(0.0, 1.0)
                   .Field("roughness", B::Float(), "0=거울, 1=완전 확산").Range(0.0, 1.0)
                   .Field("emissive", B::Color(), "자체 발광 색")
                   .Field("emissiveIntensity", B::Float(), "발광 세기")
                   .Field("opacity", B::Float(), "불투명도").Range(0.0, 1.0)
                   .Field("alphaCutoff", B::Float(), "masked 일 때의 컷오프").Range(0.0, 1.0)
                   .Field("normalStrength", B::Float(), "노멀맵 강도")
                   .Build(),
               "셰이딩 파라미터")
        .Field("textures", B::Seq(TextureBinding()).Build(), "텍스처 바인딩 목록")
        .Pitfall("baseColor: #ff8800",
                 "baseColor: \"#ff8800\"",
                 "YAML 에서 공백 뒤의 # 는 주석이다. 색상 문자열은 반드시 따옴표로 감싼다")
        .Build();
}

// ── 애니메이션 ─────────────────────────────────────────────────────────────
SchemaPtr AnimClip() {
    return B::Map()
        .Title("Animation Clip")
        .Field("name", Name(), "그래프 안에서 부를 이름").Require()
        .Field("asset", AssetRef(), "애니메이션 애셋 경로").Require()
        .Field("loop", B::Bool(), "반복할지").Default(Value{true})
        .Field("speed", B::Float(), "재생 속도 배율").Default(Value{1.0})
        .Build();
}

SchemaPtr AnimTransition() {
    return B::Map()
        .Title("Transition")
        .Field("to", B::String(), "이동할 상태 이름").Require()
        .Field("when", B::Text(TextFormat::Expression),
               "전이 조건식. 예) speed > 0.1").Require()
        .Field("duration", B::Float(), "블렌드 시간(초)").Default(Value{0.2})
        .Field("exitTime", B::Float(), "0..1. 현재 클립의 이 지점 이후에만 전이")
        .Build();
}

SchemaPtr AnimState() {
    return B::Map()
        .Title("State")
        .Field("name", Name(), "상태 이름").Require()
        .Field("clip", B::String(), "재생할 클립 이름").Require()
        .Field("transitions", B::Seq(AnimTransition()).Build(), "나가는 전이들")
        .Build();
}

SchemaPtr AnimParameter() {
    return B::Map()
        .Title("Parameter")
        .Field("type", B::Enum({"float", "int", "bool", "trigger"}), "파라미터 타입").Require()
        .Field("default", B::Union({B::Number(), B::Bool()}).Build(), "초깃값")
        .Field("description", B::String(), "무엇을 뜻하는 값인지")
        .Build();
}

SchemaPtr Animation() {
    return B::Map("alice/animation/1")
        .Title("Animation")
        .Describe("클립과 상태 그래프. 블렌드와 전이를 값으로 기술한다")
        .Field("schema", SchemaField(), "항상 alice/animation/1").Require()
        .Field("name", Name(), "애니메이션 세트 이름").Require()
        .Field("clips", B::Seq(AnimClip()).MinItems(1).Build(), "쓸 클립들").Require()
        .Field("parameters", B::Map().AllowExtraFields(AnimParameter()).Build(),
               "전이 조건에서 참조할 파라미터들")
        .Field("states", B::Seq(AnimState()).MinItems(1).Build(), "상태 그래프").Require()
        .Field("defaultState", B::String(), "시작 상태 이름").Require()
        .Build();
}

// ── 이펙트 ─────────────────────────────────────────────────────────────────
SchemaPtr RangeFloat() {
    // [min, max] 또는 단일 값. 파티클 파라미터는 대부분 범위다.
    return B::Union({B::Number(), B::NumberArray(2)})
        .Describe("단일 값 또는 [최소, 최대] 범위")
        .Build();
}

SchemaPtr Emitter() {
    return B::Map()
        .Title("Emitter")
        .Describe("파티클을 뿜는 단위")
        .Field("name", Name(), "에미터 이름").Require()
        .Field("rate", B::Float(), "초당 방출 개수").Default(Value{100.0})
        .Field("burst", B::Int(), "시작 시 한 번에 뿜을 개수")
            .Default(Value{static_cast<i64>(0)})
        .Field("maxParticles", B::Int(), "동시 최대 개수. 성능 상한이다")
            .Default(Value{static_cast<i64>(1000)})
        .Field("lifetime", RangeFloat(), "파티클 수명(초)")
        .Field("shape", B::Enum({"point", "sphere", "cone", "box", "circle"}), "방출 형상")
            .Default(Value{"point"})
        .Field("radius", B::Float(), "sphere/circle/cone 반지름").Default(Value{1.0})
        .Field("angle", B::Float(), "cone 각도(도)").Default(Value{25.0})
        .Field("speed", RangeFloat(), "초기 속도(m/s)")
        .Field("size", RangeFloat(), "크기(미터)")
        .Field("rotationSpeed", RangeFloat(), "회전 속도(도/초)")
        .Field("gravity", B::Float(), "중력 배율").Default(Value{0.0})
        .Field("drag", B::Float(), "공기 저항").Default(Value{0.0})
        .Field("colorOverLife", B::Seq(B::Color()).Build(),
               "수명에 따라 보간할 색 목록. 앞이 태어날 때, 뒤가 사라질 때")
        .Field("sizeOverLife", B::Seq(B::Number()).Build(), "수명에 따른 크기 배율")
        .Field("material", AssetRef(), "파티클 머티리얼")
        .Field("renderMode", B::Enum({"billboard", "stretched", "mesh", "trail"}), "그리는 방식")
            .Default(Value{"billboard"})
        .Field("mesh", AssetRef(), "renderMode 가 mesh 일 때의 메시")
        .Build();
}

SchemaPtr Effect() {
    return B::Map("alice/effect/1")
        .Title("Effect")
        .Describe("파티클 이펙트. 에미터를 겹쳐 하나의 연출을 만든다")
        .Field("schema", SchemaField(), "항상 alice/effect/1").Require()
        .Field("name", Name(), "이펙트 이름").Require()
        .Field("duration", B::Float(), "전체 길이(초). 0 이면 무한")
            .Default(Value{1.0})
        .Field("loop", B::Bool(), "끝나면 다시 시작").Default(Value{false})
        .Field("emitters", B::Seq(Emitter()).MinItems(1).Build(), "에미터들").Require()
        .Field("budget", B::Map()
                   .Title("Effect Budget")
                   .Field("maxParticles", B::Int(), "이 이펙트 전체의 파티클 상한")
                   .Field("maxDrawCalls", B::Int(), "허용 드로우콜 수")
                   .Build(),
               "성능 상한. 넘으면 perf.budget.exceeded 로그가 나간다")
        .Build();
}

// ── 사운드 ─────────────────────────────────────────────────────────────────
SchemaPtr SoundVariation() {
    return B::Map()
        .Field("asset", AssetRef(), "오디오 파일 경로").Require()
        .Field("weight", B::Float(), "선택 확률 가중치").Default(Value{1.0})
        .Build();
}

SchemaPtr Sound() {
    return B::Map("alice/sound/1")
        .Title("Sound")
        .Describe("소리 하나의 정의. 변주와 랜덤화를 값으로 기술해 반복감을 없앤다")
        .Field("schema", SchemaField(), "항상 alice/sound/1").Require()
        .Field("name", Name(), "사운드 이름").Require()
        .Field("bus", B::Enum({"master", "music", "sfx", "voice", "ambient", "ui"}),
               "믹서 버스").Default(Value{"sfx"})
        .Field("variations", B::Seq(SoundVariation()).MinItems(1).Build(),
               "같은 소리의 변주들").Require()
        .Field("pickMode", B::Enum({"random", "randomNoRepeat", "sequential", "shuffle"}),
               "여러 변주 중 고르는 방식").Default(Value{"randomNoRepeat"})
        .Field("volume", RangeFloat(), "음량. 범위를 주면 매번 그 안에서 뽑는다")
        .Field("pitch", RangeFloat(), "피치 배율. 범위를 주면 매번 그 안에서 뽑는다")
        .Field("loop", B::Bool(), "반복 재생").Default(Value{false})
        .Field("maxInstances", B::Int(), "동시에 울릴 수 있는 최대 개수")
            .Default(Value{static_cast<i64>(8)})
        .Field("cooldown", B::Float(), "같은 소리의 최소 재생 간격(초)")
            .Default(Value{0.0})
        .Build();
}

// ── 물리 머티리얼 ──────────────────────────────────────────────────────────
SchemaPtr PhysicsMaterial() {
    return B::Map("alice/physics/1")
        .Title("Physics Material")
        .Describe("표면의 마찰과 반발. 얼음·고무·금속의 차이가 여기서 온다")
        .Field("schema", SchemaField(), "항상 alice/physics/1").Require()
        .Field("name", Name(), "물리 머티리얼 이름").Require()
        .Field("staticFriction", B::Float(), "정지 마찰 계수").Default(Value{0.6}).Min(0.0)
        .Field("dynamicFriction", B::Float(), "운동 마찰 계수").Default(Value{0.6}).Min(0.0)
        .Field("restitution", B::Float(), "반발 계수. 1 이면 에너지를 잃지 않는다")
            .Default(Value{0.0}).Range(0.0, 1.0)
        .Field("density", B::Float(), "밀도(kg/m³)").Default(Value{1000.0}).Min(0.0)
        .Field("frictionCombine", B::Enum({"average", "min", "max", "multiply"}),
               "두 표면이 만났을 때 마찰을 합치는 방식").Default(Value{"average"})
        .Field("restitutionCombine", B::Enum({"average", "min", "max", "multiply"}),
               "반발을 합치는 방식").Default(Value{"average"})
        .Build();
}

// ── 입력 ───────────────────────────────────────────────────────────────────
SchemaPtr InputAction() {
    return B::Map()
        .Title("Input Action")
        .Field("name", Name(), "행동에서 부를 이름. 예) Jump").Require()
        .Field("type", B::Enum({"button", "axis", "axis2"}), "값의 모양").Require()
        .Field("bindings", B::Seq(B::String()).MinItems(1).Build(),
               "물리 입력. 예) [key.space, gamepad.a, touch.tap]").Require()
        .Field("deadzone", B::Float(), "축 입력의 데드존").Default(Value{0.15})
        .Build();
}

SchemaPtr InputMap() {
    return B::Map("alice/input/1")
        .Title("Input Map")
        .Describe(
            "물리 입력을 이름 있는 행동으로 바꾼다. 행동 문서는 키가 아니라 이름을 참조하므로 "
            "PC·패드·터치를 같은 콘텐츠로 지원할 수 있다")
        .Field("schema", SchemaField(), "항상 alice/input/1").Require()
        .Field("name", Name(), "입력 맵 이름").Require()
        .Field("actions", B::Seq(InputAction()).MinItems(1).Build(), "행동 목록").Require()
        .Build();
}

// ── 프로젝트 ───────────────────────────────────────────────────────────────
SchemaPtr Project() {
    return B::Map("alice/project/1")
        .Title("Project")
        .Describe("프로젝트 전체 설정. 엔진이 가장 먼저 읽는 문서다")
        .Field("schema", SchemaField(), "항상 alice/project/1").Require()
        .Field("name", Name(), "프로젝트 이름").Require()
        .Field("version", B::String(), "프로젝트 버전 문자열").Default(Value{"0.1.0"})
        .Field("startScene", AssetRef(), "실행 시 처음 여는 씬").Require()
        .Field("contentRoot", B::String(), "콘텐츠 문서들의 루트 디렉터리")
            .Default(Value{"Content"})
        .Field("renderer", B::Map()
                   .Title("Renderer Settings")
                   .Field("backend",
                          B::Enum({"auto", "d3d11", "d3d12", "vulkan", "metal", "null"}),
                          "그래픽 백엔드. auto 면 플랫폼에 맞게 고른다")
                       .Default(Value{"auto"})
                   .Field("vsync", B::Bool(), "수직동기").Default(Value{true})
                   .Field("targetFrameRate", B::Int(), "목표 프레임레이트")
                       .Default(Value{static_cast<i64>(60)})
                   .Field("resolution", B::Vec2(), "기본 창 해상도 [width, height]")
                   .Field("msaa", B::Enum({"off", "2x", "4x", "8x"}), "멀티샘플링")
                       .Default(Value{"off"})
                   .Build(),
               "렌더링 설정")
        .Field("budgets", B::Map().AllowExtraFields(B::Number()).Build(),
               "프로파일러 존별 프레임 예산(밀리초). 넘으면 구조화 로그가 나간다. "
               "예) { Frame: 16.6, Render: 8.0, Physics: 2.0 }")
        .Field("logging", B::Map()
                   .Title("Logging Settings")
                   .Field("level",
                          B::Enum({"trace", "debug", "info", "warn", "error", "fatal", "off"}),
                          "전역 로그 레벨").Default(Value{"info"})
                   .Field("channels", B::Map().AllowExtraFields(B::String()).Build(),
                          "채널별 레벨 오버라이드. 예) { rhi: warn }")
                   .Field("jsonlPath", B::String(), "구조화 로그(NDJSON) 출력 경로")
                       .Default(Value{"Logs/alice.jsonl"})
                   .Build(),
               "로깅 설정")
        .Field("platforms", B::Seq(B::Enum({"windows", "macos", "linux",
                                            "ios", "android", "console"})).Build(),
               "빌드 대상 플랫폼")
        .Build();
}

} // namespace

void RegisterCoreSchemas(Registry& registry) {
    // 컴포넌트 — 액터가 참조하므로 먼저 등록한다.
    registry.Register(Transform());
    registry.Register(MeshComponent());
    registry.Register(CameraComponent());
    registry.Register(LightComponent());
    registry.Register(RigidbodyComponent());
    registry.Register(Character2DComponent());
    registry.Register(ColliderComponent());
    registry.Register(AudioSourceComponent());
    registry.Register(ParticlesComponent());
    registry.Register(AnimatorComponent());

    // 최상위 문서 타입 — 파일 하나가 되는 것들.
    registry.Register(Project());
    registry.Register(Scene());
    registry.Register(Actor());
    registry.Register(Behavior());
    registry.Register(Material());
    registry.Register(Animation());
    registry.Register(Effect());
    registry.Register(Sound());
    registry.Register(PhysicsMaterial());
    registry.Register(InputMap());
}

} // namespace alice::schema
