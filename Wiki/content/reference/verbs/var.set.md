---
title: var.set
description: 행동의 지역 변수 값을 바꾼다
verbId: var.set
generated: true
---

# `var.set`

행동의 지역 변수 값을 바꾼다

식(when:)에서는 값을 바꿀 수 없다. 상태를 바꾸는 일은 전부 동사가 한다. 그 규칙이 있어야 규칙들의 평가 순서가 결과에 영향을 주지 않는다

분류: `flow` · 비용: `trivial`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `name` | `string` | ✓ |  | 변수 이름 |
| `value` | `number | boolean | string` | ✓ |  | 새 값 |

