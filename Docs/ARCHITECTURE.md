# 아키텍처

## 의존성 그래프

아래 그래프는 각 모듈이 사용할 수 있는 의존성을 나타냅니다.

```
Foundation                로그 · 프로파일러 · 진단 · 파일시스템 · 시간
   │                      (아무것도 의존하지 않는다)
   ├─ Doc                 문서 값 모델 · YAML/JSON 파서 · 직렬화
   │    └─ Schema         콘텐츠 모델 · 검증 · JSON Schema 생성
   │         └─ Verbs     동사 레지스트리 · 조건식 · 2차 검사
   │
   ├─ RHI                 그래픽 추상화 + Null 백엔드
   │
   └─ Platform (예정)     창 · 입력 · 플랫폼 파일시스템      → ALI-04

Runtime (ECS 구현)        World · 컴포넌트 저장소 / Scene · 규칙 평가는 후속 SID-*
   ├─ Doc, Schema, Verbs
   └─ Foundation          RHI 의존 없음

Renderer (예정)           Runtime의 RenderView → RHI 명령으로 변환

Asset (예정)              임포터 · 캐시 · AI 어댑터          → ALI-03, CHR-05
Bridge (예정)             AI 연동                            → CHR-01
Tools/AliceCLI            alice 명령
```

**현재 강제 범위.** `alice_module()`은 CMake 타깃 의존성을 선언하고 공통 빌드 설정을
적용합니다. 소스 루트 include 경로만으로 금지된 include를 막을 수는 없습니다.
ALI-02에서 추가한 설정 단계 검사는 Runtime 공개 헤더의 직접 include를 제한하며,
`RenderView.h`가 Foundation 외의 엔진 모듈을 include하면 실패합니다.
기존 모듈 전체의 전이적 include까지 검사하는 장치는 아직 없습니다.

---

## 왜 이 순서인가

### Foundation 이 아무것도 의존하지 않는 이유

로거와 프로파일러는 **모든 것보다 먼저** 살아 있어야 합니다.
초기화 실패도 로그로 남아야 하고, 어서션이 로거를 쓸 수 있어야 합니다.

그래서 `AssertFailed()` 는 로거를 거치지 않고 `stderr` 로 직접 갑니다.
로거 자신이 어서트할 수 있어야 하기 때문입니다.

### Doc 이 Schema 보다 아래인 이유

파싱과 검증은 다른 일입니다. 파싱은 "문법이 맞는가", 검증은 "의미가 맞는가"입니다.

분리해두면 스키마 없이도 문서를 읽을 수 있습니다.
`alice convert` 와 `alice fmt` 가 스키마를 모르는 문서에도 동작하는 이유입니다.

### Verbs 가 Schema 위인 이유

동사의 인자가 스키마이기 때문입니다.
`physics.impulse` 의 `direction: [x,y,z]` 를 검증하려면 스키마가 필요합니다.

### RHI 가 Doc/Schema 와 형제인 이유

그래픽 장치는 문서를 몰라도 됩니다. Runtime은 CPU 렌더 패킷을 추출하고,
향후 Renderer/Asset 계층이 애셋 참조를 GPU 리소스와 파이프라인 상태로 변환합니다.
RHI는 이미 해석된 명령만 받습니다.

이 분리 덕분에 **문서 검증이 GPU 없이 됩니다.** CI 에서 그게 결정적입니다.

---

## 핵심 계약

### 1. 왕복이 닫힌다

```
문서 → 메모리 → 문서
```

두 문서가 의미상 같아야 합니다. `Engine/Tests/Doc_RoundTrip_Tests.cpp` 가 강제합니다.

**왜 중요한가.** AI 가 "현재 상태를 읽고 → 고치고 → 되쓰는" 고리를 돌리려면
왕복이 손실 없어야 합니다. 한 번 저장할 때마다 정보가 조금씩 사라지면 그 고리가 못 돕니다.

이 계약을 지키기 위해 한 것들:

- 맵의 **키 순서를 보존**합니다. 해시맵을 쓰면 저장할 때마다 순서가 흔들려 diff 가 의미를 잃습니다
- 정수와 실수를 **구분**합니다. `3` 과 `3.0` 이 왕복 후에도 그대로여야 합니다
- 문자열이 다른 타입으로 읽힐 수 있으면 **따옴표를 붙여** 씁니다 (`"123"`, `"true"`)

### 2. 모든 값이 소스 위치를 안다

