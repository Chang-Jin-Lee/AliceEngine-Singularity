---
title: Audio Source
description: 이 액터의 위치에서 소리를 낸다
schemaId: alice/component/audioSource/1
generated: true
---

# Audio Source

> `schema: alice/component/audioSource/1`

이 액터의 위치에서 소리를 낸다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `sound` | `string` | ✓ |  | 재생할 sound 문서 |
| `autoPlay` | `boolean` |  | `false` | 액터가 살아날 때 바로 재생할지 |
| `loop` | `boolean` |  | `false` | 반복 재생 |
| `volume` | `number` |  | `1` | 0..1 |
| `pitch` | `number` |  | `1` | 재생 속도 배율 |
| `spatial` | `boolean` |  | `true` | 3D 위치 기반으로 들릴지 |
| `minDistance` | `number` |  | `1` | 이 거리 안에서는 감쇠 없음(미터) |
| `maxDistance` | `number` |  | `50` | 이 거리 밖에서는 들리지 않음(미터) |

## 확인

```bash
alice schema show alice/component/audioSource/1
alice new audioSource MyThing
```
