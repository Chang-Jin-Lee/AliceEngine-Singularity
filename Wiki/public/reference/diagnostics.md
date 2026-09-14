# 진단 코드

각 코드는 **안정 식별자**입니다. 문구는 바뀌어도 코드는 바뀌지 않습니다.

## `doc.parse.tab_indent`

들여쓰기에 탭을 썼다

YAML 명세가 탭 들여쓰기를 금지한다. 탭 폭이 편집기마다 달라 문서의 구조가 사람마다 다르게 보이기 때문이다.

```yaml
# 이렇게 쓰면 안 된다
root:
	child: 1

# 이렇게 쓴다
root:
  child: 1
```

## `doc.parse.value_eaten_by_comment`

값이 주석으로 해석되어 사라졌다

YAML 에서 공백 뒤의 '#' 는 주석 시작이다. 16진 색상을 그대로 적으면 값 전체가 사라지고 조용히 null 이 된다. 이 엔진에서 가장 흔한 콘텐츠 버그라 파서가 따로 잡아낸다.

```yaml
# 이렇게 쓰면 안 된다
tint: #ff8800

# 이렇게 쓴다
tint: "#ff8800"
```

## `doc.parse.duplicate_key`

같은 키가 두 번 나왔다

대부분의 YAML 파서는 뒤엣것으로 조용히 덮어쓴다. 그러면 앞의 설정이 왜 안 먹는지 아무도 모른다. 이 엔진은 에러로 만든다.

```yaml
# 이렇게 쓰면 안 된다
name: A
name: B

# 이렇게 쓴다
name: B
```

## `doc.parse.anchor_unsupported`

앵커(&)와 별칭(*)은 지원하지 않는다

앵커는 문서를 사람도 AI 도 읽기 어렵게 만든다. 값 재사용은 별도 문서로 빼고 경로로 참조한다.

```yaml
# 이렇게 쓰면 안 된다
base: &common
  a: 1
copy: *common

# 이렇게 쓴다
base: common.yaml  # 별도 파일로 분리
```

## `doc.schema.missing`

문서에 schema: 필드가 없다

이 한 줄이 문서의 정체를 밝힌다. 없으면 무엇으로 검증할지, 어떤 자동완성을 줄지, 런타임이 어떻게 읽을지 아무것도 정해지지 않는다.

```yaml
# 이렇게 쓰면 안 된다
name: Player

# 이렇게 쓴다
schema: alice/actor/1
name: Player
```

## `schema.unknown_field`

스키마에 없는 필드다

모르는 필드를 통과시키면 오타가 영원히 발견되지 않는다. 진단에 가장 가까운 이름과 전체 후보가 함께 나온다.

```yaml
# 이렇게 쓰면 안 된다
positon: [0, 0, 0]

# 이렇게 쓴다
position: [0, 0, 0]
```

## `schema.missing_field`

필수 필드가 없다

필수 필드는 그것 없이는 런타임이 동작을 정할 수 없는 값이다. 기본값으로 때울 수 있었다면 애초에 필수가 아니다.

```yaml
# 이렇게 쓰면 안 된다
components:
  mesh: {}

# 이렇게 쓴다
components:
  mesh:
    asset: meshes/x.mesh
```

## `schema.type_mismatch`

타입이 맞지 않는다

따옴표 하나로 숫자가 문자열이 되는 사고가 가장 흔하다. 진단이 그 경우를 따로 짚어 준다.

```yaml
# 이렇게 쓰면 안 된다
health: "100"

# 이렇게 쓴다
health: 100
```

## `verb.unknown`

없는 동사다

콘텐츠가 부를 수 있는 것은 레지스트리에 등록된 동사뿐이다. 목록이 유한하다는 점이 이 엔진의 설계 의도다.

```yaml
# 이렇게 쓰면 안 된다
- audio.paly: { sound: x }

# 이렇게 쓴다
- audio.play: { sound: x }
```

## `verb.action_multiple_keys`

동작 하나에 키가 여럿이다

각 동작은 동사 하나다. 여러 동작을 하려면 목록에 항목을 더 만든다. 이 규칙이 있어야 실행 순서가 문서 순서와 정확히 일치한다.

```yaml
# 이렇게 쓰면 안 된다
- anim.trigger: { parameter: Jump }
  wait: { seconds: 1 }

# 이렇게 쓴다
- anim.trigger: { parameter: Jump }
- wait: { seconds: 1 }
```

## `expr.assignment`

식에서 대입할 수 없다

조건식은 부작용이 없어야 한다. 그래야 규칙을 어떤 순서로 평가해도 결과가 같다. 값을 바꾸는 일은 do: 의 동사가 한다.

```yaml
# 이렇게 쓰면 안 된다
when: health = 0

# 이렇게 쓴다
when: health == 0
```

## `expr.unknown_name`

식에서 쓸 수 없는 이름이다

식이 읽을 수 있는 상태는 심볼표에 등록된 것과 문서가 선언한 변수뿐이다. `alice verbs --symbols` 로 전체를 볼 수 있다.

```yaml
# 이렇게 쓰면 안 된다
when: physics.grouned

# 이렇게 쓴다
when: physics.grounded
```

## `expr.function_not_called`

함수를 괄호 없이 썼다

input.pressed 는 값이 아니라 함수다. 어떤 행동을 물어보는지 인자로 줘야 한다.

```yaml
# 이렇게 쓰면 안 된다
when: input.pressed

# 이렇게 쓴다
when: input.pressed("Jump")
```

## `perf.budget.exceeded`

존이 프레임 예산을 넘었다

예산은 프로젝트 문서의 budgets 에서 선언한다. 초과는 에러가 아니라 신호다 — 이 로그가 있어야 '느려졌다'는 말이 기계가 추적할 수 있는 사건이 된다.

```yaml
# 이렇게 쓰면 안 된다
budgets: {}

# 이렇게 쓴다
budgets:
  Frame: 16.6
  Render: 8.0
```

## `rhi.leak.detected`

종료 시점에 GPU 리소스가 살아 있다

Null 백엔드는 누수를 이름과 함께 보고한다. 진짜 백엔드에서는 조용히 메모리만 늘어나므로 CI 에서 Null 로 먼저 잡는다.

```yaml
# 이렇게 쓰면 안 된다
device->CreateBuffer(...)  // Destroy 없음

# 이렇게 쓴다
device->CreateBuffer(...); ... device->Destroy(handle);
```
