# `vfx.stop`

재생 중인 이펙트를 멈춘다

분류: `vfx` · 비용: `cheap`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `effect` | `string` |  |  | 멈출 effect 문서 |
| `target` | `string` |  |  | 대상 액터 이름. 비우면 이 행동이 붙은 액터 자신 |
| `immediate` | `boolean` |  | `false` | 남은 파티클까지 즉시 지울지 |
