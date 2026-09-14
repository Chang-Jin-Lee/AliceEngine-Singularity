# `debug.draw`

디버그 도형을 한 프레임 그린다

분류: `diagnostics` · 비용: `cheap`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `shape` | `line | sphere | box | arrow | text` | ✓ |  | 도형 |
| `from` | `[number x3]` |  |  | 시작점 또는 중심 |
| `to` | `[number x3]` |  |  | 끝점. line/arrow 전용 |
| `size` | `[number x3]` |  |  | box 크기 |
| `radius` | `number` |  |  | sphere 반지름 |
| `text` | `string` |  |  | text 도형의 내용 |
| `color` | `string | [number x4] | [number x3]` |  | `"#00ff00"` | 색상. "#rrggbb" 문자열 또는 [r, g, b, a] 배열(0..1) |
| `duration` | `number` |  | `0` | 표시 시간(초). 0 이면 한 프레임 |
