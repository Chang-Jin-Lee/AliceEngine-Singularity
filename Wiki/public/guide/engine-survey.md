# 참고 엔진 조사

> 20개 엔진을 받아 빌드하고 실행해 봤습니다. 무엇을 가져왔고 무엇을 버렸는지 적습니다.
> 조사 폴더: `D:\자체엔진 모음`

---

## 규모 (서드파티 제외 소스 파일 수)

| 엔진 | 파일 수 | 라이선스 | 성격 |
|---|---:|---|---|
| ogre | 6,114 | MIT | 20년 넘은 씬그래프 렌더러 |
| godot | 3,873 | MIT | 2D/3D 풀 기능. 사실상 표준 |
| cocos2d-x | 1,907 | MIT | 2D 고전, 모바일 레퍼런스 |
| SpartanEngine | 1,872 | MIT | 바인드리스 GPU 드리븐. **MCP 서버 내장** |
| WickedEngine | 1,267 | MIT | DX12/Vulkan/Metal 모던 렌더링. 1인 개발 |
| urho3d | 1,160 | MIT | 구조가 깔끔한 클래식 (아카이브됨) |
| Piccolo | 374 | MIT | GAMES104 강의용. 레이어 구분이 가장 명확 |
| LumixEngine | 277 | MIT | 에디터까지 완비. "작게 끝까지 만든" 사례 |
| o3de | — | Apache-2.0 | AAA급. Lumberyard/CryEngine 계보 |
| panda3d | — | BSD-3 | 디즈니 출신. C++ 코어 + Python |

이 외에 개인/학생 자체엔진 10개(A.P.T, AuroraEngine, MikuEngine, GOTO-Engine, MMMEngine,
Baroke, DTEngine, Inferno-Engine, JesaSang, Scoopy)도 함께 봤습니다.

---

## 가져온 것

### SpartanEngine — AI 연동의 선례

`source/mcp/` 에 MCP 서버가 이미 들어 있습니다. TCP 47777 에서 요청을 받고,
`McpQueue` 가 서버 스레드에서 메인 스레드로 마샬링하고, 핸들러가 등록제로 붙는 구조입니다.

**가져온 것:** 스레드 마샬링 구조. AI 요청은 임의 시점에 오지만 엔진 상태는
메인 스레드에서만 만질 수 있습니다. 큐가 그 사이를 잇습니다. → `CHR-09`

**안 가져온 것:** 명령이 C++ 함수로 하드코딩되어 있고, 인자 스키마가 없습니다.
`McpCommands.cpp` 하나가 514KB입니다. AI 는 어떤 명령이 있는지 알려면 그 파일을 읽어야 합니다.

우리는 반대로 갑니다 — 동사가 스키마를 들고 등록되고, `alice verbs --json` 하나로 전부 나옵니다.

### WickedEngine — RHI 를 작게 유지하는 법

`wiGraphicsDevice.h` 가 **478줄**입니다. 그 뒤에 DX12, Vulkan, Metal 이 붙어 있습니다.

**가져온 것:**

- 인터페이스를 작게 유지하면 백엔드가 붙는다는 증거. 우리 `RHIDevice.h` 는 ~160줄입니다
- "가장 낮은 공통분모"가 아니라 **현대적 모델을 기준으로 잡고 구형을 에뮬레이션**하는 방향
- 바인드리스 우선

**안 가져온 것:** 배리어를 호출자에게 맡깁니다. 우리는 RenderGraph 가 자동 계산합니다(`MON-06`).
사람도 AI 도 배리어를 정확히 쓰지 못합니다.

### Piccolo — 레이어 구분과 리플렉션

`engine/source/` 가 `core / platform / resource / function / render` 로 깔끔하게 나뉩니다.
그리고 `meta_parser` 라는 libclang 기반 리플렉션 생성기가 있습니다 — 언리얼 UHT 의 축소판입니다.

