# Collider

> `schema: alice/component/collider/1`

충돌 형상. 모양에 따라 쓰는 필드가 다르다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `shape` | `box | sphere | capsule | mesh` | ✓ |  | 충돌 형상 |
| `size` | `[number x3]` |  | `[1,1,1]` | box 의 각 축 크기(미터) |
| `radius` | `number` |  | `0.5` | sphere/capsule 반지름(미터) |
| `height` | `number` |  | `2` | capsule 전체 높이(미터) |
| `asset` | `string` |  |  | mesh 형상일 때의 충돌 메시 |
| `center` | `[number x3]` |  | `[0,0,0]` | 액터 원점 기준 오프셋 |
| `isTrigger` | `boolean` |  | `false` | 겹침만 감지하고 밀어내지 않을지 |
| `layer` | `string` |  | `"default"` | 충돌 레이어 이름 |

## 확인

```bash
alice schema show alice/component/collider/1
alice new collider MyThing
```

