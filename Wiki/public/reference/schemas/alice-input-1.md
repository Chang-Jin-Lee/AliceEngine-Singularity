# Input Map

> `schema: alice/input/1`

물리 입력을 이름 있는 행동으로 바꾼다. 행동 문서는 키가 아니라 이름을 참조하므로 PC·패드·터치를 같은 콘텐츠로 지원할 수 있다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `schema` | `string` | ✓ |  | 항상 alice/input/1 |
| `name` | `string` | ✓ |  | 사람이 읽는 이름. 문서 안에서 참조 대상이 된다 |
| `actions` | `[object]` | ✓ |  | 행동 목록 |

## 확인

```bash
alice schema show alice/input/1
alice new input MyThing
```

