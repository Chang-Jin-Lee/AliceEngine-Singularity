---
title: audio.setBusVolume
description: 믹서 버스의 음량을 바꾼다
verbId: audio.setBusVolume
generated: true
---

# `audio.setBusVolume`

믹서 버스의 음량을 바꾼다

분류: `audio` · 비용: `trivial`

## 인자

| 인자 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `bus` | `master | music | sfx | voice | ambient | ui` | ✓ |  | 대상 버스 |
| `volume` | `number` | ✓ |  | 0..1 |
| `fade` | `number` |  | `0` | 변화에 걸릴 시간(초) |

