# Particles

> `schema: alice/component/particles/1`

effect 문서를 이 액터 위치에서 재생한다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `effect` | `string` | ✓ |  | effect 문서 경로 |
| `playOnStart` | `boolean` |  | `true` | 액터가 살아날 때 바로 재생 |
| `loop` | `boolean` |  | `false` | 끝나면 다시 시작 |

## 확인

```bash
alice schema show alice/component/particles/1
alice new particles MyThing
```

