# `vfx.spawn`

effect 문서를 한 번 재생한다

분류: `vfx` · 비용: `moderate` · **비결정적**

> 같은 입력이어도 결과가 다를 수 있습니다. 리플레이나 결정적 테스트에서 주의하십시오.

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `effect` | `string` | ✓ |  | effect 문서 경로 |
| `position` | `[number x3]` |  |  | 월드 좌표. 비우면 target 위치 |
| `target` | `string` |  |  | 대상 액터 이름. 비우면 이 행동이 붙은 액터 자신 |
| `attach` | `boolean` |  | `false` | 대상을 따라다닐지 |
| `scale` | `number` |  | `1` | 전체 크기 배율 |


