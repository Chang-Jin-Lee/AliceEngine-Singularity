# `physics.impulse`

강체에 순간적인 충격을 준다

분류: `physics` · 비용: `cheap`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `target` | `string` |  |  | 대상 액터 이름. 비우면 이 행동이 붙은 액터 자신 |
| `direction` | `[number x3]` | ✓ |  | 방향 벡터. 자동으로 정규화된다 |
| `force` | `number` | ✓ |  | 충격 크기(N·s) |

## 예시

```yaml
do:
  - "physics.impulse":{"direction":[0,1,0],"force":5}
```


