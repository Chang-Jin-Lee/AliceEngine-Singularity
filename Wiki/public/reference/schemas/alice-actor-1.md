# Actor

> `schema: alice/actor/1`

장면에 놓이는 개체 하나. 트랜스폼과 컴포넌트, 그리고 행동으로 이루어진다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `schema` | `string` |  |  | 독립 파일일 때는 alice/actor/1. 씬에 인라인이면 생략 |
| `name` | `string` | ✓ |  | 사람이 읽는 이름. 문서 안에서 참조 대상이 된다 |
| `tags` | `[string]` |  |  | 질의용 태그. 예) [player, damageable] |
| `active` | `boolean` |  | `true` | 처음부터 켜져 있을지 |
| `transform` | `alice-component-transform-1` |  |  | 월드 공간에서의 위치·회전·크기. 모든 액터가 암묵적으로 하나 가진다 |
| `components` | `object` |  |  | 액터에 붙은 기능들. 키가 컴포넌트 종류다 |
| `behaviors` | `[string]` |  |  | 적용할 behavior 문서 경로 목록 |
| `children` | `[alice-actor-1]` |  |  | 부모-자식 계층. 자식 트랜스폼은 부모 기준이다 |

## 확인

```bash
alice schema show alice/actor/1
alice new actor MyThing
```

