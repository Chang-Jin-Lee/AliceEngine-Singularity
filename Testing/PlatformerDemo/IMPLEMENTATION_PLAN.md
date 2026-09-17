# Platformer Physics Implementation Plan

**Goal:** 실제 Play에서 도형 캐릭터가 중력·점프·충돌로 움직이고 C++ 물리 제공자를 교체할 수 있게 한다.
**Architecture:** Runtime 순수 물리 코어와 문서/Play 어댑터를 분리한다. 캐릭터 전용 설정은 선택 컴포넌트로 추가하여 기존 큐브 미리보기를 보존한다.
**Tech Stack:** C++20, 기존 CMake/MSVC, 서드파티 없음.
**Spec:** PHYSICS_DESIGN.md (사용자 승인 2026-09-17)

## Constraints and decisions
- XY, Y 위, 고정 1/60초, 비회전 사각형과 정적 지형.
- 문서·벤치마크·예시는 Testing 내부. 백로그 관리 파일과 생성물은 저장소 검증 계약 때문에 원래 위치 갱신.
- 기존 워크스페이스의 조사 파일 보존을 위해 별도 브랜치에서 작업. 추가 worktree는 만들지 않는다.
- 신규 character2d 컴포넌트가 물리 모드 선택. speed=4, jumpSpeed=7, gravityScale=1. 기존 collider/scene gravity 재사용.
- 움직이는 캐릭터는 하나, 지형은 다수. 일반 rigidbody·trigger·layer·hierarchy 등 미지원 설정은 진단한다.

## Tasks
- [x] 1. Runtime/Physics/Platformer.h/.cpp와 Physics_Tests.cpp: 공개 계약, 고정 지형 인덱스, swept collision, 사용자 IMotionSolver 교체. 테스트를 먼저 실패시키고 구현한다. `Alice.Tests --filter=Physics`로 검증한다.
- [x] 2. CoreSchemas.cpp character2d 및 Editor PlatformerSession 어댑터: 문서 검증과 위치/힌트, 고정 스텝 누적/점프 edge 보존, ECS Transform 반영. Editor_Play_Tests.cpp에서 기존 실패를 먼저 확인한다.
- [x] 3. Testing/PlatformerDemo: 실행 씬, 사용자 제공자 예제, 벤치마크. 실제 창 smoke에서 Play 시작·낙하·착지·점프·Stop 검사. 기존 CubePlayground smoke 보존.
- [x] 4. 코어/통합 리뷰 후 지적 수정. 의도적 충돌 처리 무력화로 회귀 테스트 실패 확인 후 복구. `Scripts/verify.ps1 -BuildDir Testing/build`, Testing check/fmt, 벤치마크 실행.
- [ ] 5. 생성 스키마·위키 레퍼런스 갱신, 결과/제한 문서화, 커밋·푸시·PR 생성·CI 확인·머지.

## Shared interface
Runtime core contract is recorded in Runtime/Physics/Platformer.h before adapter coding. Provider input is bounds plus displacement; output is resolved bounds, blocked axes, grounded and query counters. Provider is stateless with respect to character state; caller owns serializable position/velocity. Static world is initialized atomically. Editor holds one Character state and calls the shared fixed-step helper.

## Validation examples
```cpp
// Gravity removal must fail landing; bypassing sweep must fail thin-floor test.
// Replacing indexed queries with all colliders must preserve motion results.
// Tick(1/120, Space), Tick(1/120, no keys) must still produce one jump.
```

Progress and exact commands/results are recorded in VERIFICATION.md after execution.

## Review rulings
- Core and adapter interfaces checked together: Box/Vec2/IMotionSolver output matches consumer; gravity remains input-policy adapter.
- All 5 tasks follow no-dependency and Testing artifact constraints; CMake/schema generated files stay at required repository paths.
- Review findings: tangent corner, terrain source provenance, transactional reload fixed with failing regression tests then passing.
- Coordinate limit tightened to 1e6 to preserve meaningful 1e-8 contact tolerance.
- Native provider replacement is demonstrated through the runtime API; YAML provider registry/DSL remain outside scope.
