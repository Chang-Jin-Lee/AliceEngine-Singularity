# `actor.spawn`

actor 문서로부터 새 액터를 만든다

분류: `actor` · `scene` · 비용: `expensive`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `actor` | `string` | ✓ |  | actor 문서 경로 |
| `name` | `string` |  |  | 새 액터의 이름. 비우면 자동 생성 |
| `position` | `[number x3]` |  |  | 월드 좌표 |
| `rotation` | `[number x3]` |  |  | 오일러 각(도) |
| `parent` | `string` |  |  | 부모 액터 이름 |
