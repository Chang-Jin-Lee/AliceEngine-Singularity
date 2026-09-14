# Scene

> `schema: alice/scene/1`

액터들이 놓인 하나의 세계. 게임은 씬의 연속이다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `schema` | `string` | ✓ |  | 항상 alice/scene/1 |
| `name` | `string` | ✓ |  | 사람이 읽는 이름. 문서 안에서 참조 대상이 된다 |
| `environment` | `object` |  |  | 씬 전체에 걸리는 설정 |
| `actors` | `[alice-actor-1]` |  |  | 이 씬의 액터들 |
| `preload` | `[string]` |  |  | 씬 진입 전에 미리 올려둘 애셋. 첫 프레임 히칭을 막는다 |

## 확인

```bash
alice schema show alice/scene/1
alice new scene MyThing
```
