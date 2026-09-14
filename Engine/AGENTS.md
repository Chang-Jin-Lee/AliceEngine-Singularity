# Engine/ 에서 일할 때

> 루트의 [`AGENTS.md`](../AGENTS.md) 를 먼저 읽으십시오. 여기는 C++ 코드에만 해당하는 것입니다.

## 의존성 방향

```
Foundation → Doc → Schema → Verbs
Foundation → RHI
```

**아래로만 흐릅니다.** `Engine/CMakeLists.txt` 가 그래프 자체이고, `alice_module()` 이
include 경로를 소스 루트 하나로 고정해 상대 경로 접근을 막습니다.

위로 흐르는 참조는 컴파일이 아니라 **CMake 설정 단계에서** 깨집니다.

## 새 파일을 추가하려면

`Engine/CMakeLists.txt` 의 해당 모듈 `SOURCES` 에 추가하십시오. GLOB 을 쓰지 않습니다 —
파일이 조용히 빌드에서 빠지거나 들어오는 것을 막기 위해서입니다.

## 진단을 만들 때

```cpp
Diagnostic& d = diagnostics.Error("영역.대상.결과", "무엇이 잘못됐는지");
d.mark = value.mark;
d.path = "actors[0].transform.position";
d.hint = "어떻게 고치는지";
```

- **코드는 안정 식별자입니다.** 한 번 릴리스되면 바꾸지 않습니다
- **힌트 없는 진단은 미완성입니다**
- 오타 후보가 있으면 `ClosestMatch()` 로 제안을 붙이십시오
- 첫 에러에서 멈추지 마십시오. `DiagnosticBag` 에 모아 한 번에 보고합니다

## 로그를 남길 때

```cpp
ALICE_LOG_WARN("perf", "perf.budget.exceeded")
    .Msg("존이 프레임 예산을 넘었다")
    .F("zone", name)
    .F("actualMs", ms)      // 숫자는 숫자로
    .F("budgetMs", budget);
```

이벤트 id 규칙: `<채널>.<대상>.<결과>` 소문자 점 표기.

레벨이 꺼져 있으면 인자가 **평가되지 않습니다.** 비싼 계산을 인자에 넣어도 됩니다.

## 성능이 중요한 곳

```cpp
ALICE_PROFILE_ZONE("Render.Shadow");   // 이름은 반드시 문자열 리터럴
```

새 서브시스템을 만들면 `project.yaml` 의 `budgets` 에 넣을 이름을 정하고 문서에 적으십시오.

## 이름 규칙

| | |
|---|---|
| 네임스페이스 | `alice`, `alice::doc`, `alice::schema`, `alice::verbs`, `alice::rhi` |
| 타입 · 함수 | `PascalCase` |
| 멤버 변수 | `m_camelCase` |
| 지역 변수 · 파라미터 | `camelCase` |
| 상수 | `kPascalCase` |
| 매크로 | `ALICE_UPPER_SNAKE` |

## 주의

- 예외를 쓰지 않습니다. `Result<T>` / `Status` 로 돌려주십시오
- 문자열 포매터는 `Fmt(...)` 입니다. `Format` 은 열거형 이름과 충돌해 개명했습니다
- `std::format` 을 쓰지 마십시오. Apple Clang 과 구형 NDK 에서 편차가 큽니다
- 경고 0으로 빌드되어야 합니다. `-DALICE_WARNINGS_AS_ERRORS=ON` 으로 확인하십시오

## 테스트

```bash
./build/bin/Alice.Tests --filter=Yaml     # 일부만
./build/bin/Alice.Tests --json            # 기계용
./build/bin/Alice.Tests --list            # 목록
```

새 기능에는 테스트를 붙이십시오. **붙이기 전에 일부러 코드를 망가뜨려
그 테스트가 빨간지 확인하십시오.**
