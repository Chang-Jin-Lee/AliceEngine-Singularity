# `physics.teleport`

물리 보간 없이 즉시 이동시킨다. 순간이동에는 setPosition 대신 이것을 쓴다

분류: `physics` · 비용: `cheap`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `target` | `string` |  |  | 대상 액터 이름. 비우면 이 행동이 붙은 액터 자신 |
| `position` | `[number x3]` | ✓ |  | 월드 좌표 |
| `resetVelocity` | `boolean` |  | `true` | 속도를 0 으로 만들지 |
