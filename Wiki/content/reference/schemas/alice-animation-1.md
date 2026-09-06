---
title: Animation
description: 클립과 상태 그래프. 블렌드와 전이를 값으로 기술한다
schemaId: alice/animation/1
generated: true
---

# Animation

> `schema: alice/animation/1`

클립과 상태 그래프. 블렌드와 전이를 값으로 기술한다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `schema` | `string` | ✓ |  | 항상 alice/animation/1 |
| `name` | `string` | ✓ |  | 사람이 읽는 이름. 문서 안에서 참조 대상이 된다 |
| `clips` | `[object]` | ✓ |  | 쓸 클립들 |
| `parameters` | `object` |  |  | 전이 조건에서 참조할 파라미터들 |
| `states` | `[object]` | ✓ |  | 상태 그래프 |
| `defaultState` | `string` | ✓ |  | 시작 상태 이름 |

## 확인

```bash
alice schema show alice/animation/1
alice new animation MyThing
```
