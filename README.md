<div align="center">

# AliceEngine-Singularity

**Multiple Engines. One AI-Native Engine.**

여러 자체엔진을 하나로 통합하고, 처음부터 AI가 다룰 수 있게 설계한 게임 엔진

[시작하기](#빌드) · [위키 실행](#위키-만들고-보기) · [현재 상태와 다음 작업](Docs/STATUS.md)

[![tests](https://img.shields.io/badge/tests-173%20passing-brightgreen)](Engine/Tests)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)](CMakeLists.txt)
[![license](https://img.shields.io/badge/license-MIT-lightgrey)](LICENSE)

</div>

---

## 이 엔진이 다른 이유

AI가 콘텐츠를 만들려면 사용할 수 있는 기능을 조회하고, 변경 결과를 검증하고,
오류 위치와 수정 방법을 읽을 수 있어야 합니다. 이 프로젝트는 그 과정을 엔진의 기본 작업 흐름으로 만듭니다.

```
콘텐츠 = 텍스트 문서.   게임플레이 = 선언적 규칙.   확인 = 명령 한 줄.
```

```bash
alice check Content/          # 전부 검증. 틀린 곳과 고치는 법이 나온다
alice check Content/ --json   # 같은 내용을 기계가 읽는 형태로
alice verbs --json            # 엔진이 할 수 있는 일 전부
alice schema show alice/actor/1
```

읽고 → 고치고 → 되쓰고 → 다시 확인하는 고리가 **닫혀 있습니다.** 이것이 전부입니다.

---

## 30초 맛보기

이 파일 하나가 언리얼의 C++ 클래스, 유니티의 `MonoBehaviour` 자리를 대신합니다.

```yaml
schema: alice/behavior/1
name: PlayerMovement

variables:
  jumpCount: 0
  maxJumps: 2

rules:
  - when: physics.grounded and jumpCount > 0
    do:
      - var.set: { name: jumpCount, value: 0 }

  - when: input.pressed("Jump") and jumpCount < maxJumps
    do:
      - physics.impulse: { direction: [0, 1, 0], force: 6.5 }
      - var.add:    { name: jumpCount, amount: 1 }
      - anim.trigger: { parameter: jump }
      - audio.play: { sound: sounds/jump.sound.yaml }
      - log.write:
          event: gameplay.player.jumped
          fields: { jumpCount: 1 }
```

`when:` 은 **상태를 읽기만** 하고, `do:` 의 동사만 **상태를 바꿉니다.**
현재는 조건식과 동사 인자를 정적으로 검사합니다. 실제 평가·적용 순서와 충돌 처리는
후속 Runtime 구현에서 정하고 테스트할 계약입니다.

실행하기 전에 이미 확인된 것:

- 모든 필드가 존재하고 타입이 맞다
- `physics.impulse` 가 실재하고 인자가 맞다
- `physics.grounded`, `jumpCount` 가 읽을 수 있는 이름이다
- `input.pressed` 가 함수이고 인자가 하나다

오타를 내면 이렇게 나옵니다.

```
player.behavior.yaml:12:9: error[verb.unknown]: 'audio.paly' 는 없는 동사다
    |       - audio.paly: { sound: sounds/jump.sound.yaml }
    |         ^
    = 'audio.play' 을(를) 뜻했는가? `alice verbs` 로 전체 목록을 볼 수 있다
```

같은 진단이 `--json` 에서는 `{"code":"verb.unknown","line":12,"column":9,"hint":"..."}` 로 나옵니다.
**AI는 이 JSON을 읽고 스스로 고칩니다.**

---

## 지금 되는 것 / 안 되는 것

**Windows에서는 독립 에디터 창을 실행할 수 있습니다. `alice` CLI와 ECS도 실행 가능합니다.**
`Samples/CubePlayground`에서 Play/Stop과 큐브 이동·회전·크기 조절을 확인할 수 있습니다.
`Samples/FirstLight`는 검증 가능한 콘텐츠 샘플이며 일반 게임 실행은 후속 작업입니다.
자세한 실행 상태와 개발 순서는 [Docs/STATUS.md](Docs/STATUS.md)에 있습니다.

**된다**

- 스키마 19개(문서 10종 + 컴포넌트 9종) 정의 · 검증 · 정규화 · YAML↔JSON 왕복
- 진단 품질: 줄·열·문서 경로 + 오타 제안 + 수정 예시. 첫 에러에서 멈추지 않고 전부 보고
- 동사 **정의** 32개 + 조건식 파서/검사기 + 식 심볼 36개 (실제 동사 실행은 Runtime에서 구현 예정)
- 구조화 로깅(NDJSON) · 계층 프로파일러 · 프레임 예산 초과 자동 감지
- 백엔드 중립 RHI 인터페이스 + 검증기를 겸하는 Null 백엔드
- Runtime ECS 라이브러리: 엔티티 생성·지연 파괴, 타입별 연속 저장, 다중 컴포넌트 질의, 읽기 가드
- Runtime 공개 헤더 6개와 컴파일 계약 검사 (렌더 추출·규칙 실행은 후속 구현)
- `alice` CLI 10개 명령, 전부 `--json` 지원
- JSON Schema 자동 생성 (에디터 자동완성)
- Windows 씬 에디터: 프로젝트·계층·속성·배치 뷰포트·원문·검증·저장·Undo/Redo
- New scene, 마우스로 크기 조절하는 패널, 오른쪽 실제 PowerShell 터미널, ECS 큐브 Play/Stop
- 테스트 173개(Windows, 다른 플랫폼 171개) + 실제 Windows 창 smoke 검사
- 위키 정적 빌드와 로컬 미리보기, Markdown 원문과 AI용 JSON 레퍼런스

**아직 안 된다 (백로그에 있습니다)**

- 런타임: Scene 로딩, 규칙 평가기, 상태 덤프, 프레임 루프 → [`Agents/Backlog/sidney/`](Agents/Backlog/sidney)
- 렌더러: D3D11/D3D12/Vulkan/Metal 백엔드 → [`Agents/Backlog/monday/`](Agents/Backlog/monday)
- 애셋 파이프라인: 임포터, 캐시 → [`Agents/Backlog/chrono/`](Agents/Backlog/chrono)
- 엔진 내장 AI 대화창 → [`CHR-01`](Agents/Backlog/chrono/CHR-01.md)
- 에디터의 실제 게임 실행·메시 렌더링·도킹, 위키 공개 배포·검색·영어 번역

**SID-02(ECS)는 구현되어 테스트 실행 파일에서 실제로 동작합니다.**
다음은 **SID-03(씬 로더) → SID-04(규칙 평가기)** 순서입니다.
화면 출력에는 실제 그래픽 백엔드와 플랫폼 창·입력 구현도 필요합니다.

---

## 에디터 켜기

Windows PowerShell에서 저장소 루트를 기준으로 실행합니다.

```powershell
pwsh -NoProfile -File .\Scripts\editor.ps1
```

처음에는 자동 빌드하고 CubePlayground 씬을 엽니다. 빌드 후에는 `build\bin\Alice.Editor.exe`를
더블클릭해도 됩니다. 액터 선택 → 속성 수정 → **Apply changes** → **Save** 순서로 편집합니다.
**New scene**으로 씬을 만들고, **Cube demo → Play**로 예시를 실행합니다. Scene을 클릭한 뒤
WASD 이동, Space/Ctrl 높이, Q/E 회전, F/R 크기를 조절합니다. **Stop**은 편집 상태를 복원합니다.
오른쪽은 AI CLI를 직접 실행할 수 있는 PowerShell이며, 패널 경계는 드래그로 조절합니다.
뷰포트는 기본 와이어 큐브이며 실제 메시·물리·일반 규칙 실행은 후속 작업입니다. [자세한 사용법](Docs/EDITOR.md).

## 빌드

```bash
git clone https://github.com/Chang-Jin-Lee/AliceEngine-Singularity.git
cd AliceEngine-Singularity
```

Windows / PowerShell:

```powershell
pwsh -NoProfile -File .\Scripts\build.ps1
.\build\bin\alice.exe doctor --json
.\build\bin\alice.exe check Samples --json
.\build\bin\Alice.Tests.exe
pwsh -NoProfile -File .\Scripts\verify.ps1
```

macOS / Linux:

```bash
./Scripts/build.sh
./build/bin/alice doctor --json
./build/bin/alice check Samples --json
./Scripts/verify.sh
```

필요한 것: **CMake 3.24+, C++20 컴파일러.** 전체 검증의 백로그 검사에는 Python 3도 필요합니다.
Windows 스크립트는 Visual Studio의 C++ 개발자 환경을 자동으로 불러옵니다.
PowerShell 실행 정책 오류의 대처 방법은 [실행 가이드](Docs/STATUS.md#엔진-실행해-보기)에 있습니다.

> C++ 코어의 서드파티 의존성은 **0개**입니다. 위키의 npm 패키지는 별도입니다.
> 참고한 오픈소스 엔진 10개 중 6개가 첫 빌드에서 서드파티 때문에 막혔습니다.
> 그 경험이 이 결정의 근거입니다. → [`Docs/ENGINE_SURVEY.md`](Docs/ENGINE_SURVEY.md)

`doctor`의 `backends`가 `["null"]`이면 화면을 그리는 백엔드가 없는 현재 구성이 맞습니다.
`aiCli`는 PATH에서 CLI를 찾았는지 표시하며, 계정 로그인이나 엔진 내 AI 대화창의 완성을 뜻하지 않습니다.

---

## 위키 만들고 보기

**Node.js 22+와 npm**이 필요합니다. 저장소 루트에서:

```powershell
cd Wiki
npm ci
npm run build
npm start
```

브라우저에서 **[http://localhost:3000](http://localhost:3000)**을 엽니다.
`npm start`는 `Wiki/out/`의 정적 빌드 결과를 제공합니다. 종료는 `Ctrl+C`입니다.

- 편집하면서 확인: `npm run dev`
- 다른 포트: `npm start -- --port 3001`
- 생성·미리보기 회귀 검사: `npm test`
- 현재 기능 안내: `/guide/status/`
- AI용 목록: `/llms.txt` · 전체 문서: `/llms-full.txt`
- 원문 예시: `/guide/status.md` · 기계용 데이터: `/api/verbs.json`

엔진을 빌드했다면 최신 `alice`에서 레퍼런스를 생성합니다. 엔진 실행 파일이 없으면
커밋된 `Schemas/`와 `Wiki/public/api/` 스냅샷에서 재생성합니다.
공개 사이트는 아직 배포하지 않았습니다. Vercel 연결 방법은 [Wiki/README.md](Wiki/README.md)를 보십시오.

---

## 문서

| 문서 | 내용 |
|---|---|
| [`Docs/STATUS.md`](Docs/STATUS.md) | **지금 실행되는 것, 위키 열기, 다음 엔진 개발 순서** |
| [`Docs/EDITOR.md`](Docs/EDITOR.md) | **Windows 에디터 실행·편집·저장과 현재 범위** |
| [`AGENTS.md`](AGENTS.md) | **AI 에이전트 진입점.** Codex·Claude Code 가 자동으로 읽는다 |
| [`Docs/ONBOARDING.md`](Docs/ONBOARDING.md) | **새 세션 시작하기.** 복사해서 붙여넣을 프롬프트 |
| [`Docs/ARCHITECTURE.md`](Docs/ARCHITECTURE.md) | 모듈 경계와 의존성 규칙 |
| [`Docs/CONTENT_FORMAT.md`](Docs/CONTENT_FORMAT.md) | 문서 문법과 규칙. **콘텐츠를 만들려면 여기부터** |
| [`Docs/ENGINE_SURVEY.md`](Docs/ENGINE_SURVEY.md) | 참고 엔진 20개에서 무엇을 가져오고 무엇을 버렸는가 |
| [`Docs/ASSET_PIPELINE.md`](Docs/ASSET_PIPELINE.md) | 애셋 관리 설계 |
| [`Docs/AI_BRIDGE.md`](Docs/AI_BRIDGE.md) | 엔진 내장 AI 대화창 설계 |
| [`Docs/PERFORMANCE.md`](Docs/PERFORMANCE.md) | 로깅·프로파일링·예산 |
| [`Agents/README.md`](Agents/README.md) | **에이전트로 기여하는 법** |

---

# 개발 규칙

> 이 엔진은 사람과 AI가 함께 만듭니다. 아래 규칙은 **둘 모두에게** 적용됩니다.
> 규칙의 목적은 통제가 아니라, **나중에 온 사람이 이 코드가 왜 이런지 알 수 있게** 하는 것입니다.

## 1. 역할(Agent)을 반드시 밝힌다

모든 PR과 커밋은 어느 역할로 한 작업인지 밝힙니다.

```
Agent: Monday (Graphics)
```

역할은 다섯입니다. 자세한 책임 범위는 [`Agents/`](Agents) 를 보십시오.

| 역할 | 담당 | 문서 |
|---|---|---|
| **Alice** | Architect — 인터페이스, 모듈 경계, 의존성 규칙 | [Alice.md](Agents/Alice.md) |
| **Sidney** | Runtime — ECS, Scene, 메모리, 직렬화 | [Sidney.md](Agents/Sidney.md) |
| **Monday** | Graphics — RHI 백엔드, 머티리얼, 셰이더, RenderGraph | [Monday.md](Agents/Monday.md) |
| **Chrono** | Tools & AI — CLI, 스키마, 애셋, AI 연동, 자동화 | [Chrono.md](Agents/Chrono.md) |
| **Seeho** | Review — 코드 리뷰, 테스트, 빌드, 성능 회귀 검증 | [Seeho.md](Agents/Seeho.md) |

**왜 이 규칙이 있는가.** 이 저장소는 서로 다른 컴퓨터에서, 서로 다른 세션의 AI가,
서로를 모르는 채로 기여합니다. 역할이 적혀 있으면 다음 사람이 "이 변경이 어느 경계 안에서
이루어졌는지"를 즉시 압니다. 역할이 없으면 모든 변경을 처음부터 다시 읽어야 합니다.

## 2. PR에 다음 넷을 반드시 적는다

[PR 템플릿](.github/PULL_REQUEST_TEMPLATE.md)이 자동으로 물어봅니다. 빈칸으로 두지 마십시오.

### 왜 이렇게 만들었는가

무엇을 했는지는 diff 가 말합니다. **왜 그 선택을 했는지는 당신만 압니다.**
이 항목이 비어 있으면 6개월 뒤 누군가가 같은 고민을 처음부터 반복합니다.

### 다르게 만들 방법은 없었는가

고려했다가 버린 대안을 적습니다. 버린 이유까지 적습니다.

> 나쁜 예: "다른 방법도 있었지만 이게 나았다"
> 좋은 예: "yaml-cpp 를 vendoring 하는 방법도 있었다. 버린 이유는 에러 메시지를
> 우리가 통제할 수 없기 때문이다. 이 엔진에서 파서의 진단은 제품 기능이다."

**대안이 정말 없었다면 "없었다. 이유는 —" 라고 적으십시오.** 그것도 정보입니다.

### 장점과 단점

단점 칸을 비우지 마십시오. **단점 없는 설계는 없습니다.**
단점이 안 보인다면 아직 충분히 안 본 것이거나, 비용을 다른 곳으로 미룬 것입니다.

> 예: "장점 — 의존성이 없어 클론 즉시 빌드된다.
> 단점 — YAML 전체 명세를 지원하지 않는다. 앵커를 쓰던 문서는 마이그레이션이 필요하다."

### 이전과 무엇이 달라졌는가

성능이면 숫자로. 동작이면 전/후로. 형식이면 마이그레이션 경로까지.

> 예: "본 상수 버퍼 업로드 605MB/프레임 → 451KB/프레임 (1341배).
> 측정: RTX 4060 Ti · 1080p · vsync off · 같은 카메라 경로 598프레임 평균."

## 3. AI와 나눈 대화를 요약해 붙여주면 고맙겠습니다

**필수가 아닙니다.** 다만 붙여주시면 다음 AI가 판단하기 훨씬 쉬워집니다.

작업을 AI와 함께 했다면, 마지막에 이렇게 부탁하십시오.

> "지금까지의 대화를 PR 에 붙일 수 있게 요약해줘. 내가 무엇을 요구했고, 네가 어떤 대안을
> 제시했고, 왜 이 방향으로 정했는지, 중간에 버린 접근이 있으면 그것도 포함해서."

그리고 PR의 `<details>` 블록에 넣어주십시오.

**왜 이게 도움이 되는가.** 코드는 결론만 남기고 과정을 지웁니다. 그런데 다음에 이 코드를
고칠 사람(대개 다른 AI)이 가장 알고 싶어 하는 것은 **이미 시도했다가 안 됐던 것**입니다.
그 기록이 없으면 같은 벽에 다시 부딪힙니다. 대화 요약은 그 벽의 지도입니다.

없어도 PR은 병합됩니다. 있으면 감사합니다.

## 4. 코드가 지켜야 할 것

- **의존성 방향은 아래로만.** `Foundation → Doc → Schema → Verbs`. CMake 가 강제합니다.
- **서드파티를 추가하지 않는다.** 정말 필요하면 PR 에서 먼저 합의합니다.
- **모든 진단은 고치는 법을 말한다.** 코드·위치·힌트 셋이 없으면 진단이 아닙니다.
- **로그는 문자열이 아니라 레코드다.** `ALICE_LOG_*(채널, 이벤트id).F(키, 값)`
- **새 기능에는 테스트가 붙는다.** 특히 진단 품질에 대한 테스트를.
- **주석은 '무엇'이 아니라 '왜'를 적는다.** 무엇은 코드가 말합니다.
- **경고 0으로 빌드된다.** CI 가 `-Werror` 로 검사합니다.

## 5. 병합 전 통과해야 하는 것

```bash
./Scripts/verify.sh          # Windows: .\Scripts\verify.ps1
```

이 하나가 빌드(경고 0) · 테스트 · 샘플 검증 · 문서 정규화 · 생성물 최신 여부 ·
백로그 표를 전부 검사합니다. CI 가 같은 것을 돌립니다.
→ [`.github/workflows/ci.yml`](.github/workflows/ci.yml)

---

## 다른 컴퓨터에서 기여하기

어느 컴퓨터에서든, 어떤 AI 세션에서든 이렇게 시작할 수 있습니다.

```
넌 이제부터 Seeho다.
AGENTS.md 와 Agents/Seeho.md 를 읽고,
Agents/Backlog/seeho/ 에서 작업 하나를 골라 진행하라.
```

Codex 는 [`AGENTS.md`](AGENTS.md) 를, Claude Code 는 [`CLAUDE.md`](CLAUDE.md)(그 파일을 가리킴)를
자동으로 읽습니다. 별도 설정이 필요 없습니다.

에이전트 문서에 **책임 범위, 건드려도 되는 파일, 건드리면 안 되는 파일, 완료 기준**이
적혀 있습니다. 역할만 지키면 서로의 작업이 충돌하지 않습니다.

자세한 절차: [`Docs/ONBOARDING.md`](Docs/ONBOARDING.md) · [`Agents/README.md`](Agents/README.md)

---

## 기반이 된 것

- [Chang-Jin-Lee/AliceEngine-Optimization](https://github.com/Chang-Jin-Lee/AliceEngine-Optimization) — 이 프로젝트의 베이스가 된 DirectX 11 자체 엔진
- 참고한 오픈소스 엔진 20개에서 무엇을 가져왔는지 → [`Docs/ENGINE_SURVEY.md`](Docs/ENGINE_SURVEY.md)

<p align="center">
  <sub>C++20 · 코어 서드파티 의존성 0 · 현재 Null RHI · 목표 DX11 / DX12 / Vulkan / Metal</sub>
</p>
