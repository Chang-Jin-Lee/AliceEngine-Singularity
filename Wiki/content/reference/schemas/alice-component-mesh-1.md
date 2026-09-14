---
title: Mesh
description: 정적/스키닝 메시를 그린다
schemaId: alice/component/mesh/1
generated: true
---

# Mesh

> `schema: alice/component/mesh/1`

정적/스키닝 메시를 그린다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `asset` | `string` | ✓ |  | 메시 애셋 경로 |
| `materials` | `[string]` |  |  | 서브메시 순서대로의 머티리얼. 개수가 모자라면 마지막 것이 반복된다 |
| `castShadow` | `boolean` |  | `true` | 그림자를 드리울지 |
| `receiveShadow` | `boolean` |  | `true` | 그림자를 받을지 |
| `visible` | `boolean` |  | `true` | 화면에 보일지 |
| `lodBias` | `number` |  | `1` | LOD 전환 거리 배율. 1 이 기본 |

## 확인

```bash
alice schema show alice/component/mesh/1
alice new mesh MyThing
```
