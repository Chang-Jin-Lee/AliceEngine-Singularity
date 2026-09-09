# AGENTS.md

> 이 저장소에서 일하는 모든 AI 에이전트가 읽는 파일입니다.
> Codex 는 이 파일을, Claude Code 는 `CLAUDE.md`(이 파일을 가리킴)를 자동으로 읽습니다.

---

## 이 저장소가 무엇인가

**AI 가 다룰 수 있게 처음부터 설계한 게임 엔진입니다.**

전제는 하나입니다.

> 콘텐츠는 코드가 아니라 **스키마로 검증되는 텍스트 문서**다.
> 게임플레이는 스크립트가 아니라 **선언적 규칙**이다.

언리얼의 `C++ → UHT → 블루프린트`, 유니티의 `C# 스크립팅`이 있던 자리를 YAML 문서와
**유한한 동사 목록**이 대신합니다. 그래서 당신은 다음을 할 수 있습니다.

```bash
alice verbs --json          # 엔진이 할 수 있는 일 전부 — 추측할 필요 없음
alice schema list --json    # 만들 수 있는 문서 타입 전부
alice check Content/ --json # 내가 만든 게 맞는지 즉시 확인
```

**추측하지 마십시오. 물어보십시오.** 위 명령들이 답을 줍니다.

---

## 30초 시작

```bash
# 1. 빌드 (CMake 3.24+, C++20 컴파일러만 있으면 됩니다. 서드파티 0)
./Scripts/build.sh            # Windows: .\Scripts\build.ps1

# 2. 엔진이 뭘 아는지 확인
./build/bin/alice doctor

# 3. 전체 검증 — 커밋 전에 항상 이것
./Scripts/verify.sh           # Windows: .\Scripts\verify.ps1
```

`verify` 가 통과하면 커밋해도 됩니다. 실패하면 그 이유가 화면에 나옵니다.

---

## 당신은 어느 역할인가

이 저장소는 **서로 다른 컴퓨터에서, 서로를 모르는 AI 세션들이** 기여하는 것을 전제로 합니다.
충돌을 막는 장치는 하나뿐입니다 — **각자 자기 영역만 건드리기.**

| 역할 | 한 문장 | 문서 | 백로그 |
|---|---|---|---|
| **Alice** | 무엇과 무엇이 만나도 되는지 정한다 | [Agents/Alice.md](Agents/Alice.md) | [alice/](Agents/Backlog/alice) |
| **Sidney** | 세계를 메모리에 올리고 굴린다 | [Agents/Sidney.md](Agents/Sidney.md) | [sidney/](Agents/Backlog/sidney) |
| **Monday** | 화면에 그린다 | [Agents/Monday.md](Agents/Monday.md) | [monday/](Agents/Backlog/monday) |
| **Chrono** | 사람과 AI 가 엔진을 쓸 수 있게 한다 | [Agents/Chrono.md](Agents/Chrono.md) | [chrono/](Agents/Backlog/chrono) |
| **Seeho** | 무너지지 않았는지 확인한다 | [Agents/Seeho.md](Agents/Seeho.md) | [seeho/](Agents/Backlog/seeho) |

**역할을 지정받지 않았다면 사용자에게 물어보십시오.** 역할 없이 코드를 고치지 마십시오.

역할을 받았다면 순서는 이렇습니다.

1. `Agents/<역할>.md` — 내 경계는 어디까지인가
2. `Agents/Backlog/<역할>/` — `상태: 대기` 인 작업 중 하나를 고른다
3. **고른 작업의 `상태:` 를 `진행중` 으로 바꾸고 그것만 먼저 커밋한다**
   — 다른 컴퓨터의 세션이 같은 작업을 잡는 것을 막는 유일한 장치입니다
4. 그 작업 문서의 **완료 기준**을 전부 만족시킨다

현재 백로그: `python Scripts/backlog.py`

---

## 절대 규칙 다섯

### 1. 진단에는 셋이 다 있어야 한다

```cpp
Diagnostic& d = bag.Error("영역.대상.결과", "무엇이 잘못됐는지");
d.mark = value.mark;                    // 어디인지
d.hint = "어떻게 고치는지";               // ← 이게 없으면 미완성이다
```

힌트 없는 진단은 AI 가 스스로 못 고칩니다. 이 엔진의 존재 이유를 부정하는 것입니다.

**진단 코드(`schema.unknown_field`)는 안정 식별자입니다. 문구는 바꿔도 코드는 절대 바꾸지 마십시오.**

### 2. 로그는 문자열이 아니라 레코드다

