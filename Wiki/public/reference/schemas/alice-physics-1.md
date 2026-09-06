# Physics Material

> `schema: alice/physics/1`

표면의 마찰과 반발. 얼음·고무·금속의 차이가 여기서 온다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `schema` | `string` | ✓ |  | 항상 alice/physics/1 |
| `name` | `string` | ✓ |  | 사람이 읽는 이름. 문서 안에서 참조 대상이 된다 |
| `staticFriction` | `number` |  | `0.6` | 정지 마찰 계수 |
| `dynamicFriction` | `number` |  | `0.6` | 운동 마찰 계수 |
| `restitution` | `number` |  | `0` | 반발 계수. 1 이면 에너지를 잃지 않는다 |
| `density` | `number` |  | `1000` | 밀도(kg/m³) |
| `frictionCombine` | `average | min | max | multiply` |  | `"average"` | 두 표면이 만났을 때 마찰을 합치는 방식 |
| `restitutionCombine` | `average | min | max | multiply` |  | `"average"` | 반발을 합치는 방식 |

## 확인

```bash
alice schema show alice/physics/1
alice new physics MyThing
```

