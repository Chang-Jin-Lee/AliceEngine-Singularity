# `profile.zone`

이 동작 묶음을 프로파일러 존으로 감싼다

콘텐츠가 만든 비용도 프로파일러에서 엔진 비용과 같은 축에 보이게 한다. budgetMs 를 주면 초과 시 perf.budget.exceeded 가 나가므로 회귀를 CI 에서 잡을 수 있다

분류: `diagnostics` · 비용: `trivial`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `name` | `string` | ✓ |  | 프로파일러에 표시될 이름 |
| `budgetMs` | `number` |  |  | 프레임 예산(밀리초). 넘으면 경고 로그가 나간다 |
