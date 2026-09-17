---
title: Platformer Character 2D
description: XY 평면의 비회전 사각형 캐릭터. box collider와 함께 사용하며 고정 지형과 충돌한다
schemaId: alice/component/character2d/1
generated: true
---

# Platformer Character 2D

> `schema: alice/component/character2d/1`

XY 평면의 비회전 사각형 캐릭터. box collider와 함께 사용하며 고정 지형과 충돌한다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `speed` | `number` |  | `4` | 좌우 이동 속도(m/s) |
| `jumpSpeed` | `number` |  | `7` | 접지 상태에서 점프할 때 위쪽 초기 속도(m/s) |
| `gravityScale` | `number` |  | `1` | 씬의 수직 중력 배율 |

## 예시

```json
{
  "speed": 4,
  "jumpSpeed": 7,
  "gravityScale": 1
}
```

## 흔한 실수

**플랫폼어 컨트롤러는 일반 강체가 아니다. rigidbody와 동시에 사용하지 않는다**

```yaml
# 이렇게 쓰면 안 된다
rigidbody: {}

# 이렇게 쓴다
character2d: { speed: 4, jumpSpeed: 7 }
```

## 확인

```bash
alice schema show alice/component/character2d/1
alice new character2d MyThing
```
