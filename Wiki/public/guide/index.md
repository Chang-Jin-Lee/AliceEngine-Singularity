# 시작하기

**현재 실행 가능한 것은 콘텐츠 도구 `alice`입니다.** 게임 창과 에디터는 아직 없습니다.
이 가이드에서는 문서를 만들고 검사합니다. 전체 구현 상태는 [현재 상태](/guide/status/)를 보십시오.

## 빌드

```bash
git clone https://github.com/Chang-Jin-Lee/AliceEngine-Singularity.git
cd AliceEngine-Singularity
```

필요한 것은 **CMake 3.24+ 와 C++20 컴파일러**입니다. C++ 코어의 서드파티 의존성은 없습니다.

Windows / PowerShell:

```powershell
pwsh -NoProfile -File .\Scripts\build.ps1
.\build\bin\alice.exe doctor --json
.\build\bin\alice.exe check Samples --json
# 이 터미널에서 아래 예시의 짧은 명령을 사용한다
Set-Alias alice (Resolve-Path .\build\bin\alice.exe).Path
```

macOS / Linux:

```bash
./Scripts/build.sh
./build/bin/alice doctor --json
./build/bin/alice check Samples --json
export PATH="$PWD/build/bin:$PATH"
```

`doctor`는 스키마 19개, 동사 정의 32개, 심볼 36개와 `null` 백엔드를 보고합니다.
`aiCli` 항목은 각 PC의 PATH에 따라 달라지며 로그인 상태를 검사하지 않습니다.
샘플 검증에서 문서 10개가 오류·경고 없이 통과하면 콘텐츠 도구를 사용할 준비가 된 것입니다.

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

- [현재 상태와 개발 순서](/guide/status/) — CLI·위키 실행과 아직 없는 기능
- [콘텐츠 문서 문법](/guide/content-format/) — 문법과 흔한 실수
- [조건식 심볼](/reference/expression-symbols/) — `when:` 에서 쓸 수 있는 이름
- [동사 목록](/api/verbs.json) — 할 수 있는 동작 전부
