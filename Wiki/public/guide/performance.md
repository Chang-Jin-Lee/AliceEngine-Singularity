# 성능 — 로깅 · 프로파일링 · 예산

> 요구사항 2: "처음부터 최적화가 손쉽게 가능하도록. 처음 구현부터 프로파일링과 로깅을 지원."

성능 도구는 나중에 붙이는 것이 아닙니다. 나중에 붙이면 그때는 이미
어디가 느린지 알 수 없는 코드가 되어 있습니다.

---

## 로그는 문자열이 아니라 레코드다

```cpp
ALICE_LOG_ERROR("asset", "asset.load.failed")
    .Msg("메시를 열지 못했다")
    .F("path", path)
    .F("bytes", size)
    .F("cached", false);
```

같은 레코드가 두 곳으로 나갑니다.

**콘솔 (사람)**
```
14:32:05.123 [E] asset      asset.load.failed  메시를 열지 못했다  path=...  bytes=1024  cached=false
```

**NDJSON 파일 (AI)**
```json
{"ts":1757251925123000000,"lv":"error","ch":"asset","ev":"asset.load.failed","frame":124,"tid":1,"msg":"메시를 열지 못했다","src":"Loader.cpp:88","f":{"path":"...","bytes":1024,"cached":false}}
```

### 왜 이렇게 하는가

요구사항이 "AI 가 수많은 로그를 파악해 원인을 좁힌다" 입니다.
사람이 읽는 문장만 뱉으면 AI 는 매번 정규식을 새로 짜야 하고,
문장이 조금만 바뀌어도 깨집니다.

그래서 셋을 함께 들고 다닙니다.

1. **안정 이벤트 id** (`asset.load.failed`) — 릴리스되면 바꾸지 않는다
2. **타입이 있는 필드** — 숫자는 숫자로. 그래야 비교와 집계가 된다
3. **사람용 문장** — 콘솔에서 눈으로 훑기 위한 것

### 프레임 번호가 들어간다

모든 레코드에 `frame` 이 실립니다. 시간이 아니라 프레임으로 볼 수 있어야
렌더링 문제를 짚을 수 있습니다. "124프레임에 무슨 일이 있었나"가 질의 가능해집니다.

### 채널별 레벨

```cpp
Log::SetLevel(LogLevel::Info);              // 전역
Log::SetChannelLevel("rhi", LogLevel::Trace); // 렌더만 자세히
```

병목을 쫓을 때 한 채널만 내립니다. 전체를 Trace 로 내리면 로그가 신호가 아니라 잡음이 됩니다.

### 인자 평가를 건너뛴다

```cpp
ALICE_LOG_DEBUG("test", "x").F("v", ExpensiveCall());
```

레벨이 꺼져 있으면 `ExpensiveCall()` 이 **호출되지 않습니다.**
매크로가 `if (!IsEnabled(...)) {} else ...` 로 감싸기 때문입니다.
테스트가 이걸 강제합니다 (`Log.LevelFilteringSkipsArgumentEvaluation`).

---

## 프로파일러

```cpp
ALICE_PROFILE_ZONE("Render.Shadow");   // 스코프 존
Profiler::BeginFrame(frameIndex);
Profiler::EndFrame();
```

소비자가 둘이고 원하는 모양이 정반대입니다.

- **사람** — 프레임 하나를 눈으로 훑고 싶다 → 플레임 그래프, 클릭해서 내려간다
- **AI** — "지금 뭐가 제일 비싼가"를 한 번의 질의로 → 정렬된 JSON

같은 이벤트 스트림에서 두 표현을 뽑습니다. **계측을 두 번 하지 않습니다.**

### 사람용 — Chrome Trace

```cpp
Profiler::WriteChromeTrace("frame.trace.json", 60);
```

