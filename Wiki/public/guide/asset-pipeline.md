# 애셋 파이프라인

> 상태: 설계. 구현은 `CHR-05`, `CHR-06`, `ALI-03`.

## 문제

언리얼과 유니티의 애셋 관리에는 공통된 구조가 있습니다.

```
원본 파일  →  임포트  →  엔진 내부 형식  →  런타임
(.fbx)       (설정)     (.uasset/.asset)   (메모리)
```

여기서 AI 가 막히는 지점은 셋입니다.

1. **임포트 설정이 안 보인다.** 유니티 `.meta` 는 GUID 와 숫자 나열이고,
   언리얼은 설정이 `.uasset` 안에 들어 있습니다
2. **결과를 확인할 수 없다.** 임포트가 잘 됐는지 알려면 에디터를 켜야 합니다
3. **모르는 형식은 손도 못 댄다.** 임포터가 없으면 끝입니다

## 설계

### 1. 임포트 설정은 사람과 AI 가 읽는 문서다

```yaml
# meshes/player.fbx.import.yaml
schema: alice/import/mesh/1
source: player.fbx

scale: 0.01                 # cm → m
generateTangents: true
optimizeVertexCache: true
lods:
  - { distance: 0,  ratio: 1.0 }
  - { distance: 20, ratio: 0.5 }
  - { distance: 60, ratio: 0.2 }
materials:
  - slot: Body
    assign: materials/skin.material.yaml
```

이 문서도 스키마로 검증됩니다. `alice check` 가 임포트 설정의 오타를 잡습니다.

### 2. 캐시는 해시 기반, 결과는 조회 가능

```
Cache/
  <원본해시>-<설정해시>/
    mesh.bin
    manifest.yaml        ← 무엇이 나왔는지. 사람이 읽는다
```

`manifest.yaml` 에 정점 수, 서브메시, 본 수, 바운딩 박스가 적힙니다.
AI 는 이걸 읽고 "임포트가 잘 됐는지"를 에디터 없이 판단합니다.

```bash
alice asset info meshes/player.fbx --json
# → { "vertices": 12043, "submeshes": 3, "bones": 54, "bounds": [...] }
```

### 3. 모르는 형식은 AI 가 다룬다 — `alice adapt`

이것이 요구사항 5의 핵심입니다.

```
alice adapt path/to/unknown_thing.xyz
```

동작:

```
1. 형식 감지 시도
   ├─ 알려진 확장자/매직넘버  → 임포터로 처리 (AI 불필요)
   └─ 모름                    → 2로

2. AI 에게 분석 요청
   - 파일 앞부분 (텍스트면 원문, 바이너리면 헥스 덤프)
   - 이 엔진의 문서 타입 목록 (`alice schema list --json`)
   - "이 파일이 무엇이고 어느 타입에 가장 가까운가"

3. AI 가 변환 문서를 만든다

4. `alice check` 로 즉시 검증
   ├─ 통과  → 저장하고, 근거를 .adapt.yaml 로 남긴다
   └─ 실패  → 진단을 AI 에게 되먹이고 3으로 (최대 N회)

5. N회 실패하면 사람에게 보고
   "이 파일을 <타입> 으로 해석하려 했으나 <필드>를 찾지 못했다"
```

**핵심은 4단계입니다.** AI 가 뭔가 만들어냈는데 그게 맞는지 모르는 상황이 안 생깁니다.
검증이 즉시 판정하고, 실패하면 그 진단이 다시 AI 에게 갑니다.

이것이 이 엔진의 전체 설계가 이 기능을 위해 존재한다고 말할 수 있는 이유입니다 —
스키마·진단·CLI 가 없으면 이 고리가 안 돕니다.

### 4. 변환 근거를 남긴다

```yaml
# unknown_thing.xyz.adapt.yaml
schema: alice/adapt/1
source: unknown_thing.xyz
sourceHash: sha256:...
adaptedTo: alice/animation/1
adaptedAt: 2026-09-07T14:32:00Z
model: claude-opus-5
attempts: 2
reasoning: |
  파일 헤더의 "MOTN" 매직과 프레임 배열 구조로 보아 모션 캡처 데이터다.
  본 이름이 Mixamo 규약을 따르므로 표준 리타깃이 가능하다.
  첫 시도에서 defaultState 를 빠뜨려 검증에 실패했고, 두 번째에 채웠다.
```

**재현 가능해야 합니다.** 같은 원본에 같은 설정이면 같은 결과가 나와야 하고,
왜 그렇게 변환됐는지 사람이 읽을 수 있어야 합니다.

## 언리얼 UHT 와의 대응

요구사항에 "언리얼에서 C++을 UHT로 만들어서 블루프린트를 만드는 것처럼"이라고 적혀 있습니다.
대응 관계는 이렇습니다.

| | 언리얼 | 여기 |
|---|---|---|
| 입력 | C++ 헤더 (`UCLASS`, `UPROPERTY`) | 임의의 애셋 파일 |
| 변환기 | UHT (파서) | 임포터 + AI 어댑터 |
| 출력 | 리플렉션 메타데이터 | 엔진 문서 (스키마 검증됨) |
| 소비자 | 블루프린트 에디터 | 런타임 + 에디터 + AI |
| **검증** | 컴파일 에러 | **`alice check` — 즉시, 구조화된 진단** |

방향이 반대입니다. UHT 는 **코드에서 데이터를 뽑고**, 여기서는 **데이터를 코드 없이 쓸 수 있게** 만듭니다.

## 열려 있는 문제

- **AI 없이도 되어야 한다.** 네트워크가 없거나 AI CLI 가 없으면 알려진 형식만 임포트됩니다.
  엔진 자체는 정상 동작해야 합니다
- **비결정성.** AI 변환은 매번 같은 결과가 아닐 수 있습니다.
  `.adapt.yaml` 을 커밋해 결과를 고정하고, 재변환은 명시적으로만 합니다
- **비용.** 큰 애셋을 AI 에게 통째로 보낼 수 없습니다. 헤더와 샘플만 보냅니다
- **신뢰.** AI 가 만든 문서를 그대로 믿지 않습니다. 검증이 통과해야 저장됩니다

