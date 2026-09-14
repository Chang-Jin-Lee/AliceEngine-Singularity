# Project

> `schema: alice/project/1`

프로젝트 전체 설정. 엔진이 가장 먼저 읽는 문서다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `schema` | `string` | ✓ |  | 항상 alice/project/1 |
| `name` | `string` | ✓ |  | 사람이 읽는 이름. 문서 안에서 참조 대상이 된다 |
| `version` | `string` |  | `"0.1.0"` | 프로젝트 버전 문자열 |
| `startScene` | `string` | ✓ |  | 실행 시 처음 여는 씬 |
| `contentRoot` | `string` |  | `"Content"` | 콘텐츠 문서들의 루트 디렉터리 |
| `renderer` | `object` |  |  | 렌더링 설정 |
| `budgets` | `object` |  |  | 프로파일러 존별 프레임 예산(밀리초). 넘으면 구조화 로그가 나간다. 예) { Frame: 16.6, Render: 8.0, Physics: 2.0 } |
| `logging` | `object` |  |  | 로깅 설정 |
| `platforms` | `[windows | macos | linux | ios | android | console]` |  |  | 빌드 대상 플랫폼 |

## 확인

```bash
alice schema show alice/project/1
alice new project MyThing
```