**가져온 것:** 레이어 이름과 순서. 우리 `Foundation / Doc / Schema / Verbs / RHI / Runtime` 이
같은 발상입니다.

**중요한 반면교사:** Piccolo 의 애셋은 이미 **JSON** 입니다. 그런데 열어보면 이렇습니다.

```json
{ "convert": [1, 7, 55, 29, 33, 34, 35, 36, ...] }
```

**JSON 이라고 다 AI 친화가 아닙니다.** 형식이 아니라 **모양**이 문제입니다.
사람도 AI 도 이 배열이 무엇인지 알 수 없습니다. 스키마도 설명도 없습니다.

그래서 우리는 형식(YAML/JSON)이 아니라 **스키마 + 설명 + 예시 + 진단**을 핵심으로 잡았습니다.

### godot — 씬/리소스 모델

`.tscn` 은 텍스트이고 사람이 읽을 수 있습니다. 리소스가 경로로 참조되고,
씬이 다른 씬을 인스턴싱할 수 있습니다.

**가져온 것:** 텍스트 씬 포맷. 경로 기반 참조. 씬 중첩.

**안 가져온 것:** GDScript. 스크립트 언어를 하나 더 만드는 것은 이 엔진의 전제와 정반대입니다.
그리고 `.tscn` 은 `[node name="X" type="Y" parent="."]` 같은 독자 문법이라
기존 도구가 못 읽습니다. 우리는 표준 YAML 부분집합을 씁니다.

### LumixEngine — 작게 끝까지 만들기

277개 파일로 에디터까지 완비되어 있습니다.

**가져온 것:** 기능을 늘리기 전에 하나를 끝까지 만드는 태도.
Phase 0 에서 렌더러 없이도 **콘텐츠 검증 고리를 완전히 닫은** 것이 이 영향입니다.

### 베이스 엔진 (AliceEngine-Optimization) — 계측의 방법론

가장 많이 가져온 것은 여기입니다.

**ECS 선택의 근거.** ECS 와 GameObject-Component 를 같은 워크로드로 실측한 데이터가 있습니다.
N=100 에서도 이미 약 3배 차이가 났고, 그건 캐시가 아니라 간접 참조와 가상 호출 비용이었습니다.
N=50000 파편화 상태에서는 8배 가까이 벌어졌습니다. → `SID-02` 의 근거입니다.

**성능 주장을 지키는 법.**

> 빨라졌다는 주장은 조건이 어긋나는 순간 무너진다.

그래서 셋을 고정합니다 — 계측 계층, 런타임 토글(한 빌드 안에서 경로만 바꾼다),
재생 가능한 입력(시간이 아니라 프레임 인덱스로 진행). → `SEE-04`

**GPU 프로파일러 구조.** 타임스탬프 쿼리의 disjoint 처리와 다중 프레임 버퍼링은
`Runtime/Rendering/Metrics/GpuProfiler` 에서 그대로 가져올 가치가 있습니다. → `MON-02`

**p99 를 본다.** 체감 끊김은 평균에서 오지 않습니다. `Profiler::Stats()` 가 p50/p95/p99 를 냅니다.

---

## 버린 것과 이유

