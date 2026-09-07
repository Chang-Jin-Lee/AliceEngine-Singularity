---
title: 새 세션 시작하기
synced: Docs/ONBOARDING.md
---

# 새 세션 시작하기

> 어느 컴퓨터에서든, 어느 AI 도구에서든 이 저장소에 기여를 시작하는 방법입니다.
> **복사해서 붙여넣으면 됩니다.**

---

## 0. 한 번만 — 환경 준비

필요한 것은 둘뿐입니다.

- **CMake 3.24+**
- **C++20 컴파일러** (MSVC 2022+, GCC 13+, Clang 17+)

서드파티 의존성이 0이라 설치할 것이 더 없습니다.

```bash
git clone https://github.com/<org>/AliceEngine-Singularity
cd AliceEngine-Singularity

./Scripts/build.sh          # Windows: .\Scripts\build.ps1
./build/bin/alice doctor
```

`doctor` 가 이렇게 나오면 준비된 것입니다.

```
AliceEngine-Singularity 0.1.0

  플랫폼      windows
  콘텐츠 모델 19개 문서 타입 · 32개 동사 · 36개 식 심볼
  렌더 백엔드 null
  AI CLI      claude 있음  ·  codex 있음
```

---

## 1. Codex 에서 시작하기

Codex 는 저장소 루트의 `AGENTS.md` 를 자동으로 읽습니다. 별도 설정이 없습니다.

### 대화형

```bash
cd AliceEngine-Singularity
codex
```

그리고 첫 메시지로:

```
넌 이제부터 Sidney다.
AGENTS.md 와 Agents/Sidney.md 를 읽고,
Agents/Backlog/sidney/ 에서 상태가 대기인 작업 하나를 골라 진행하라.

시작하기 전에 어느 작업을 골랐는지 알려주고,
그 작업의 상태를 진행중으로 바꾸는 커밋을 먼저 만들어라.
```

### 작업을 지정해서

```bash
codex "넌 이제부터 Monday다. Agents/Backlog/monday/MON-02.md 를 진행하라."
```

### 비대화형 (CI, 자동화)

```bash
codex exec "넌 Seeho다. ./Scripts/verify.sh 를 돌리고 실패한 것이 있으면 원인을 보고하라."
```

### Codex 에게 이 저장소를 이해시키는 가장 빠른 길

```
alice verbs --json 과 alice schema list --json 을 먼저 실행해서
이 엔진이 무엇을 할 수 있는지 파악한 다음 작업을 시작하라.
```

이 저장소의 설계 의도가 그것입니다 — **문서를 읽지 않고도 엔진에게 직접 물어볼 수 있게.**

---

## 2. Claude Code 에서 시작하기

Claude Code 는 `CLAUDE.md` 를 읽고, 그 파일이 `AGENTS.md` 를 가리킵니다.

```bash
cd AliceEngine-Singularity
claude
```

```
넌 이제부터 Chrono다.
AGENTS.md 와 Agents/Chrono.md 를 읽고,
Agents/Backlog/chrono/ 에서 작업 하나를 골라 진행하라.
```

비대화형:

```bash
claude -p "넌 Seeho다. ./Scripts/verify.sh 를 돌리고 결과를 요약하라."
```

---

## 3. 다른 AI 도구에서

`AGENTS.md` 한 파일을 컨텍스트에 넣으면 됩니다. 그 파일이 나머지를 전부 가리킵니다.

위키가 배포되어 있다면 더 간단합니다.

```
https://<위키주소>/llms.txt        전체 지도 (5KB)
https://<위키주소>/llms-full.txt   전문 (64KB)
```

---

## 4. 역할 고르기

어느 역할을 맡을지 모르겠다면, **하려는 일**로 고르십시오.

| 하려는 일 | 역할 |
|---|---|
| 모듈 경계를 정하거나 인터페이스를 설계한다 | **Alice** |
| ECS, 씬 로딩, 규칙 실행을 만든다 | **Sidney** |
| 렌더 백엔드, 셰이더를 만든다 | **Monday** |
| CLI, 스키마, 동사, 애셋, AI 연동, 위키 | **Chrono** |
| 테스트, CI, 리뷰, 성능 회귀 | **Seeho** |