```cpp
struct Value {
    Mark mark;   // 줄 · 열 · 바이트 오프셋
};
```

이 한 필드가 진단 품질의 전부입니다. 검증 실패가 "어느 줄"을 말할 수 있어야
AI 가 스스로 고칩니다.

### 3. 진단은 셋을 갖춘다

```
안정 코드 + 위치 + 힌트
```

힌트 없는 진단은 미완성입니다. `Engine/Tests/Schema_Tests.cpp` 가 주요 진단의
힌트 존재를 테스트로 강제합니다.

### 4. 로그는 레코드다

같은 레코드가 콘솔에는 문장으로, 파일에는 NDJSON 한 줄로 나갑니다.
사람과 AI 가 **같은 데이터**를 봅니다.

### 5. 조건식은 부작용이 없다

`when:`은 읽기만 하고 `do:`의 동사만 씁니다. 읽기 전용 접근만으로는 규칙 순서
독립성을 보장하지 못합니다. 전체 조건 평가 중 World와 입력을 고정하고,
수집한 명령을 정해진 순서대로 적용해야 합니다. 아래 Runtime 계약이 그 경계를 정합니다.

<a id="runtime-contract"></a>

## Runtime 공개 계약 — ALI-02

상태: **SID-02에서 World와 ComponentRegistry를 구현**했습니다. `Alice.Runtime` 정적
라이브러리를 링크해 ECS를 실행할 수 있습니다. 씬 로딩·규칙 실행·렌더 추출은 후속 작업이며,
범용 게임플레이 실행은 아직 없습니다. Windows 에디터는 Tools/AliceEditor에서 별도로 제공하며,
공개 헤더 컴파일 검사와 ECS 실행 테스트를 함께 제공합니다.

에디터의 `PlaySession`은 공개 World API로 Transform을 등록하고 큐브 예시의 키 입력과
Inspector 값을 적용합니다. 편집 문서와 실행 World를 분리하며 Stop 시 World를 버립니다.
이는 SID-03 씬 로더나 SID-04 behavior 실행기를 대체하지 않습니다. `TerminalSession`은
Windows ConPTY의 별도 읽기·쓰기 스레드를 사용하며 GUI와 엔진 모듈에 쉘 실행을 섞지 않습니다.

### 파일과 책임

| 헤더 | 계약 | 후속 구현 |
|---|---|---|
| `RuntimeTypes.h` | World/레지스트리 소속 ID, 진단 원본 위치 | SID-02 |
| `ComponentRegistry.h` | 스키마↔네이티브 타입 등록 및 변환 콜백 | SID-02, SID-03 |
| `World.h` | 엔티티 수명, 컴포넌트 접근, AND 질의, 읽기 가드 | SID-02 |
| `RenderView.h` | World와 무관하게 보관할 수 있는 CPU 패킷 | SID-03 추출 / Monday 소비 |
| `RenderExtraction.h` | 읽기 가드에서 렌더 패킷을 만드는 연결부 | SID-03 |
| `Rules.h` | 조건식 심볼 읽기, 명령 수집/적용 경계 | SID-04 |

### 엔티티와 읽기 수명

`World::Create`는 동결한 컴포넌트 레지스트리를 공유 소유합니다. 엔티티는
`{world, index, generation}`으로 식별합니다. 0은 무효이며 서로 다른 World의 ID,
파괴 후 재사용된 슬롯의 옛 세대는 거부합니다. World/레지스트리 번호는 프로세스에서
재사용하지 않고, 세대 번호가 소진되면 슬롯을 폐기합니다. 이 번호는 저장 파일에 쓰지 않습니다.

`AcquireRead()`로 얻은 이동 전용 `WorldReadView`가 살아 있는 동안 **모든 변경 API**는
`runtime.world.read_locked`로 실패합니다. 생성·컴포넌트 교체·네이티브 편집뿐 아니라
파괴 예약과 파괴 확정도 포함합니다. 여러 읽기 가드는 허용합니다. World 객체를 먼저
파괴해도 마지막 가드가 해제될 때까지 저장소와 레지스트리가 살아 있습니다.
이 API는 단일 스레드 호출 계약이며 동시 호출을 위한 락 API는 아닙니다.

