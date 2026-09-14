# RHI/ 에서 일할 때

> 루트의 [`AGENTS.md`](../../AGENTS.md) 와 [`Agents/Monday.md`](../../Agents/Monday.md) 를 먼저 읽으십시오.

## 인터페이스를 작게 유지한다

`RHIDevice.h` 는 ~160줄입니다. WickedEngine 은 478줄로 DX12·Vulkan·Metal 을 전부 굴립니다.
**짧아야 새 백엔드가 붙습니다.**

인터페이스 밖으로 밀어낸 것과 그 이유:

| 밖으로 뺀 것 | 어디로 | 왜 |
|---|---|---|
| 배리어 · 리소스 상태 전이 | RenderGraph (MON-06) | 사람도 AI 도 정확히 못 쓴다 |
| 디스크립터 힙 관리 | 백엔드 내부 | 바인드리스면 인덱스만 보이면 된다 |
| 메모리 할당자 | 백엔드 내부 | VMA/D3D12MA 는 백엔드 사정이다 |
| 셰이더 컴파일 | 애셋 파이프라인 | 런타임은 바이트코드만 받는다 |

이 넷이 다른 엔진에서 이식을 어렵게 만드는 주범입니다. **다시 끌어들이지 마십시오.**

## Null 백엔드가 먼저다

`NullDevice.cpp` 는 검증기를 겸합니다. 진짜 백엔드보다 엄격합니다.

새 코드를 쓰면 Null 에서 먼저 돌려보십시오. `rhi.cmd.*` 위반 로그가 나오면
D3D12/Vulkan 에서도 반드시 문제가 됩니다 — 다만 거기서는 조용히 깨지거나
드라이버가 죽을 뿐입니다.

Null 이 잡는 것: 렌더패스 밖 Draw, 파이프라인 없는 Draw, 불균형 디버그 그룹/GPU 존,
128바이트 초과 푸시 상수, 잘못된 인덱스 포맷, 해제된 핸들 사용, 종료 시 리소스 누수.

## 새 백엔드를 추가하려면

1. `Engine/RHI/<이름>/` 에 `IDevice` 와 `ICommandList` 구현
2. 정적 초기화에서 `RegisterBackend()` 호출
3. `ForceLink<이름>Backend()` 훅 제공 —
   **없으면 정적 라이브러리에서 링커가 통째로 버려 백엔드가 조용히 사라집니다**
4. `Engine/RHI/CMakeLists.txt` 에 조건부 추가
5. `RHIBackend.cpp` 의 `LinkAllBackends()` 와 `PreferredBackendOrder()` 갱신

## 반드시

- **모든 리소스에 `debugName` 을 주십시오.** 이름 없는 리소스는 프로파일러에서도,
  누수 보고에서도, RenderDoc 에서도 추적할 수 없습니다
- 백엔드 생성 실패는 **이유와 함께** 로그로 남기십시오.
  `rhi.backend.rejected { backend, reason }` — 이 한 줄이 "왜 Vulkan 으로 떨어졌지?" 의 답입니다
- 푸시 상수 128바이트를 넘지 마십시오. Vulkan 이 보장하는 최소값입니다
- GPU 타임스탬프를 `Profiler::SubmitGpuZone()` 으로 되먹이십시오
