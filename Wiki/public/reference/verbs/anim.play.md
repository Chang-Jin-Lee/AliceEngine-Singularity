# `anim.play`

상태 그래프의 특정 상태로 즉시 전환한다

분류: `animation` · 비용: `cheap`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `target` | `string` |  |  | 대상 액터 이름. 비우면 이 행동이 붙은 액터 자신 |
| `state` | `string` | ✓ |  | animation 문서의 상태 이름 |
| `blend` | `number` |  | `0.15` | 블렌드 시간(초) |
