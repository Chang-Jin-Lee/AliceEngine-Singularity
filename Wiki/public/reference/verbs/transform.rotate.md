# `transform.rotate`

현재 회전에서 상대적으로 돌린다

분류: `transform` · 비용: `trivial`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `target` | `string` |  |  | 대상 액터 이름. 비우면 이 행동이 붙은 액터 자신 |
| `delta` | `[number x3]` | ✓ |  | 회전량(도) |
| `space` | `world | local` |  | `"local"` | 기준 좌표계 |