`Get<T>`는 등록된 `nativeType`과 `typeid(T)`가 일치해야 `const T*`를 반환합니다.
포인터는 해당 가드를 해제하거나 이동 대입으로 교체할 때까지 빌린 값입니다.
이동으로 넘겨받은 가드가 원래 수명을 이어받습니다. 이동된 원본은 파괴/이동 대입만
가능하며 그 외 호출은 계약 위반입니다. `Query`는 요구 타입의 AND 교집합을
`(index, generation)` 순으로 반환합니다. 빈 요구 목록은 전체 엔티티이며, 중복 요구는
한 번으로 취급하고 잘못된 컴포넌트 ID는 오류로 보고합니다.

`DestroyEntity`는 중복 예약을 성공으로 취급합니다. 예약된 엔티티는 확정 전까지
조회·편집 가능합니다. `CommitDestructions`는 읽기 가드가 없을 때 한 번에 처리하고
세대를 증가시킵니다. 생성·추가·삭제는 읽기 가드가 없으면 즉시 반영합니다.
엔티티 이름·태그·계층, 씬 환경과 물리 상태는 SID-03 이후의 계약이며 이 ECS API에
임의의 문자열 컴포넌트나 스키마를 새로 만들어 넣지 않습니다.

### 컴포넌트 등록과 문서 경계

`ComponentDescriptor`는 actor 컴포넌트 키, 버전이 포함된 스키마 ID, C++ 타입,
크기·정렬, 생성·파괴·이동 생성·decode·encode 콜백을 연결합니다.
빈 키/ID, `void` 타입, 0 크기, 2의 거듭제곱이 아닌 정렬, 정렬의 배수가 아닌 크기,
필수 콜백 누락과 키·스키마·네이티브 타입의 중복 등록은 실패합니다.
등록 콜백은 신뢰하는 C++ 어댑터입니다. `type_index`는 잘못된 소비자 캐스트를 검출하며,
콜백이 거짓 크기나 타입을 등록하는 C++ 프로그래밍 오류까지 검증하지는 않습니다.

Runtime은 정렬을 맞춘 빈 저장소에 `construct`, 교체용 빈 저장소에 `moveConstruct`를
호출합니다. 이동 후 원본도 살아 있는 객체이므로 반드시 `destroy`해야 합니다.
실패한 decode는 임시 객체만 파괴하고 기존 값은 유지합니다. 콜백은 인자의 포인터를
저장하거나 예외를 던지지 않습니다. 함수 코드의 수명은 레지스트리보다 길어야 합니다.

`Freeze`는 스키마 레지스트리가 null인지, ID와 참조가 해석되는지 확인한 뒤 등록을
막습니다. 실패 시 등록 가능한 상태를 유지하고, 이미 동결된 레지스트리에 다시 호출하면
오류입니다. 전달한 스키마 레지스트리와 그 스키마 그래프는 보유 기간 동안 다른 별칭으로도
변경하지 않는 것이 호출자의 계약입니다. `shared_ptr<const Registry>` 자체가 내부
스키마의 불변성까지 보장하지는 않습니다. 스키마 핫 리로드는 새 레지스트리/World를 만듭니다.

`ComponentRegistry::Validate`가 동결한 스키마를 사용하는 공개 검증 경로입니다.
전달받은 진단 모음에 모든 검증 진단을 추가하고 기존 항목을 지우지 않습니다.
이번 호출의 첫 오류를 Status로 반환하며 이전에 들어 있던 오류는 반환 상태에 영향을
주지 않습니다. 씬 로더는 이 API로 여러 컴포넌트의 오류를 모을 수 있습니다.
`SetComponent`도 이 경로로 검증한 뒤 decode하여 추가 또는 교체합니다. `EditComponent`는
같은 타입의 네이티브 값을 콜백 범위에서 수정하는 빠른 경로입니다. 입력 검증은 편집
호출 전에 끝내야 하고, 콜백 안에서는 World에 재진입하거나 포인터를 보관하지 않습니다.
실패할 수 있는 편집은 먼저 임시 값으로 계산하고 성공한 값만 적용합니다.
`EncodeComponent`는 소유한 `doc::Value`를 반환합니다.

문서의 모든 값을 매 프레임 `doc::Value`로 해석하는 방식은 배제했습니다. 타입별 연속
저장은 SID-02 sparse set 구현이 맡고, 문서 변환은 로딩/편집/덤프 경계에서만 수행합니다.
네이티브 값의 encode는 현재 상태의 문서화를 위한 것이며 주석·원본 필드 순서·생략된 기본값
복원까지 약속하지 않습니다. 원본 보존과 변경 병합은 SID-05에서 별도로 검증합니다.

### 렌더러에 전달하는 것

