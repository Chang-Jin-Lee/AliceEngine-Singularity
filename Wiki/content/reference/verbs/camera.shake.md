---
title: camera.shake
description: 카메라를 흔든다
verbId: camera.shake
generated: true
---

# `camera.shake`

카메라를 흔든다

분류: `camera` · 비용: `cheap` · **비결정적**

> 같은 입력이어도 결과가 다를 수 있습니다. 리플레이나 결정적 테스트에서 주의하십시오.

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `amplitude` | `number` | ✓ |  | 흔들림 크기(미터) |
| `duration` | `number` | ✓ |  | 지속 시간(초) |
| `frequency` | `number` |  | `20` | 초당 흔들림 횟수 |

