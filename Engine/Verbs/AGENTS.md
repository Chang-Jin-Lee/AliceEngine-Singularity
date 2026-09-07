# Verbs/ 에서 일할 때

> **이 디렉터리가 엔진의 API 표면 전부입니다.** 콘텐츠가 호출할 수 있는 것은
> 여기 등록된 동사뿐입니다. 그 유한함이 이 엔진의 AI 친화성의 실질입니다.

## 새 동사를 추가하려면

`CoreVerbs.cpp` 에서:

```cpp
r.Register(Make("영역.동작", "한 줄 설명",
    B::Map()
        .Field("target", TargetField())
        .Field("force", B::Float(), "충격 크기(N·s)").Require()
        .Build(),
    {"물리"},                       // 태그 — 분류와 필터에 쓴다
    Verb::Cost::Cheap,              // 비용 등급
    /*deterministic=*/true));       // 같은 입력에 같은 결과인가
```

등록하면 `alice verbs`, 위키, 2차 검사, 자동완성이 전부 따라옵니다.

## 반드시 정확히 적을 것

**`deterministic`** — 난수나 변주가 들어가면 `false` 입니다.
리플레이와 결정적 테스트가 이 플래그를 믿고 동작합니다. 틀리면 재현 불가 버그가 됩니다.

**`Cost`** — `Trivial` / `Cheap` / `Moderate` / `Expensive`.
프레임 예산 경고의 근거가 됩니다.

**`summary`** — AI 가 이것만 보고 고릅니다. 무엇을 하는지 한 문장으로.

## 동사를 추가하기 전에 물어볼 것

> 기존 동사 조합으로 안 되는가?

동사가 150개를 넘으면 "목록이 유한해서 AI 가 파악할 수 있다"는 이점이 약해집니다.
→ [ADR-0001](../../Docs/adr/0001-declarative-content.md)

## 식 심볼을 추가하려면

`RegisterCoreSymbols()` 에서:

```cpp
t.AddProperty("physics.grounded", "bool", "이 액터가 바닥에 닿아 있는지");
t.AddFunction("input.pressed", 1, 1, "bool", "이번 프레임에 눌리기 시작한 행동인지");
```

**함수와 값을 정확히 구분하십시오.** 검사기가 `input.pressed` 를 괄호 없이 쓴 것과
`physics.grounded()` 를 괄호와 함께 쓴 것을 각각 다른 에러로 잡습니다.

## 조건식은 부작용이 없어야 한다

`when:` 은 읽기만 합니다. 이 불변식이 있어야 규칙 평가 순서가 결과에 영향을 주지 않습니다.
값을 바꾸는 것은 전부 `do:` 의 동사입니다.

**이 규칙을 완화하는 변경은 하지 마십시오.** 순서 의존 버그는 6개월 뒤에
재현 불가로 돌아옵니다.
