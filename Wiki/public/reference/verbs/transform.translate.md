# `transform.translate`

현재 위치에서 상대적으로 움직인다

분류: `transform` · 비용: `trivial`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `target` | `string` |  |  | 대상 액터 이름. 비우면 이 행동이 붙은 액터 자신 |
| `delta` | `[number x3]` | ✓ |  | 이동량 [x, y, z] |
| `space` | `world | local` |  | `"world"` | 기준 좌표계 |