여전히 모르겠으면 **사용자에게 물어보십시오.** 역할 없이 코드를 고치면 안 됩니다.

---

## 5. 작업 고르기

```bash
python Scripts/backlog.py
```

```
Sidney
  SID-01  완료  L    문서 값 모델과 파서
  SID-02  대기  L    ECS World — 컴포넌트 저장소
  SID-03  대기  L    Scene 로더
  ...
```

`상태: 대기` 이고 `선행:` 이 전부 끝난 것 중에서 고릅니다.

**고른 즉시 상태를 바꾸고 그것만 먼저 커밋하십시오.**

```bash
# Agents/Backlog/sidney/SID-02.md 에서
#   상태: 대기  →  상태: 진행중
#   담당: -     →  담당: <내 이름 또는 세션 표시>

git add Agents/Backlog/sidney/SID-02.md
git commit -m "chore(backlog): SID-02 진행중

Agent: Sidney (Runtime)
Task: SID-02"
```

여러 컴퓨터가 동시에 작업하는 구조에서 **이것이 충돌을 막는 유일한 장치**입니다.

---

## 6. 작업하기

작업 문서에 적힌 것을 그대로 따릅니다.

- **무엇을** — 만들 것
- **왜** — 이 작업이 존재하는 이유. 판단이 필요할 때 여기로 돌아옵니다
- **완료 기준** — 체크박스 전부를 만족시켜야 끝입니다
- **건드리는 파일** / **건드리면 안 되는 것** — 경계

막히면 [`AGENTS.md`](../AGENTS.md) 의 "절대 규칙 다섯"을 다시 보십시오.

---

## 7. 끝내기

```bash
./Scripts/verify.sh          # Windows: .\Scripts\verify.ps1
```

전부 통과하면:

1. 백로그 항목의 `상태:` 를 `리뷰` 로 바꿉니다
2. 커밋합니다 — `Agent:` 와 `Task:` 를 반드시 포함
3. PR 을 엽니다 — 템플릿의 넷을 채웁니다 (왜 · 대안 · 장단점 · 차이)
4. 가능하면 AI 와 나눈 대화 요약을 붙입니다 (선택이지만 다음 사람에게 큰 도움)

---

## 자주 묻는 것

**Q. 빌드가 `No CMAKE_CXX_COMPILER could be found` 로 실패합니다**

Windows 에서 Visual Studio 개발자 환경 밖에서 `cmake` 를 부른 것입니다.
`Scripts/build.ps1` 을 쓰십시오. 그 스크립트가 `vswhere` 로 VS 를 찾아 들어갑니다.

**Q. 어떤 동사가 있는지 어떻게 압니까**

```bash
alice verbs                 # 사람용 목록
alice verbs --json          # 기계용 (인자 스키마 포함)
alice verbs audio.play      # 하나의 상세
alice verbs --symbols       # 조건식에서 읽을 수 있는 이름
```

**Q. 이 필드가 뭔지 모르겠습니다**

```bash
alice schema show alice/actor/1
```

**Q. 이 에러가 왜 나는지 모르겠습니다**

```bash
alice explain <진단코드>
```

진단 자체에도 대개 `= hint:` 로 고치는 법이 붙어 있습니다.

**Q. 남의 영역을 건드려야 합니다**

구현(.cpp)이면 그 역할의 담당입니다. 공개 헤더를 바꿔야 한다면
[인터페이스 변경 이슈](../.github/ISSUE_TEMPLATE/interface_change.md)를 열어 Alice 의 판단을 받으십시오.

**Q. 서드파티를 쓰고 싶습니다**

[ADR-0002](adr/0002-no-third-party.md) 를 먼저 읽으십시오.
그래도 필요하다면 PR 에서 [Alice 의 판단 기준 넷](../Agents/Alice.md)에 답하십시오.