`ExtractRenderView(view)`는 현재 변환을 읽어 mesh/camera/light 배열을 새로 만듭니다.
SID-03의 9개 기본 컴포넌트 바인딩과 네이티브 타입을 사용하므로 추출 구현은 그 작업과
함께 들어갑니다. 등록 이름은 `FindComponent`로 조회하며 임의의 ID를 가정하지 않습니다.
필요한 등록 자체가 없거나 타입이 다르면 진단을 반환합니다. 등록은 있으나 엔티티에
해당 컴포넌트가 없으면 그 배열에 항목을 만들지 않습니다. 대상이 없으면 빈 배열이 성공입니다.

결과는 문자열·행렬·배열을 직접 소유하고 ECS 포인터, EntityId, RHI 핸들을 담지 않습니다.
World 제거 후에도 패킷을 소비할 수 있고, RHI는 이 헤더조차 include할 필요가 없습니다.
메시와 머티리얼 경로는 프로젝트 상대 경로이며 아직 읽어 들인 리소스가 아닙니다.
추출 중 파일 I/O·GPU 호출은 하지 않습니다. 머티리얼 override가 비면 애셋 기본값을 쓰고,
개수가 부족하면 마지막 override를 반복하는 현재 스키마 의미를 유지합니다.

행렬은 열 우선 저장, 열벡터, 미터, 왼손 좌표계(+Y 위, +Z 전방)입니다.
Euler 변환은 SID-03에서 `worldFromLocal = parent * T * Ry(yaw) * Rx(pitch) * Rz(roll) * S`로
고정하고 비가환 회전 테스트를 붙입니다. 시야각은 수직 도 단위이고 직교 크기는 수직 반높이입니다.
스팟 광원의 `spotAngleDegrees`는 원뿔 전체 벌어진 각도이며 셰이딩의 반각은 그 절반입니다.
색은 선형 RGBA이며 스키마 색 문자열의 sRGB RGB를 선형화하고 alpha는 그대로 둡니다.
GPU 투영의 깊이 범위·Y 반전은 Renderer가 백엔드에 맞게 변환합니다.

SID-03의 활성 계층 밖 엔티티와 `mesh.visible=false`는 추출에서 제외합니다.
메시/광원은 엔티티 순, 카메라는 priority 내림차순 뒤 엔티티 순입니다.
카메라 선택은 소비자가 하며 0개도 유효합니다. 씬 환경·스키닝·파티클 렌더·프레임 보간은
이 첫 패킷에 포함하지 않았습니다. 기본 카메라·정적 메시·광원부터 검증하는 범위입니다.

### 규칙 읽기와 변경 순서

1. 프레임 루프가 입력·물리·시간·지역 변수 스냅샷을 준비하고 `AcquireRead`를 얻습니다.
2. 모든 조건은 같은 가드와 고정된 `IRuleSymbols`로 평가합니다. 심볼 구현은 외부 장치를
   그때그때 읽지 않습니다. 조건 실패 진단은 `RuleContext.source`와 AST의 mark를 유지합니다.
3. 참인 규칙의 명령을 `ActionCommand`로 수집합니다. 인자는 읽기 단계에서 모두 해석하여
   소유 값으로 저장합니다. once/cooldown/wait 상태 갱신도 이 단계의 관찰값을 바꾸지 않습니다.
4. 가드를 모두 해제한 다음 `ApplyCommands`를 호출합니다. 먼저 읽기 잠금, 소속 World와
   살아 있는 self, self와 정렬 키의 일치, 중복 키를 전체 검사합니다. 하나라도 잘못되면
   전체 배치를 실행하지 않습니다.
5. 키 `(entityIndex, entityGeneration, behavior, rule, action)` 오름차순으로 실행합니다.
   behavior/rule/action은 문서 내 0 기반 위치입니다. 수집 순서가 달라도 실행 순서는 같습니다.
6. 성공한 명령은 남기고 실패 진단을 모아 뒤 명령을 계속 실행합니다. executor는 한 명령의
   실패가 그 명령의 상태 변경을 남기지 않게 해야 합니다. **전체 배치 롤백은 없습니다.**
7. 프레임 루프가 `CommitDestructions`를 호출합니다. 파괴 예약 뒤 같은 엔티티를 대상으로
   한 명령도 확정 전에는 실행됩니다. executor가 중간에 파괴 확정을 호출하면 계약 위반입니다.