`chrome://tracing` 또는 [Perfetto](https://ui.perfetto.dev) 로 엽니다.

### AI용 — Hotspots JSON

```cpp
Profiler::HotspotsJson(20, 1);
```

```json
{
  "frame": 1240,
  "frameMs": 18.4,
  "stats": { "avgMs": 16.1, "p50Ms": 15.9, "p95Ms": 18.2, "p99Ms": 24.7 },
  "budgetViolations": 3,
  "hotspots": [
    { "name": "Render.Shadow", "selfMs": 6.2, "totalMs": 6.2, "maxMs": 8.1, "calls": 4 },
    { "name": "Physics.Step",  "selfMs": 3.1, "totalMs": 3.4, "maxMs": 4.0, "calls": 1 }
  ]
}
```

`selfMs` 로 정렬합니다. **자식을 제외한 시간**이 진짜 범인을 가리킵니다.

### p99 를 본다

```
avgMs  16.1     ← 괜찮아 보인다
p99Ms  24.7     ← 여기가 사람이 느끼는 끊김이다
```

체감 끊김은 평균에서 오지 않습니다. 베이스 엔진의 측정에서도
GPU 평균 51.21ms 일 때 1% low 는 62.33ms 였습니다.

---

## 예산 — 이 설계의 핵심

```yaml
# project.yaml
budgets:
  Frame: 16.6
  Render: 8.0
  Physics: 2.0
```

존이 예산을 넘으면 구조화 로그가 나갑니다.

```json
{"lv":"warn","ch":"perf","ev":"perf.budget.exceeded",
 "f":{"zone":"Render.Shadow","actualMs":9.4,"budgetMs":8.0,"overMs":1.4,"calls":4,"frame":1240}}
```

### 왜 이게 중요한가

**"게임이 느려요" 라는 사람의 말이 기계가 처리할 수 있는 사건으로 바뀝니다.**

- AI 가 로그만 보고 범인을 짚는다
- CI 가 회귀를 자동으로 잡는다 (`Profiler::BudgetViolationCount()`)
- 언제부터 넘기 시작했는지 커밋 단위로 추적된다

예산 없이는 "느리다"가 주관적 인상으로 남습니다.
예산이 있으면 **선언된 목표를 넘은 사건**이 됩니다.

### 콘텐츠도 예산을 선언한다

```yaml
# effect 문서
budget:
  maxParticles: 64
  maxDrawCalls: 2
```

```yaml
# behavior 문서 안에서
- profile.zone: { name: EnemyAI, budgetMs: 0.5 }
```

콘텐츠가 만든 비용도 엔진 비용과 같은 축에 보입니다.

---

## 성능 주장을 지키는 법

베이스 엔진에서 얻은 방법론입니다.

> **빨라졌다는 주장은 조건이 어긋나는 순간 무너진다.**

그래서 셋을 고정합니다.

### 1. 계측 계층

GPU 타임스탬프 쿼리로 패스별 시간을, 파이프라인 통계 쿼리로 드로우콜과 정점 수를 모읍니다.
추정하지 않습니다.

### 2. 런타임 토글

**두 빌드를 비교하지 않고, 한 빌드 안에서 경로만 바꿉니다.**

컴파일러 버전, 최적화 플래그, 메모리 레이아웃이 달라지면 비교가 무의미해집니다.
되살린 최적화마다 코드에 ID 주석을 남겨 무엇을 껐는지 추적합니다.

### 3. 재생 가능한 입력

사람이 한 번 조작한 경로를 파일로 남기고, 양쪽 실행이 그 파일을 재생합니다.
**시간이 아니라 프레임 인덱스로 진행**하므로 두 실행의 프레임 번호가 어긋나지 않습니다.

### 조건을 반드시 함께 적는다

```
RTX 4060 Ti · 1920×1080 · vsync off · RelWithDebInfo · 같은 카메라 경로 598프레임 평균
```

조건 없는 숫자는 숫자가 아닙니다.

---

## Null 백엔드로 GPU 없이 검증

`Engine/RHI/NullDevice.cpp` 는 드로우콜, 디스패치, 렌더패스, 파이프라인 변경,
업로드 바이트를 셉니다.

```
{"ev":"rhi.frame.stats","f":{"frame":1240,"draws":6700,"dispatches":12,
 "passes":8,"pipelineChanges":34,"uploadedBytes":462848}}
```

GPU 없는 CI 러너에서도 **드로우콜 회귀**를 잡을 수 있습니다.
"이 커밋 이후 드로우콜이 1.6배가 됐다"가 GPU 없이 검출됩니다.

---

## 도구

```bash
alice doctor                              # 환경
alice check Content/ --json               # 콘텐츠 검증

# 엔진이 떠 있을 때 (CHR-01 이후)
alice engine profile --json               # 지금 뭐가 비싼가
alice engine logs --level=warn --json     # 최근 경고
alice engine trace --frames=60 -o f.json  # Perfetto 로 열 트레이스
```
