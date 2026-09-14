---
title: physics.force
description: 매 프레임 지속되는 힘을 가한다
verbId: physics.force
generated: true
---

# `physics.force`

매 프레임 지속되는 힘을 가한다

분류: `physics` · 비용: `cheap`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `target` | `string` |  |  | 대상 액터 이름. 비우면 이 행동이 붙은 액터 자신 |
| `direction` | `[number x3]` | ✓ |  | 방향 벡터 |
| `force` | `number` | ✓ |  | 힘의 크기(N) |