```cpp
ALICE_LOG_ERROR("asset", "asset.load.failed")
    .Msg("메시를 열지 못했다")
    .F("path", path)          // 문자열
    .F("bytes", size);        // 숫자는 숫자로. 그래야 집계된다
```

`printf` 스타일로 문장만 뱉지 마십시오. 기계가 못 읽습니다.

### 3. 의존성은 아래로만 흐른다

```
Foundation → Doc → Schema → Verbs
Foundation → RHI
```

CMake 가 강제합니다. 위로 흐르는 참조는 **설정 단계에서** 깨집니다.
남의 모듈 공개 헤더를 바꿔야 하면 [인터페이스 변경 이슈](.github/ISSUE_TEMPLATE/interface_change.md)를 여십시오.

### 4. 서드파티를 추가하지 않는다

이 저장소는 의존성이 **0개**입니다. 클론하면 바로 빌드됩니다.
정말 필요하면 PR 에서 먼저 합의합니다. 판단 기준은 [Agents/Alice.md](Agents/Alice.md)에 있습니다.

이유: 참고한 오픈소스 엔진 10개 중 **6개가 첫 빌드에서 막혔고, 전부 서드파티 문제**였습니다.
→ [ADR-0002](Docs/adr/0002-no-third-party.md)

### 5. 테스트는 무언가 깨지면 실패해야 한다

통과하는 테스트를 붙이는 것은 쉽습니다. **붙이기 전에 일부러 코드를 망가뜨려
그 테스트가 빨간지 확인하십시오.**

특히 **진단 품질 테스트**를 붙이십시오. 힌트가 사라지거나 위치가 틀어지는 회귀는
일반 테스트로는 안 잡힙니다.

---

## 애셋을 저장소에 올릴 때

사용자 요청에 따라, 외부 이미지·모델·텍스처·음원·폰트는 출처와 권리 조건을 확인한
것만 추가합니다. 직접 제작한 애셋 또는 원저작자가 CC0·퍼블릭 도메인으로 제공한
애셋을 기본으로 사용합니다. 단순히 무료로 내려받을 수 있다는 이유로 포함하지 마십시오.

- 추가할 때 파일 경로, 제작자, 원본 URL, 라이선스 원문과 확인 날짜를 함께 기록합니다.
- 직접 제작한 파일은 제작 경위와 외부 소재 사용 여부를 기록합니다.
- 출처·재배포 권한이 불명확하거나 별도 사용 제한이 있는 애셋은 커밋·푸시에서 제외합니다.
- 참조 엔진의 애셋 폴더를 통째로 복사하지 않습니다. 문서에 적힌 예시 경로는
  해당 파일의 배포 권한을 증명하지 않습니다.

현재 FirstLight에 포함된 것은 YAML 샘플 문서입니다. 문서가 참조하는 메시·음원 파일은
포함되어 있지 않습니다. 이후 애셋을 추가하면 개별 파일의 출처 기록을 먼저 남깁니다.

## 코드 관례

```cpp
// 파일 머리에는 "이 파일이 왜 존재하는지"를 적는다. 무엇을 하는지는 코드가 말한다.

namespace alice::doc {          // 모듈마다 네임스페이스

class Value {
public:
    bool IsNull() const noexcept;   // PascalCase 멤버 함수
private:
    Kind m_kind;                    // m_ 접두 멤버
};

}
```

- **주석은 '무엇'이 아니라 '왜'를 적는다.** 무엇은 코드가 말합니다
- 예외를 쓰지 않습니다. 실패는 `Result<T>` / `Status` 로 돌려줍니다
- `Format` 이라는 이름의 문자열 포매터는 `Fmt` 입니다 (`Format` 은 열거형 이름과 충돌했습니다)
- include 는 항상 모듈 이름부터: `#include "Foundation/Log.h"`.
  상대 경로(`../../`)는 CMake 가 막습니다
- 경고 0으로 빌드되어야 합니다

---

## 커밋과 PR

```
<타입>(<영역>): <한 줄 요약>

Agent: <역할> (<담당>)
Task: <백로그 ID>

<왜 이렇게 했는지>
```

타입: `feat` `fix` `perf` `refactor` `docs` `test` `build` `chore`

브랜치: `<역할소문자>/<작업ID>-<짧은-설명>` (예: `monday/MON-02-d3d11-device`)

**한 PR = 한 작업.** 지나가다 발견한 다른 문제는 새 백로그 항목으로 만들고 넘어가십시오.
"김에 같이 고쳤다"가 리뷰를 가장 어렵게 만듭니다.

### PR 이 반드시 묻는 넷 — 빈칸으로 두지 마십시오

