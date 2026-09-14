# SID-02 · ECS World — 컴포넌트 저장소

```
상태: 진행중
크기: L
선행: ALI-02
담당: Codex (Sidney)
```

## 무엇을

SparseSet 기반 컴포넌트 저장소와 엔티티 관리(SlotMap + 세대).

- `EntityId` = 인덱스 + 세대. 파괴된 엔티티의 id 를 재사용해도 즉시 잡힌다
- 컴포넌트는 타입별로 연속 배열에 모인다
- 질의: 컴포넌트 조합으로 순회

## 왜

베이스 엔진(AliceEngine-Optimization)에서 ECS 와 GameObject-Component 를 같은 워크로드로
실측한 결과가 있다. N=100 에서도 이미 약 3배 차이가 났고, 그건 캐시가 아니라
간접 참조와 가상 호출 비용이었다. N=50000 파편화 상태에서는 8배 가까이 벌어졌다.

→ `AliceEngine-Optimization/Docs/ECS_VS_OOP.md`

그 측정이 이 선택의 근거다. 추측이 아니다.

## 완료 기준

- [x] `World` 가 엔티티를 만들고 파괴한다
- [x] 파괴된 엔티티의 옛 핸들로 접근하면 잡힌다
- [x] 컴포넌트가 타입별 연속 메모리에 저장된다
- [x] 다중 컴포넌트 질의가 동작한다
- [x] 지연 파괴(프레임 경계에서 커밋)가 있다
- [x] 테스트: 수명, 세대 검사, 질의 정확성

## 건드리는 파일

```
Engine/Runtime/ECS/*
Engine/CMakeLists.txt
Engine/Tests/CMakeLists.txt
Engine/Tests/Runtime_ECS_Tests.cpp
README.md · AGENTS.md · Docs/STATUS.md · Docs/ARCHITECTURE.md (현황)
Docs/superpowers/plans/2026-09-09-ecs-world.md
Wiki/app/page.js (현황) · Wiki 생성 문서
```

## 구현과 검증

2026-09-14: 공개 헤더를 유지한 채 `Alice.Runtime` 정적 라이브러리에 구현을 연결했다.
읽기 가드는 저장소를 공유 소유하며, 타입별 풀은 정렬된 연속 메모리와 swap erase를 사용한다.
코덱은 임시 객체에 decode한 뒤 교체하므로 스키마·변환 실패가 기존 값을 바꾸지 않는다.

- 공식 verify 6단계, 전체 148개 테스트 통과 (RuntimeECS 11개)
- 세대 검사·읽기 잠금·진단 힌트/위치 제거 변형이 각각 테스트 실패로 검출됨
- 독립 리뷰에서 발견한 진단 200개 제한 문제는 재현 실패를 확인하고 수정함.
  경고 200개 뒤의 잘못된 값도 검사하고 decode 호출을 차단한다.
- 64바이트 정렬과 비자명 타입 수명, World 파괴 후 읽기 가드 수명, 실패 원자성 검증
- 외부 의존성·미디어 애셋 추가 없음. 씬 로딩·렌더 추출·규칙 실행은 후속 작업

## 건드리면 안 되는 것

렌더 데이터 추출 — 인터페이스는 ALI-02 가 정한다