`ApplyCommands`는 기존 진단 모음을 비우지 않고 이번 호출의 진단을 뒤에 추가합니다.
사전 검사 또는 실행에서 발생한 첫 오류를 실패 Status에 담고, 이번 호출에서 오류가
없으면 성공합니다. 진단 모음에 이미 있던 오류는 이번 반환 상태를 바꾸지 않습니다.

같은 값에 두 번 set하면 뒤 set이 이기고, add/translate는 그 시점 값에 순서대로 적용합니다.
예를 들어 `set(10)` 뒤 `add(2)`는 12, `add(2)` 뒤 `set(10)`은 10입니다.
**문서 내 동사 순서는 의미가 있습니다.** 보장하는 것은 같은 초기 상태·입력·문서에서
조건 평가/명령 수집 스케줄을 바꿔도 결과가 같다는 것이며, 부동소수점 계산의 플랫폼 간
비트 동일성은 아닙니다. 실행기에서 동사를 지원하지 않으면 실패 진단을 반환합니다.
정적 동사 목록 32개를 실제 실행 바인딩 32개로 간주하지 않습니다.

### 실패 식별자와 검증

새 진단은 아래 코드를 사용하고, 가능한 경우 `SourceLocation`의 file/path/mark를 보존합니다.
문서가 없는 API 호출은 위치를 지어내지 않고 엔티티/컴포넌트를 path에 식별합니다.
기존 스키마 검증 오류는 원래 코드를 유지합니다. 모든 실패에는 수정 힌트가 있어야 합니다.

| 코드 | 상황 / 힌트 내용 |
|---|---|
| `runtime.registry.invalid_descriptor` | 잘못된 등록 정보 / 빠진 콜백·키·크기·정렬 수정 |
| `runtime.registry.duplicate` | 중복 등록 / 기존 ComponentId 재사용 |
| `runtime.registry.frozen` | 동결 후 변경 / 새 레지스트리 생성 |
| `runtime.registry.not_frozen` | World 생성 전 동결 누락 / Freeze 호출 |
| `runtime.registry.schema_unresolved` | null 레지스트리, 미등록 스키마나 참조 / 스키마 등록 후 재시도 |
| `runtime.component.unknown` | 다른 레지스트리 ID 또는 미등록 이름 / Find로 다시 조회 |
| `runtime.component.missing` | 엔티티에 해당 컴포넌트 없음 / SetComponent로 추가 |
| `runtime.component.type_mismatch` | 네이티브 타입 불일치 / 등록된 타입으로 Get/Edit |
| `runtime.component.invalid_edit` | 편집 콜백 null / 유효한 편집 함수 제공 |
| `runtime.entity.invalid` | 소속·세대·수명 오류 / 현 World에서 ID 재조회 |
| `runtime.world.read_locked` | 가드 보유 중 변경 / 모든 읽기 가드 해제 후 변경 |
| `runtime.world.reentrant` | 변경 콜백 안에서 World 재진입 / 콜백 종료 후 읽기·변경 |
| `runtime.storage.capacity` | 저장소 한도 초과 / World 또는 컴포넌트 수 축소 |
| `runtime.storage.allocation_failed` | 네이티브 저장소 할당 실패 / 메모리 확보 후 재시도 |
| `runtime.component.codec_failed` | 코덱이 빈 오류 코드를 반환 / 코덱 진단과 입력 수정 |
| `runtime.command.invalid_order` | self와 정렬 키 불일치 / 문서 위치와 self로 키 재생성 |
| `runtime.command.duplicate_order` | 중복 명령 키 / 규칙당 각 action을 한 번만 수집 |
| `runtime.verb.unbound` | 실행 바인딩 없음 / 지원하는 바인딩 등록 또는 규칙 수정 |

조건 연산/codec/추출 도메인의 구체적인 오류 코드는 SID-03/04에서 진단 테스트와 함께
확장합니다. 이 표의 코드를 변경하거나 기존 스키마 진단을 새 코드로 덮어쓰지 않습니다.

기본 빌드의 `Alice.Runtime.Contracts`가 헤더 6개를 각각 단독 컴파일하고 소비자 예제와
읽기 constness·수명 타입·소유 필드를 컴파일 검사합니다. 실행 테스트를 꺼도 유지됩니다.
`CMake/RuntimeContracts.cpp.in`은 검사 소스이며 Runtime 구현이 아닙니다.
SID-02의 실행 테스트 11개는 stale ID·가드 잠금·실패 원자성·코덱 수명·64바이트 정렬과
연속 저장·교집합 질의를 검사합니다. 명령 정렬 충돌은 SID-04에서 검증합니다.

