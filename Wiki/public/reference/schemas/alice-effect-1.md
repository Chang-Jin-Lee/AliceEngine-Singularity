# Effect

> `schema: alice/effect/1`

파티클 이펙트. 에미터를 겹쳐 하나의 연출을 만든다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `schema` | `string` | ✓ |  | 항상 alice/effect/1 |
| `name` | `string` | ✓ |  | 사람이 읽는 이름. 문서 안에서 참조 대상이 된다 |
| `duration` | `number` |  | `1` | 전체 길이(초). 0 이면 무한 |
| `loop` | `boolean` |  | `false` | 끝나면 다시 시작 |
| `emitters` | `[object]` | ✓ |  | 에미터들 |
| `budget` | `object` |  |  | 성능 상한. 넘으면 perf.budget.exceeded 로그가 나간다 |

## 확인

```bash
alice schema show alice/effect/1
alice new effect MyThing
```
