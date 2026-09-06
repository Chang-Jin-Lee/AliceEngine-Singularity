# AI 에이전트를 위한 안내

이 파일은 이 저장소에서 일하는 AI 가 **가장 먼저** 읽는 문서다.

## 60초 요약

이 엔진의 전제는 하나다.

> **콘텐츠는 코드가 아니라 텍스트 문서다.**

언리얼의 C++/UHT/블루프린트와 유니티의 C# 스크립팅이 있던 자리를,
스키마로 검증되는 YAML/JSON 문서와 **유한한 동사 목록**이 대신한다.

그래서 너는 이 엔진에서 다음을 할 수 있다. 다른 엔진에서는 못 하던 것들이다.

```bash
alice verbs --json              # 엔진이 할 수 있는 일 전부 (추측할 필요 없음)
alice schema list --json        # 만들 수 있는 문서 타입 전부
alice schema show alice/actor/1 # 그 문서에 뭘 적을 수 있는지
alice check Content/ --json     # 내가 만든 게 맞는지 즉시 확인
alice explain <진단코드>         # 왜 그 규칙이 있는지
```

**추측하지 마라. 물어봐라.** 위 명령들이 답을 준다.

## 작업을 시작하기 전에

1. `Agents/README.md` — 어떻게 기여하는가
2. `Agents/<네 역할>.md` — 네 경계는 어디까지인가
3. `Agents/Backlog/<네 역할>/` — 무엇을 할 것인가

역할을 지정받지 않았다면 물어봐라. 역할 없이 코드를 고치지 마라.

## 반드시 지킬 것

**진단에는 셋이 다 있어야 한다**

```cpp
Diagnostic& d = bag.Error("영역.대상.결과", "무엇이 잘못됐는지");
d.mark = value.mark;                    // 어디인지
d.hint = "어떻게 고치는지";               // 이게 없으면 미완성이다
```

힌트 없는 진단은 AI 가 스스로 고칠 수 없다. 그건 이 엔진의 존재 이유를 부정하는 것이다.

**로그는 문자열이 아니라 레코드다**

```cpp
ALICE_LOG_ERROR("asset", "asset.load.failed")
    .Msg("메시를 열지 못했다")
    .F("path", path)          // 문자열
    .F("bytes", size);        // 숫자는 숫자로. 그래야 집계된다
```

이벤트 id(`asset.load.failed`)는 **안정 식별자**다. 한 번 릴리스되면 바꾸지 않는다.

**의존성은 아래로만 흐른다**

```
Foundation → Doc → Schema → Verbs
Foundation → RHI
```

CMake 가 강제한다. 위로 흐르는 참조는 설정 단계에서 깨진다.

**서드파티를 추가하지 마라**

이 저장소는 의존성이 0개다. 클론하면 바로 빌드된다.
정말 필요하면 PR 에서 먼저 합의한다. 판단 기준은 `Agents/Alice.md` 에 있다.

## 커밋과 PR

```
<타입>(<영역>): <한 줄 요약>

Agent: <역할> (<담당>)
Task: <백로그 ID>

<왜 이렇게 했는지>
```

PR 템플릿이 네 가지를 묻는다. **빈칸으로 두지 마라.**

1. 왜 이렇게 만들었는가
2. 다르게 만들 방법은 없었는가 (버린 대안과 버린 이유)
3. 장점과 단점 (**단점 칸을 비우지 마라**)
4. 이전과 무엇이 달라졌는가

그리고 선택이지만 권장: **사람과 나눈 대화의 요약을 붙여라.**
코드는 결론만 남기고 과정을 지운다. 다음에 이 코드를 고칠 AI 가 가장 알고 싶어 하는 것은
**이미 시도했다가 안 됐던 것**이다.

## 병합 전 확인

```bash
cmake --build build                     # 경고 0
./build/bin/Alice.Tests                 # 전부 통과
./build/bin/alice check Samples --json  # 오류 0
./build/bin/alice fmt Samples --check   # 정규화 상태
```

## 자주 하는 실수

| 하지 마라 | 대신 |
|---|---|
| 동사 이름을 지어낸다 | `alice verbs --json` 으로 확인 |
| 스키마 필드를 추측한다 | `alice schema show <id>` |
| 진단 코드(id)를 바꾼다 | 문구만 바꾼다. id 는 안정 식별자다 |
| 남의 모듈 공개 헤더를 조용히 고친다 | Alice 앞으로 이슈를 연다 |
| 백로그 상태를 안 바꾸고 시작한다 | 먼저 `진행중` 으로 커밋 |
| 통과하는 테스트를 붙인다 | 일부러 망가뜨려 빨간지 확인하고 붙인다 |

## 이 저장소의 지형

```
Engine/
  Foundation/   로그 · 프로파일러 · 진단 · 파일시스템   (의존 없음)
  Doc/          문서 값 모델 · YAML/JSON 파서 · 직렬화
  Schema/       콘텐츠 모델 · 검증 · JSON Schema 생성   ← 콘텐츠 타입은 여기
  Verbs/        동사 · 조건식 · 2차 검사              ← 엔진 API 표면은 여기
  RHI/          그래픽 추상화 + Null 백엔드
  Tests/        131개
Tools/AliceCLI/ alice 명령
Samples/        FirstLight — 코드 없는 완전한 프로젝트
Agents/         역할과 백로그
Docs/           설계 문서
Wiki/           문서 사이트
```

**콘텐츠 모델을 알고 싶으면** `Engine/Schema/CoreSchemas.cpp` 하나를 읽어라.
그 파일이 이 엔진으로 만들 수 있는 것의 전부다.

**엔진이 할 수 있는 일을 알고 싶으면** `Engine/Verbs/CoreVerbs.cpp`.