1. **왜 이렇게 만들었는가** — 무엇을 했는지는 diff 가 말합니다. 왜는 당신만 압니다
2. **다르게 만들 방법은 없었는가** — 버린 대안과 **버린 이유**까지
3. **장점과 단점** — **단점 칸을 비우지 마십시오.** 대가 없는 설계는 없습니다
4. **이전과 무엇이 달라졌는가** — 성능이면 숫자와 **측정 조건**까지

CI 가 이 넷과 `Agent:` 표기를 검사합니다.

### 선택이지만 권장 — 대화 요약

작업을 사람과 함께 했다면 마지막에 이렇게 부탁받을 수 있습니다.

> "지금까지의 대화를 PR 에 붙일 수 있게 요약해줘. 사용자가 무엇을 요구했고,
> 어떤 대안을 제시했고, 왜 이 방향으로 정했는지, 중간에 버린 접근도 포함해서."

코드는 결론만 남기고 과정을 지웁니다. 다음에 이 코드를 고칠 AI 가 가장 알고 싶어 하는 것은
**이미 시도했다가 안 됐던 것**입니다.

---

## 커밋 전 반드시

```bash
./Scripts/verify.sh          # Windows: .\Scripts\verify.ps1
```

이 하나가 다음을 전부 돌립니다.

| 검사 | 명령 |
|---|---|
| 경고 0 빌드 | `cmake --build build` |
| 테스트 137개 | `alice.Tests` |
| 샘플 콘텐츠 0오류 | `alice check Samples --json` |
| 문서 정규화 상태 | `alice fmt Samples --check` |
| 생성된 JSON Schema 최신 | `alice schema emit Schemas` + `git diff` |
| 백로그 표 최신 | `python Scripts/backlog.py --check` |

---

## 자주 하는 실수

| 하지 마십시오 | 대신 |
|---|---|
| 동사 이름을 지어낸다 | `alice verbs --json` 으로 확인 |
| 스키마 필드를 추측한다 | `alice schema show <id>` |
| 진단 코드(id)를 바꾼다 | 문구만 바꾼다. id 는 안정 식별자다 |
| 남의 모듈 공개 헤더를 조용히 고친다 | 인터페이스 변경 이슈를 연다 |
| 백로그 상태를 안 바꾸고 시작한다 | 먼저 `진행중` 으로 커밋 |
| 통과하는 테스트를 붙인다 | 일부러 망가뜨려 빨간지 확인하고 붙인다 |
| PR 의 "대안" 칸을 비운다 | 다음 사람이 같은 고민을 반복한다 |
| 생성된 파일을 손으로 고친다 | `Schemas/`, `Wiki/content/reference/`, `Wiki/public/` 는 생성물이다 |

---

## 저장소 지형

```
Engine/
  Foundation/   로그 · 프로파일러 · 진단 · 파일시스템 · 시간   (의존 없음)
  Doc/          문서 값 모델 · YAML/JSON 파서 · 직렬화
  Schema/       콘텐츠 모델 · 검증 · JSON Schema 생성
  Verbs/        동사 · 조건식 · 2차 검사
  RHI/          그래픽 추상화 + Null 백엔드
  Tests/        137개
  Runtime/      공개 계약 헤더 6개 (구현은 SID-02~06)
Tools/AliceCLI/ alice 명령
Samples/        FirstLight — 코드 없는 완전한 프로젝트
Agents/         역할 정의와 백로그 26건
Docs/           설계 문서 · ADR
Schemas/        생성물 — 에디터 자동완성용 JSON Schema
Wiki/           문서 사이트 (Next.js)
Scripts/        build · verify · backlog
```

**이 엔진을 이해하려면 두 파일이면 됩니다.**

- `Engine/Schema/CoreSchemas.cpp` — **만들 수 있는 것의 전부**
- `Engine/Verbs/CoreVerbs.cpp` — **할 수 있는 것의 전부**

---

## 더 읽을 것

| 무엇을 하려는가 | 어디를 |
|---|---|
| 콘텐츠 문서를 만든다 | [Docs/CONTENT_FORMAT.md](Docs/CONTENT_FORMAT.md) |
| 왜 이렇게 설계했는지 안다 | [Docs/adr/](Docs/adr) — 결정 3건 |
| 모듈 경계를 안다 | [Docs/ARCHITECTURE.md](Docs/ARCHITECTURE.md) |
| 성능을 다룬다 | [Docs/PERFORMANCE.md](Docs/PERFORMANCE.md) |
| 다른 엔진에서 뭘 가져왔나 | [Docs/ENGINE_SURVEY.md](Docs/ENGINE_SURVEY.md) |
| 세션을 새로 시작한다 | [Docs/ONBOARDING.md](Docs/ONBOARDING.md) |
| 개발 규칙 전문 | [README.md](README.md#개발-규칙) |
