---
title: Sound
description: 소리 하나의 정의. 변주와 랜덤화를 값으로 기술해 반복감을 없앤다
schemaId: alice/sound/1
generated: true
---

# Sound

> `schema: alice/sound/1`

소리 하나의 정의. 변주와 랜덤화를 값으로 기술해 반복감을 없앤다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `schema` | `string` | ✓ |  | 항상 alice/sound/1 |
| `name` | `string` | ✓ |  | 사람이 읽는 이름. 문서 안에서 참조 대상이 된다 |
| `bus` | `master | music | sfx | voice | ambient | ui` |  | `"sfx"` | 믹서 버스 |
| `variations` | `[object]` | ✓ |  | 같은 소리의 변주들 |
| `pickMode` | `random | randomNoRepeat | sequential | shuffle` |  | `"randomNoRepeat"` | 여러 변주 중 고르는 방식 |
| `volume` | `number | [number x2]` |  |  | 단일 값 또는 [최소, 최대] 범위 |
| `pitch` | `number | [number x2]` |  |  | 단일 값 또는 [최소, 최대] 범위 |
| `loop` | `boolean` |  | `false` | 반복 재생 |
| `maxInstances` | `integer` |  | `8` | 동시에 울릴 수 있는 최대 개수 |
| `cooldown` | `number` |  | `0` | 같은 소리의 최소 재생 간격(초) |

## 확인

```bash
alice schema show alice/sound/1
alice new sound MyThing
```
