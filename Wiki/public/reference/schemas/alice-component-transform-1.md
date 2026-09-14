# Transform

> `schema: alice/component/transform/1`

월드 공간에서의 위치·회전·크기. 모든 액터가 암묵적으로 하나 가진다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `position` | `[number x3]` |  | `[0,0,0]` | 미터 단위 위치 [x, y, z] |
| `rotation` | `[number x3]` |  | `[0,0,0]` | 오일러 각(도) [pitch, yaw, roll] |
| `scale` | `[number x3]` |  | `[1,1,1]` | 축별 배율. 1 이 원본 크기 |

## 흔한 실수

**위치는 항상 3개짜리 배열이다. 숫자 하나로 줄여 쓸 수 없다**

```yaml
# 이렇게 쓰면 안 된다
position: 0

# 이렇게 쓴다
position: [0, 0, 0]
```

## 확인

```bash
alice schema show alice/component/transform/1
alice new transform MyThing
```
