---
title: 시작하기
description: 5분 안에 첫 콘텐츠를 만든다
order: 0
---

# 시작하기

## 빌드

```bash
git clone https://github.com/<org>/AliceEngine-Singularity
cd AliceEngine-Singularity
./Scripts/build.sh          # Windows: Scripts\build.ps1
```

필요한 것은 **CMake 3.24+ 와 C++20 컴파일러**뿐입니다. 서드파티 의존성이 없습니다.

```bash
./build/bin/alice doctor
```

```
AliceEngine-Singularity 0.1.0

  플랫폼      windows
  콘텐츠 모델 19개 문서 타입 · 32개 동사 · 36개 식 심볼
  렌더 백엔드 null
  AI CLI      claude 있음  ·  codex 있음
```

## 엔진에게 물어보기

**추측하지 마십시오. 물어보십시오.**

```bash
alice schema list           # 만들 수 있는 문서 타입
alice verbs                 # 할 수 있는 동작
alice verbs --symbols       # 조건식에서 읽을 수 있는 이름
alice schema show alice/actor/1
```

## 첫 문서

```bash
alice new material Steel -o materials/steel.material.yaml
```

```yaml
# Material
# 표면이 빛에 어떻게 반응하는지. 셰이더 코드를 쓰지 않고 값으로 기술한다.
# 스키마: alice/material/1
# 필드 설명: alice schema show alice/material/1
#
# 흔한 실수:
#   YAML 에서 공백 뒤의 # 는 주석이다. 색상 문자열은 반드시 따옴표로 감싼다

schema: alice/material/1
name: Steel
shader: pbr
blend: opaque
doubleSided: false
```

`alice new` 는 스키마에서 뼈대를 만듭니다. 필수 필드와 기본값이 채워져 있고,
그 타입의 **흔한 실수**가 주석으로 붙습니다.

## 확인

```bash
alice check materials/
```

```
  1개 문서 · 0개 오류 · 0개 경고  (0.12 ms)
```

일부러 틀려 봅시다.

```yaml
shader: pbrr
params:
  metalic: 1.0
  roughness: 5.0
```

```
materials/steel.material.yaml:3:9: error[schema.enum_mismatch]: 'pbrr' 는 허용된 값이 아니다  (at shader)
    | shader: pbrr
    |         ^
    = 'pbr' 을(를) 뜻했는가? 허용값: pbr, unlit, toon, custom

materials/steel.material.yaml:6:12: error[schema.unknown_field]: 'metalic' 은(는) Material Params 에 없는 필드다
    |   metalic: 1.0
    |            ^
    = 'metallic' 을(를) 뜻했는가? 쓸 수 있는 필드: baseColor, metallic, roughness, ...

materials/steel.material.yaml:7:14: error[schema.out_of_range]: 5.0 는 최댓값 1.0 보다 크다  (at params.roughness)
```

**세 개가 한 번에 나옵니다.** 첫 에러에서 멈추지 않습니다.
AI 가 한 번의 수정으로 전부 고칠 수 있어야 하기 때문입니다.

## AI 로 작업하기

```bash
alice check materials/ --json
```

```json
{
  "ok": false,
  "errors": 3,
  "results": [{
    "path": "materials/steel.material.yaml",
    "diagnostics": [{
      "code": "schema.unknown_field",
      "message": "'metalic' 은(는) Material Params 에 없는 필드다",
      "hint": "'metallic' 을(를) 뜻했는가? 쓸 수 있는 필드: ...",
      "path": "params.metalic",
      "line": 6, "column": 12,
      "snippet": "  metalic: 1.0"
    }]
  }]
}
```

AI 에게 줄 것은 이 JSON 하나입니다.
**코드 · 위치 · 문서 경로 · 힌트**가 다 있으므로 추측 없이 고칩니다.

## 다음

- [콘텐츠 문서 문법](/guide/content-format/) — 문법과 흔한 실수
- [조건식 심볼](/reference/expression-symbols/) — `when:` 에서 쓸 수 있는 이름
- [동사 목록](/api/verbs.json) — 할 수 있는 동작 전부
