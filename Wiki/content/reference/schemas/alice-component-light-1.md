---
title: Light
description: 장면을 밝힌다
schemaId: alice/component/light/1
generated: true
---

# Light

> `schema: alice/component/light/1`

장면을 밝힌다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `type` | `directional | point | spot` | ✓ |  | 광원 종류 |
| `color` | `string | [number x4] | [number x3]` |  | `"#ffffff"` | 색상. "#rrggbb" 문자열 또는 [r, g, b, a] 배열(0..1) |
| `intensity` | `number` |  | `1` | 세기. directional 은 lux, 나머지는 lumen 기준 |
| `range` | `number` |  | `10` | 도달 거리(미터). point/spot 전용 |
| `spotAngle` | `number` |  | `45` | 스팟 원뿔 각도(도). spot 전용 |
| `castShadow` | `boolean` |  | `false` | 그림자를 만들지. 켤수록 비싸다 |
| `shadowBias` | `number` |  | `0.005` | 그림자 여드름을 줄이는 오프셋 |

## 확인

```bash
alice schema show alice/component/light/1
alice new light MyThing
```