### ECS 구현 — SID-02

`Engine/Runtime/ECS/`는 세대 슬롯과 타입별 sparse/dense 풀을 사용합니다. 풀 성장 시
move-construct 후 원본을 파괴하며, 제거 시 마지막 원소를 이동해 연속 저장을 유지합니다.
문서 설정은 스키마 검증과 임시 객체 decode를 마친 뒤 교체하므로 실패 시 기존 값이 남습니다.
읽기 가드는 공유 저장소를 소유하므로 World 객체를 먼저 파괴해도 네이티브 포인터가 유효합니다.
이 API는 단일 스레드에서 사용하며, 읽기 가드가 있는 동안 모든 변경을 거부합니다.

질의는 가장 작은 풀을 기준으로 교집합을 모아 엔티티 인덱스순으로 반환합니다.
`World.Query`, `World.CommitDestructions` 프로파일 영역을 제공합니다. 질의 결과의 배열 할당과
정렬 비용이 있으며, 성능 수치는 아직 측정하지 않았습니다.

### 대안과 비용

- **렌더러가 ECS를 직접 순회:** 복사가 줄지만 ECS 저장 방식·포인터 수명과 결합합니다.
  소유 패킷을 선택했으며 매 추출 시 배열·문자열 복사 비용을 냅니다. 재사용/리소스 ID
  최적화는 실제 측정 후 공개 계약을 유지하는 범위에서 진행합니다.
- **전체 World 복사 스냅샷:** 여러 쓰기 스레드를 격리할 수 있지만 지금의 단일 프레임 루프에
  복사·버전 관리가 추가됩니다. 읽기 가드로 변경을 거부하는 방식을 먼저 사용합니다.
- **doc::Value를 ECS의 주 저장소로 사용:** 등록이 단순해지지만 타입별 연속 순회와 저비용
  네이티브 접근을 잃습니다. 명시적 코덱을 선택하며 수명·정렬 규약을 구현해야 합니다.
- **가상 World 인터페이스를 모듈마다 추가:** 모킹은 쉽지만 구현체가 하나인 현 단계에서
  인터페이스 수가 늘어납니다. World는 PImpl 선언, 실제 교체 지점인 심볼/동사 실행기만
  가상 인터페이스로 둡니다. SID-02의 구현은 이 공개 계약을 유지합니다.

---

## 모듈별 요약

| 모듈 | 줄 수(대략) | 핵심 파일 |
|---|---:|---|
| Foundation | 2,000 | `Log.h` (구조화 로깅), `Profiler.h` (예산) |
| Doc | 2,300 | `YamlParser.cpp` (진단이 제품인 파서) |
| Schema | 2,300 | `CoreSchemas.cpp` (**콘텐츠 모델 그 자체**) |
| Verbs | 2,000 | `CoreVerbs.cpp` (**엔진 API 표면 그 자체**) |
| RHI | 2,000 | `RHIDevice.h` (~160줄 인터페이스), `NullDevice.cpp` (검증기) |
| Tools | 1,400 | `Commands_Doc.cpp`, `Commands_Info.cpp` |
| Runtime | 공개 헤더 6개 + ECS | `ECS/World.cpp`, `ECS/ComponentRegistry.cpp`, `ECS/NativePool.cpp` |
| Editor (Tools) | 문서 모델 + Win32/GDI | `Tools/AliceEditor/` (기존 Doc/Schema/Verbs 소비, 엔진 공개 헤더 변경 없음) |
| Tests | 2,500 | 실행 테스트 Windows 173개 / 기타 171개 + Runtime 컴파일 계약 + Windows 창 smoke |

**이 엔진을 이해하려면 두 파일을 읽으면 됩니다.**

- `Engine/Schema/CoreSchemas.cpp` — 만들 수 있는 것의 전부
- `Engine/Verbs/CoreVerbs.cpp` — 할 수 있는 것의 전부

---

## 설계 결정 기록

큰 결정은 `Docs/adr/` 에 남깁니다. 형식:

```markdown
# ADR-0001 · 제목

상태: 채택 | 대체됨(ADR-XXXX) | 폐기
날짜: YYYY-MM-DD
Agent: <역할>

## 맥락
## 결정
## 대안과 버린 이유
## 결과 (좋은 것과 나쁜 것 모두)
```

**"결과" 에 나쁜 것도 적습니다.** 대가 없는 결정은 없습니다.
