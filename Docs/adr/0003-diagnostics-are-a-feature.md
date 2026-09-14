# ADR-0003 · 진단은 제품 기능이다

```
상태: 채택
날짜: 2026-09-07
Agent: Alice (Architect)
```

## 맥락

AI 가 콘텐츠를 만들고 검증받는 고리를 돌 때, **한 번의 왕복에서 얼마나 많이 고칠 수 있는가**가
전체 속도를 결정합니다.

보통의 검증기는 이렇게 말합니다.

```
Error: field 'positon' is not allowed at /components/transform
```

AI 는 이걸 받고 스키마 전문을 다시 읽어야 합니다. 그리고 대개 다른 곳을 고칩니다.
왕복이 3~4번 필요해집니다.

## 결정

**모든 진단은 셋을 갖춥니다. 하나라도 없으면 미완성입니다.**

```
안정 코드 + 정확한 위치 + 고치는 법
```

그리고 **첫 에러에서 멈추지 않습니다.** 가능한 모든 문제를 한 번에 보고합니다.

```
player.actor.yaml:12:5: error[schema.unknown_field]: 'positon' 은 Transform 에 없는 필드다  (at transform.positon)
  |       positon: [0, 1, 0]
  |       ^
  = 'position' 을(를) 뜻한 것인가? 쓸 수 있는 필드: position, rotation, scale
```

같은 진단이 `--json` 에서는 이렇게 나옵니다.

```json
{"severity":"error","code":"schema.unknown_field",
 "message":"'positon' 은 Transform 에 없는 필드다",
 "hint":"'position' 을(를) 뜻한 것인가? 쓸 수 있는 필드: position, rotation, scale",
 "file":"player.actor.yaml","path":"transform.positon",
 "line":12,"column":5,"offset":240,"snippet":"      positon: [0, 1, 0]"}
```

### 이 결정이 강제한 것들

이 목표 하나가 아래를 전부 요구했습니다.

| 요구 | 어디에 반영됐나 |
|---|---|
| 모든 값이 소스 위치를 안다 | `doc::Value::mark` |
| 문서 안 경로를 안다 | `Diagnostic::path` (`actors[0].transform.position`) |
| 오타를 제안한다 | `ClosestMatch()` — Levenshtein |
| 여러 에러를 모은다 | `DiagnosticBag` (첫 에러에서 안 멈춤) |
| 스키마가 설명·예시·흔한 실수를 안다 | `Schema::description`, `examples`, `pitfalls` |
| **파서를 직접 쓴다** | ADR-0002 |

**진단 품질이 아키텍처를 결정했습니다.** 반대가 아닙니다.

### 안정 코드

진단 코드(`schema.unknown_field`)는 **안정 식별자**입니다.
한 번 릴리스되면 바꾸지 않습니다. 문구는 바뀌어도 코드는 그대로입니다.

AI 와 CI 가 코드로 분기하기 때문입니다. 문구로 분기하면 번역만 바뀌어도 깨집니다.

## 대안과 버린 이유

### A. 표준 형식(SARIF 등)을 쓴다

정적 분석 도구의 표준이고 GitHub 이 지원합니다.

**버린 이유:** SARIF 에는 **힌트를 담을 자리가 없습니다.**
`fixes` 필드가 있지만 텍스트 치환용이고, "'position' 을 뜻했는가?" 같은 설명이 안 들어갑니다.
→ 나중에 `alice check --format=sarif` 를 **추가**할 수는 있습니다. 내부 모델은 유지합니다.

### B. 첫 에러에서 멈춘다 (컴파일러 방식)

에러가 연쇄하면 뒤의 진단은 대개 잡음입니다.

**버린 이유:** AI 에게는 **한 번에 전부**가 훨씬 낫습니다. 왕복 비용이 사람보다 큽니다.
→ 절충: 파싱 실패면 검증 단계로 안 넘어갑니다(잡음이 확실하므로).
   같은 단계 안에서는 전부 모읍니다.

### C. 힌트를 선택 사항으로 둔다

빨리 만들 수 있습니다.

**버린 이유:** 선택 사항으로 두면 아무도 안 넣습니다.
그래서 테스트로 강제합니다 — `Schema_Tests.cpp` 의 여러 케이스가 특정 진단의
힌트 존재와 내용을 검사합니다.

## 결과

### 좋은 것

- **AI 가 한 번의 왕복에서 여러 곳을 고친다.** 실측: 5개 오류가 있는 문서에서 전부 한 번에 나온다
- 사람도 같은 진단을 읽는다. 문서를 찾아갈 필요가 없다
- `alice explain <코드>` 로 "왜 그 규칙이 있는지"까지 답한다
- 진단 코드가 안정적이라 CI 가 특정 문제를 게이트할 수 있다

### 나쁜 것

- **진단을 만드는 비용이 크다.** 새 검사를 넣을 때마다 힌트 문구를 고민해야 한다
- **번역 부담.** 지금 진단은 한국어다. 영어 지원을 하려면 문구가 두 벌이 된다
  → 코드가 안정 식별자라 매핑은 가능하다. 다만 아직 안 했다
- **문구 변경이 골든 테스트를 깨뜨린다** (SEE-05 이후).
  이건 의도된 것이다 — 진단 변경을 리뷰어가 반드시 보게 만든다
- **오타 제안이 틀릴 수 있다.** 편집 거리는 의미를 모른다.
  → 완화: 임계값을 이름 길이에 비례시켜 엉뚱한 제안을 줄였다

### 측정

이 결정이 지켜지고 있는지 보는 방법:

```bash
alice check <망가진 문서> --json | python -c "
import json,sys
d=json.load(sys.stdin)
diags=[x for r in d['results'] for x in r['diagnostics']]
no_hint=[x for x in diags if not x.get('hint')]
print(f'진단 {len(diags)}개 중 힌트 없음 {len(no_hint)}개')
"
```

힌트 없는 진단의 비율이 늘어나면 이 원칙이 무너지고 있는 것입니다.
