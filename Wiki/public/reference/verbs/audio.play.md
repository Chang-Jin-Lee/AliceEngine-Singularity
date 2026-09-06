# `audio.play`

sound 문서를 재생한다

sound 문서의 pickMode 에 따라 변주를 고르므로 결과가 매번 다를 수 있다. 결정적 재생이 필요하면 변주가 하나뿐인 문서를 쓰라

분류: `audio` · 비용: `cheap` · **비결정적**

> 같은 입력이어도 결과가 다를 수 있습니다. 리플레이나 결정적 테스트에서 주의하십시오.

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `sound` | `string` | ✓ |  | sound 문서 경로 |
| `target` | `string` |  |  | 대상 액터 이름. 비우면 이 행동이 붙은 액터 자신 |
| `volume` | `number` |  | `1` | 0..1. 문서의 값에 곱해진다 |
| `pitch` | `number` |  | `1` | 재생 속도 배율 |

## 예시

```yaml
do:
  - "audio.play":{"sound":"sounds/jump.sound.yaml"}
```


