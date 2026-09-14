# `log.write`

구조화 로그를 남긴다

콘텐츠가 남긴 로그도 엔진 로그와 같은 NDJSON 스트림으로 나간다. 그래서 AI 가 '이 규칙이 언제 몇 번 발동했나'를 엔진 이벤트와 나란히 놓고 볼 수 있다. 게임플레이 버그를 쫓을 때 이게 가장 빠른 길이다

분류: `diagnostics` · 비용: `trivial`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `level` | `trace | debug | info | warn | error` |  | `"info"` | 로그 레벨 |
| `event` | `string` | ✓ |  | 안정 이벤트 id. 예) gameplay.player.died |
| `message` | `string` |  |  | 사람이 읽을 한 문장 |
| `fields` | `object` |  |  | 기계가 읽을 키-값. 숫자는 숫자로 남겨라 |

## 예시

```yaml
do:
  - "log.write":{"event":"gameplay.player.jumped","fields":{"height":2.4}}
```
