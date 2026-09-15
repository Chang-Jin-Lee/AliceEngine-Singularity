# 현재 실행 상태와 개발 순서

> 기준: 2026-09-15 · Runtime ECS + Windows 에디터 · 엔진 버전 0.1.0

**Windows 에디터와 `alice` 콘텐츠 도구를 실행할 수 있습니다.** 에디터에서 씬의 액터를
선택하고 속성을 수정·검증·저장할 수 있습니다. 게임플레이 실행은 아직 없습니다.
`Samples/FirstLight`는 검증 가능한 콘텐츠 샘플이며, 현재 플레이할 수 있는 게임은 아닙니다.
`Alice.Runtime` 라이브러리의 ECS도 링크·실행할 수 있으며 `Alice.Tests`가 실제 API 호출을 검증합니다.

## 지금 할 수 있는 것

| 기능 | 현재 상태 | 확인 방법 |
|---|---|---|
| Windows 에디터 | 독립 창, 문서 목록·계층·속성·배치 뷰포트·검증·저장 | `pwsh -File Scripts/editor.ps1`, [사용법](EDITOR.md) |
| 문서 모델 | 스키마 19개: 문서 10종 + 컴포넌트 9종 | `alice schema list --json` |
| 게임플레이 문법 | 동사 정의 32개, 조건식 심볼 36개, 정적 검사 | `alice verbs --json` |
| 콘텐츠 검증 | 위치·진단 코드·수정 힌트를 함께 출력 | `alice check Samples --json` |
| 문서 편집 | 뼈대 생성, YAML/JSON 변환, 주석을 보존하는 정규화 | `alice new`, `alice convert`, `alice fmt` |
| 계측 | 코어의 구조화 로그, CPU 프로파일러, 프레임 예산 | `Engine/Foundation/` 및 테스트 |
| 그래픽 추상화 | RHI 인터페이스와 검증용 Null 백엔드 | `alice doctor --json` |
| Runtime ECS | World·컴포넌트 등록·연속 저장·질의·지연 파괴·읽기 가드 구현 | `Engine/Runtime/ECS/`, `Alice.Tests`, [아키텍처](ARCHITECTURE.md#runtime-contract) |
| Runtime 후속 계약 | 렌더 패킷·규칙 실행 공개 헤더와 컴파일 검사 | `Engine/Runtime/RenderExtraction.h`, `Rules.h` |
| 위키 | 정적 사이트, 문서 원문, JSON 레퍼런스, AI용 문서 목록 | `Wiki/`에서 `npm run build` 후 `npm start` |

**동사가 목록에 있다는 것은 문법과 인자를 검증한다는 뜻입니다.** 예를 들어
`physics.impulse`와 `audio.play`의 실제 물리·오디오 실행은 후속 런타임 작업입니다.
`doctor`의 AI CLI 항목은 PATH에서 프로그램을 찾은 결과이며 로그인 확인은 아닙니다.

## 엔진 실행해 보기

Windows 에디터는 저장소 루트에서 `pwsh -NoProfile -File .\Scripts\editor.ps1`로 실행합니다.
빌드 후 `build\bin\Alice.Editor.exe`를 더블클릭해도 됩니다. [에디터 사용법](EDITOR.md)을 보십시오.

Windows에서 저장소 루트를 기준으로 실행합니다. CMake 3.24+와 C++20 컴파일러가 필요합니다.

```powershell
pwsh -NoProfile -File .\Scripts\build.ps1
.\build\bin\alice.exe doctor --json
.\build\bin\alice.exe check Samples --json
.\build\bin\alice.exe verbs --json
.\build\bin\Alice.Tests.exe
```

PowerShell이 스크립트 실행 정책 오류를 표시하는 개인 개발 환경에서는 해당 실행에만
적용되는 `pwsh -NoProfile -ExecutionPolicy Bypass -File .\Scripts\build.ps1`을 사용할 수 있습니다.
관리되는 PC에서는 조직의 정책을 따릅니다.

macOS / Linux에서는 다음을 사용합니다. 이번 작업의 실행 검증은 Windows에서 수행했습니다.

```bash
./Scripts/build.sh
./build/bin/alice doctor --json
./build/bin/alice check Samples --json
```

## 위키 열기

Node.js 22 이상과 npm이 필요합니다. C++ 코어의 서드파티 0 원칙과 별개로,
위키는 Next.js·React 등 `Wiki/package-lock.json`에 기록된 패키지를 사용합니다.

```powershell
cd Wiki
npm ci
npm run build
npm start
```

브라우저에서 [http://localhost:3000](http://localhost:3000)을 엽니다.
수정할 때 자동으로 다시 보고 싶으면 `npm run dev`를 사용합니다.
3000번 포트를 이미 사용 중이면 `npm start -- --port 3001`로 실행합니다.

`build`는 레퍼런스를 재생성하고 `Wiki/out/`에 정적 사이트를 만듭니다.
`start`는 이 결과를 로컬에서 제공합니다. 엔진 실행 파일이 없을 때는 저장소에 포함된
스키마와 API 스냅샷을 사용하므로, 위키만 먼저 빌드할 수도 있습니다.

## 아직 개발 중인 것

| 영역 | 남은 작업 | 백로그 |
|---|---|---|
| Runtime | ECS 구현됨. 씬 로딩, 규칙 실행, 상태 덤프, 프레임 루프 구현이 남음 | [Sidney](../Agents/Backlog/sidney/) |
| 그래픽 | D3D11, D3D12, Vulkan, 셰이더, RenderGraph. Metal도 목표이며 현재 구현 없음 | [Monday](../Agents/Backlog/monday/) |
| 애셋 | 참조 해석, 임포터·캐시, AI 변환 | [ALI-03](../Agents/Backlog/alice/ALI-03.md), [CHR-05](../Agents/Backlog/chrono/CHR-05.md), [CHR-06](../Agents/Backlog/chrono/CHR-06.md) |
| AI 연동 | 엔진 내 대화창, 실행 중 엔진 질의, MCP | [CHR-01](../Agents/Backlog/chrono/CHR-01.md), [CHR-09](../Agents/Backlog/chrono/CHR-09.md) |
| 위키 후속 | 공개 Vercel 배포, 검색, 영어 번역 | [CHR-07](../Agents/Backlog/chrono/CHR-07.md) |

## 다음 엔진 작업

1. **Alice / ALI-02** — 공개 계약 작성·검증 후 작업 기반 브랜치에 병합했습니다.
2. **Sidney / SID-02** — ECS와 엔티티 수명·세대 검사를 구현하고 실행 검증했습니다.
3. **Sidney / SID-03** — FirstLight 문서를 World로 로딩합니다.
4. **Sidney / SID-04** — 조건식을 평가하고 동사를 실행합니다.

화면 출력은 Monday의 실제 렌더 백엔드와 플랫폼 창·입력 구현이 추가로 필요합니다.
Runtime 검증은 우선 Null 백엔드에서 진행할 수 있습니다.

역할별 작업 절차는 [온보딩](ONBOARDING.md), 모듈 경계는 [아키텍처](ARCHITECTURE.md)를 보십시오.

## 이번 검증 기록

2026-09-07, Windows에서 다음을 확인했습니다.

- 공식 `Scripts/verify.ps1`: 경고 0 빌드, 테스트 137개, 샘플 10개 검증,
  문서 정규화, JSON Schema 최신 여부, 백로그 표 검사 통과
- 위키 `npm run build`: 정적 내보내기 성공
- `npm test`: 엔진 없는 환경의 레퍼런스 재생성과 로컬 미리보기 HTTP 동작 통과
- 브라우저: 데스크톱·모바일 화면, 문서 이동, 목차 펼치기, HTML·Markdown·JSON 접근 확인

공개 배포와 macOS/Linux 빌드는 이번 확인에 포함하지 않았습니다.

2026-09-08, ALI-02에서 Runtime 헤더 6개의 단독 컴파일과 실제 소비자 코드의
타입 검사를 MSVC 경고 0 설정으로 확인했습니다. 읽기 포인터의 const 제거는
정적 어설션 실패, RenderView에 World include 추가는 CMake 설정 실패로 검출했습니다.
결함을 되돌린 후 공식 검증 6단계가 다시 통과했습니다. 실행 테스트 수는 137개로
유지되며 이번에 추가한 검사는 런타임 동작 테스트가 아닌 컴파일 계약 검사입니다.

ALI-02의 첫 GitHub CI에서 세 플랫폼의 빌드·테스트는 통과했지만 Linux·macOS의
샘플 정규화가 실패했습니다. 기존 실수 출력의 `to_chars`/`%.17g` 분기가 원인이었습니다.
별도 ALI-05에서 출력 경로를 통일하고 소수·부호 있는 0·극단값 왕복 회귀 검사를 보강했습니다.
수정 커밋 `80710f4`의 [CI run 34228935631](https://github.com/Chang-Jin-Lee/AliceEngine-Singularity/actions/runs/34228935631)에서
Windows·Ubuntu·macOS, 위키, 개발 규칙이 모두 통과했습니다. 샘플 값과 미디어 애셋은 변경하지 않았습니다.

2026-09-09, Runtime 공개 계약 [PR #1](https://github.com/Chang-Jin-Lee/AliceEngine-Singularity/pull/1)과
실수 정규화 수정 [PR #2](https://github.com/Chang-Jin-Lee/AliceEngine-Singularity/pull/2)를 통합했습니다.
PR #2를 PR #1에 병합한 뒤 PR #1을 작업 기반 브랜치 `chrono/CHR-10-local-wiki`에 병합했습니다.
main 병합은 아직 남아 있습니다.

2026-09-14, SID-02의 ECS 구현을 Windows에서 검증했습니다.

- 공식 검증 6단계 통과: 경고 0 빌드, 테스트 148개, 샘플 10개, 정규화·스키마·백로그 검사
- ECS 테스트 11개: 수명·세대·연속 저장·64바이트 정렬·교집합·읽기 잠금·실패 원자성
- 세대 검사, 읽기 잠금, 진단 힌트·위치를 일부러 제거한 세 변형 모두 테스트 실패로 검출 후 복구
- 독립 리뷰에서 발견한 경고 200개 뒤의 검증 누락을 재현 테스트로 확인하고 수정
- 외부 의존성과 미디어 애셋 추가 없음

위키 테스트 2개와 정적 페이지 67개 빌드를 통과했고, 로컬 미리보기에서 HTML·Markdown을
HTTP 200으로 확인했습니다. 구현은 [PR #3](https://github.com/Chang-Jin-Lee/AliceEngine-Singularity/pull/3)에서
리뷰 중입니다. macOS 테스트 출력의 `size_t` 타입 차이를 수정한 `e1cbfb5`의
[GitHub CI](https://github.com/Chang-Jin-Lee/AliceEngine-Singularity/actions/runs/34814297441)에서
Windows·Ubuntu·macOS, 위키, 개발 규칙이 모두 통과했습니다. main 병합과 공개 위키 배포는 남아 있습니다.

2026-09-14, PR #3과 통합 [PR #4](https://github.com/Chang-Jin-Lee/AliceEngine-Singularity/pull/4)를
병합해 위키·CLI·ECS가 main에 들어갔습니다 (`443cd21`).

2026-09-15, CHR-11에서 Windows 에디터를 구현했습니다. 문서 모델 테스트 6개와 실제 창의
smoke 검사 10항목을 통과했습니다. 저장 검증·외부 변경 검사·진단 품질을 제거한 세 변형을
테스트 실패로 검출한 뒤 복구했습니다. 전체 실행 테스트는 154개입니다. 미디어 애셋 추가는 없습니다.
