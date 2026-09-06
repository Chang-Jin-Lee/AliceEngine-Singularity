# 콘텐츠 문서 문법

> 콘텐츠를 만들려면 이 문서 하나면 됩니다.

## 첫 줄

모든 문서는 자기가 무엇인지 밝히면서 시작합니다.

```yaml
schema: alice/actor/1
```

이 한 줄이 있어야:

- 검증기가 무엇으로 검사할지 안다
- 에디터가 자동완성을 붙인다
- AI 가 파일을 열자마자 정체를 안다 (추측하지 않는다)
- 형식이 바뀌어도 버전으로 마이그레이션 경로가 생긴다

```bash
alice schema list          # 쓸 수 있는 타입 전부
alice schema show <id>     # 그 타입의 필드
alice new actor Player     # 올바른 뼈대 생성
```

---

## YAML 부분집합

**전체 YAML 이 아닙니다.** 게임 콘텐츠에 필요 없는 기능은 뺐습니다.
빼는 만큼 사람도 AI 도 헷갈릴 여지가 줄어듭니다.

### 지원하는 것

```yaml
# 주석

키: 값
중첩:
  키: 값

목록:
  - 항목
  - 키: 값        # 목록 안의 맵
    다른키: 값

벡터: [0, 1, 0]           # 플로우 시퀀스
설정: {a: 1, b: 2}        # 플로우 맵

평문: 따옴표 없이
홑따옴표: '리터럴 그대로'
겹따옴표: "이스케이프 \n 가능"

여러줄: |
  줄바꿈을
  그대로 유지

접기: >
  이어지는
  문장은 한 줄로

없음: null      # 또는 ~
참: true
거짓: false
```

### 지원하지 않는 것 (전부 명확한 에러와 대안 안내가 나옵니다)

| 안 되는 것 | 왜 | 대신 |
|---|---|---|
| 앵커 `&x` / 별칭 `*x` | 사람도 AI 도 읽기 어렵다 | 별도 파일 + 경로 참조 |
| 태그 `!!str` | 타입은 스키마가 정한다 | 지운다 |
| 복합 키 `? key` | 키는 항상 문자열이다 | — |
| 다중 문서 (`---` 반복) | 파일 하나에 문서 하나 | 파일을 나눈다 |
| **탭 들여쓰기** | 편집기마다 폭이 다르다 | 공백 2칸 |

---

## 타입 판정

| 쓴 것 | 읽히는 타입 |
|---|---|
| `42` | 정수 |
| `4.5` `1e3` | 실수 |
| `true` `false` | 불리언 |
| `null` `~` | 없음 |
| `hello` | 문자열 |
| `"42"` | **문자열** (따옴표가 승격을 막는다) |
| `1.0.0` `2026-09-07` | 문자열 (숫자로 안 읽힌다) |
| `007` | 정수 7 (YAML 규칙) |

### `no` 는 문자열입니다

YAML 1.1 은 `no` / `yes` / `on` / `off` 를 불리언으로 읽습니다.
노르웨이 국가코드 `NO` 가 `false` 가 되는 유명한 사고가 여기서 나옵니다.

**이 엔진에서 불리언은 `true` 와 `false` 뿐입니다.**

```yaml
country: NO       # 문자열 "NO" — 의도한 대로
enabled: no       # 문자열 "no" — 불리언이 아니다!
enabled: false    # 이렇게 써야 한다
```

---

## 가장 흔한 실수

### 1. 색상에 따옴표를 안 붙인다

```yaml
tint: #ff8800       # 값이 통째로 사라진다
tint: "#ff8800"     # 이렇게
```

YAML 에서 공백 뒤의 `#` 는 주석 시작입니다. 값이 조용히 `null` 이 됩니다.
**파서가 이 경우를 따로 잡아냅니다** — 그만큼 흔합니다.

```
error[doc.parse.value_eaten_by_comment]: 'tint' 의 값이 주석으로 해석되어 사라졌다
  = YAML 에서 공백 뒤의 '#' 는 주석이다. 따옴표로 감싸라: tint: "#ff8800"
```

### 2. 벡터를 숫자 하나로 쓴다

```yaml
position: 0             # 에러
position: [0, 0, 0]     # 이렇게
```

### 3. 값에 콜론이 들어간다

```yaml
url: http://x/y         # 이건 된다 (콜론 뒤에 공백이 없다)
title: 제목: 부제        # 에러
title: "제목: 부제"      # 이렇게
```

### 4. 숫자를 따옴표로 감싼다

```yaml
health: "100"           # 문자열이 된다
health: 100             # 이렇게
```

---

## 게임플레이 — 스크립트 대신 규칙

**이 엔진에는 게임플레이 스크립트 언어가 없습니다.**

```yaml
schema: alice/behavior/1
name: PlayerMovement

variables:
  jumpCount: 0
  maxJumps: 2

rules:
  - when: input.pressed("Jump") and jumpCount < maxJumps
    do:
      - physics.impulse: { direction: [0, 1, 0], force: 6.5 }
      - var.add: { name: jumpCount, amount: 1 }
    once: false
    cooldown: 0.1
```

### `when:` — 조건식

읽기만 합니다. 대입할 수 없습니다.

```
input.pressed("Jump") and physics.grounded
physics.speed > 0.1
math.clamp(health / maxHealth, 0, 1) < 0.3
not self.hasTag("stunned")
```

연산자: `and` `or` `not` (또는 `&&` `||` `!`),
`==` `!=` `<` `<=` `>` `>=`, `+` `-` `*` `/` `%`

```bash
alice verbs --symbols     # 쓸 수 있는 이름 전부
```

### `do:` — 동사 목록

상태를 바꾸는 일은 전부 여기서 합니다.

```bash
alice verbs               # 전체 목록
alice verbs audio.play    # 하나의 인자와 예시
```

**동작 하나에 동사 하나입니다.**

```yaml
do:
  - anim.trigger: { parameter: Jump }
    wait: { seconds: 1 }        # 에러 — 키가 둘이다

do:
  - anim.trigger: { parameter: Jump }
  - wait: { seconds: 1 }        # 이렇게
```

### 왜 이렇게 나누는가

`when` 이 읽기만 하고 `do` 만 쓰기 때문에,
**같은 프레임의 규칙들을 어떤 순서로 평가해도 결과가 같습니다.**

스크립트 언어라면 규칙 A 가 상태를 바꿔서 규칙 B 의 조건이 달라질 수 있습니다.
그런 버그는 재현이 안 되고, 6개월 뒤에 돌아옵니다.

---

## 확인하는 법

```bash
alice check Content/                 # 사람이 읽는 진단
alice check Content/ --json          # AI 가 읽는 진단
alice fmt Content/                   # 정규화 (diff 안정화)
alice explain doc.parse.tab_indent   # 왜 그 규칙이 있는지
```

진단은 항상 셋을 줍니다.

```
파일:줄:열: error[안정코드]: 무엇이 잘못됐는지  (at 문서.안.경로)
  | 문제가 있는 줄
  |         ^
  = 어떻게 고치는지
```

---

## 에디터 자동완성

```bash
alice schema emit Schemas
```

`.vscode/settings.json` 이 이미 생성된 스키마를 파일 패턴에 물려 두었습니다.
VS Code 에 [YAML 확장](https://marketplace.visualstudio.com/items?itemName=redhat.vscode-yaml)을
설치하면 필드 자동완성과 실시간 오타 검사가 동작합니다.

