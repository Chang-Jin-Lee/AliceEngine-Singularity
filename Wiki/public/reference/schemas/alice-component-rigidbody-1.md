# Rigidbody

> `schema: alice/component/rigidbody/1`

물리 시뮬레이션에 참여한다. Collider 가 함께 있어야 의미가 있다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `mass` | `number` |  | `1` | 질량(kg). kinematic 이면 무시된다 |
| `kinematic` | `boolean` |  | `false` | 직접 움직이고 힘을 받지 않을지 |
| `useGravity` | `boolean` |  | `true` | 중력을 받을지 |
| `linearDamping` | `number` |  | `0` | 선형 감쇠 |
| `angularDamping` | `number` |  | `0.05` | 각 감쇠 |
| `freezePosition` | `[boolean x3]` |  |  | 축별 위치 고정 [x, y, z] |
| `freezeRotation` | `[boolean x3]` |  |  | 축별 회전 고정 [x, y, z] |
| `physicsMaterial` | `string` |  |  | 마찰·반발 계수를 담은 physics 문서 |

## 흔한 실수

**질량 0 은 물리적으로 정의되지 않는다. 힘을 받지 않게 하려면 kinematic 을 쓰라**

```yaml
# 이렇게 쓰면 안 된다
mass: 0

# 이렇게 쓴다
kinematic: true
```

## 확인

```bash
alice schema show alice/component/rigidbody/1
alice new rigidbody MyThing
```

