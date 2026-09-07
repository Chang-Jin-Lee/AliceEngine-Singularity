# Camera

> `schema: alice/component/camera/1`

장면을 촬영한다. priority 가 가장 높은 카메라가 화면을 담당한다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `projection` | `perspective | orthographic` |  | `"perspective"` | 투영 방식 |
| `fieldOfView` | `number` |  | `60` | 수직 화각(도). perspective 일 때만 쓴다 |
| `orthographicSize` | `number` |  | `5` | 세로 절반 크기(미터). orthographic 일 때만 |
| `nearClip` | `number` |  | `0.1` | 근평면 거리(미터) |
| `farClip` | `number` |  | `1000` | 원평면 거리(미터) |
| `priority` | `integer` |  | `0` | 여러 카메라 중 우선순위. 큰 쪽이 이긴다 |
| `clearColor` | `string | [number x4] | [number x3]` |  |  | 색상. "#rrggbb" 문자열 또는 [r, g, b, a] 배열(0..1) |

## 흔한 실수

**화각을 100도 이상으로 올리면 원근 왜곡이 심해지고 컬링 효율이 급락한다**

```yaml
# 이렇게 쓰면 안 된다
fieldOfView: 120

# 이렇게 쓴다
fieldOfView: 60
```

## 확인

```bash
alice schema show alice/component/camera/1
alice new camera MyThing
```
