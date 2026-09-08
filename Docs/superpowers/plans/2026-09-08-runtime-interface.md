# ALI-02 Runtime Interface Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to execute the steps in this session.

**Goal:** Sidney와 Monday가 같은 계약으로 구현을 시작할 수 있는 C++20 공개 헤더를 제공한다.

**Architecture:** World는 저장소 구현을 숨기고 읽기 가드와 명시적 변경 API를 제공한다.
컴포넌트 등록은 스키마와 네이티브 타입의 변환을 연결한다. 렌더 추출 결과는 자체 데이터를
소유하고, 규칙은 읽기 단계와 순서가 고정된 쓰기 단계를 분리한다.

**Tech Stack:** C++20, CMake 3.24+, 기존 Foundation/Doc/Schema/Verbs. 새 의존성 없음.

**Spec:** [설계 및 이유](../../ARCHITECTURE.md#runtime-contract).

## Constraints

- Runtime 구현 `.cpp`는 추가하지 않는다. 선언과 기본 값/비교 및 타입 변환 어댑터만 제공한다.
- 미디어 애셋은 추가하지 않는다.
- 공개 헤더는 단독으로 경고 0 컴파일되어야 한다.
- 읽기 가드 수명 동안 World와 컴포넌트 포인터가 유효해야 한다.
- 렌더 결과는 World를 참조하지 않는다. RHI는 Runtime을 의존하지 않는다.

## 1. 공개 계약과 컴파일 검증

Files: `Engine/Runtime/{RuntimeTypes,ComponentRegistry,World,RenderView,RenderExtraction,Rules}.h`,
`Engine/CMakeLists.txt`, `CMake/RuntimeContracts.cpp.in`, `Docs/ARCHITECTURE.md`.

- [x] CMake에서 각 공개 헤더를 하나씩 include하는 검사 소스를 빌드 디렉터리에 생성한다.
  `add_library(Alice.Runtime.Contracts OBJECT ...)`로 기본 빌드에 포함한다.
- [x] 소비자 검사를 먼저 작성하고 `Scripts/build.ps1 -WarningsAsErrors`로 헤더 부재 실패를 확인한다.
  읽기 접근은 다음 타입 계약을 검사한다:
  ```cpp
  static_assert(std::is_same_v<decltype(std::declval<const WorldReadView&>().Get<int>(
      EntityId{}, ComponentId{})), Result<const int*>>);
  static_assert(!std::is_copy_constructible_v<WorldReadView>);
  ```
- [x] 각 헤더에 식별자 수명, 등록 콜백 소유권, 읽기 가드, 변경 시점, 렌더 추출,
  규칙 평가/명령 실행 경로를 선언한다. 렌더 패킷은 `std::vector`와 `std::string`으로 소유한다.
- [x] 아키텍처 문서에 실패 정책, 명령 정렬/충돌 규칙, 대안과 비용을 기록한다.
- [x] 빌드 성공 후 읽기 포인터를 mutable로 바꾸는 결함과 RenderView에 World를 include하는
  결함을 각각 주입한다. 전자는 컴파일 실패, 후자는 CMake 설정 실패를 확인하고 되돌린다.

## 2. 전달 및 검증

Files: `Docs/STATUS.md`, `Agents/Backlog/alice/ALI-02.md`, `Agents/README.md`, 위키 생성물.

- [x] `Scripts/verify.ps1 -Json`의 6단계 성공을 확인한다.
- [x] `Wiki/`에서 `npm test`, `npm run build`를 실행한다.
- [x] 독립 코드 리뷰에서 컴파일 계약과 후속 구현의 모호한 지점을 점검하고 해결한다.
- [x] 백로그에 실제 검증 범위와 미구현 범위를 기록한다.
- [ ] 검증된 변경을 커밋하고 `alice/ALI-02-runtime-interface`에 푸시한다.
  PR은 기존 기반 브랜치 `chrono/CHR-10-local-wiki`를 대상으로 열어 ALI-02만 비교한다.