| 버린 것 | 어디서 봤나 | 왜 |
|---|---|---|
| 스크립트 언어 | godot(GDScript), 유니티(C#), 언리얼(블루프린트 VM) | 언어를 하나 더 만들면 AI 가 배울 것이 하나 더 는다. 선언적 규칙 + 유한한 동사로 대체 |
| 바이너리 애셋 | 언리얼 `.uasset`, 유니티 | AI 가 읽을 수도 diff 할 수도 없다 |
| 독자 텍스트 문법 | godot `.tscn` | 기존 도구가 못 읽는다. 표준 YAML 부분집합을 쓴다 |
| 호출자에게 맡기는 배리어 | WickedEngine, Spartan | 사람도 AI 도 정확히 못 쓴다. RenderGraph 가 계산한다 |
| 무거운 서드파티 | 대부분의 엔진 | 아래 참조 |
| C++ 헤더 리플렉션 생성 | Piccolo `meta_parser`, 언리얼 UHT | libclang 의존이 크고, 스키마를 코드에서 뽑으면 설명·예시·흔한 실수를 담을 곳이 없다 |

### 서드파티를 0으로 만든 이유

받은 엔진 10개를 이 머신에서 빌드해 봤습니다. **6개가 첫 빌드에서 막혔습니다.**

| 막힌 이유 | 어디서 |
|---|---|
| 한글 + 공백 경로에서 빌드 스텝이 쪼개짐 | LumixEngine |
| CMake 4.x 가 `cmake_minimum_required(<3.5)` 거부 | 다수 |
| Windows SDK 28000 의 `DirectX::Internal` 개명 | WickedEngine |
| CP949 로케일 → C4819 경고가 `/WX` 와 만나 에러 | 다수 |
| Python 3.13 에서 `distutils` 제거 | cocos2d-x |
| 병렬 빌드 레이스로 PDB 충돌(C1041) | LumixEngine, panda3d |

전부 **서드파티나 생성 스텝**에서 났습니다. 엔진 자체 코드의 문제가 아니었습니다.

그래서 이 저장소는 의존성이 0입니다. YAML 파서도, JSON 파서도, 테스트 프레임워크도 직접 썼습니다.
합쳐서 3,000줄 남짓입니다. **그 비용이 "클론하면 바로 빌드된다"보다 싸다고 판단했습니다.**

그리고 파서를 직접 쓴 데는 더 중요한 이유가 있습니다 —
**이 엔진에서 파서의 에러 메시지는 제품 기능**입니다. 남의 파서로는 이걸 낼 수 없습니다.

```
player.behavior.yaml:5:3: error[doc.parse.value_eaten_by_comment]:
  'baseColor' 의 값이 주석으로 해석되어 사라졌다
    |   baseColor: #ff8800
    |   ^
    = YAML 에서 공백 뒤의 '#' 는 주석이다. 따옴표로 감싸라: baseColor: "#ff8800"
```

### 빌드 환경에서 얻은 것

이 조사에서 부딪힌 함정들이 `CMakeLists.txt` 와 `CMake/AliceModule.cmake` 에 흡수되어 있습니다.

```cmake
# SDK 28000 의 DirectXCollision 개명을 피한다
set(CMAKE_SYSTEM_VERSION "10.0.26100.0" CACHE STRING "...")

# CP949 로케일에서 C4819 를 막는다
/utf-8

# __cplusplus 가 199711L 로 거짓말하는 것을 막는다
/Zc:__cplusplus

# 표준 전처리기. __VA_ARGS__ 가 GCC/Clang 과 같아진다
/Zc:preprocessor
```

---

## 요약

| 무엇을 | 어디서 | 어떻게 |
|---|---|---|
| AI 요청의 스레드 마샬링 | SpartanEngine | `CHR-09` |
| 작은 RHI 인터페이스 | WickedEngine | `Engine/RHI/RHIDevice.h` (~160줄) |
| 레이어 구분 | Piccolo | `Foundation → Doc → Schema → Verbs` |
| 텍스트 씬 + 경로 참조 | godot | `alice/scene/1` |
| 작게 끝까지 만들기 | LumixEngine | Phase 0 의 범위 결정 |
| ECS 선택의 실측 근거 | 베이스 엔진 | `SID-02` |
| 성능 주장을 지키는 방법론 | 베이스 엔진 | `SEE-04` |
| **"JSON 이라고 다 AI 친화가 아니다"** | Piccolo (반면교사) | 스키마 + 설명 + 예시 + 진단 |
| **"서드파티가 첫 빌드를 막는다"** | 10개 중 6개 (반면교사) | 의존성 0 |

