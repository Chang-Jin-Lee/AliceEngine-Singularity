# Animator

> `schema: alice/component/animator/1`

animation 문서의 상태 그래프를 이 액터의 스키닝 메시에 적용한다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `animation` | `string` | ✓ |  | animation 문서 경로 |
| `playbackSpeed` | `number` |  | `1` | 재생 속도 배율 |
| `applyRootMotion` | `boolean` |  | `false` | 루트 본의 이동을 액터 트랜스폼에 반영할지 |

## 확인

```bash
alice schema show alice/component/animator/1
alice new animator MyThing
```

