# ALI-02 · Runtime 모듈 인터페이스 설계

```
상태: 리뷰
크기: M
선행: 없음
담당: Codex (Alice)
```

## 무엇을

`Engine/Runtime/` 의 공개 인터페이스를 정한다. Sidney 가 구현하기 전에 경계가 있어야 한다.

정해야 할 것:
- `World` 가 밖에 보여주는 것 (엔티티 생성/파괴, 컴포넌트 접근, 질의)
- 컴포넌트 타입을 등록하는 방법 — 스키마와 어떻게 연결되는가
- 렌더러가 World 에서 그릴 것을 뽑아가는 경로 (RHI 는 ECS 를 알면 안 된다)
- 규칙 평가기가 World 를 읽고 쓰는 경로

## 왜

지금 Runtime 이 비어 있어서 Sidney 와 Monday 가 동시에 시작할 수 없다.
경계가 먼저 있어야 둘이 병렬로 간다.

특히 **렌더러가 ECS 를 직접 순회하게 두면 안 된다.** 그 순간 두 모듈이 붙어버리고,
헤드리스 테스트도 렌더 백엔드 교체도 불가능해진다.

## 완료 기준

- [x] `Engine/Runtime/World.h` 등 공개 헤더가 존재하고 컴파일된다
- [x] 렌더러가 쓸 추출 인터페이스(`RenderView` 같은 것)가 정의되어 있다
- [x] `Docs/ARCHITECTURE.md` 에 이유와 대안이 기록된다
- [x] 구현은 없어도 된다 — 인터페이스와 문서가 산출물이다

## 건드리는 파일

```
Engine/Runtime/*.h
Engine/CMakeLists.txt
Docs/ARCHITECTURE.md
CMake/RuntimeContracts.cpp.in (링크하지 않는 소비자 컴파일 검사)
Docs/STATUS.md, Docs/superpowers/plans/2026-09-08-runtime-interface.md
README.md, AGENTS.md (실행 상태·저장소 지형 갱신)
Wiki/ 생성물 (설계·상태 문서 동기화)
```

## 건드리면 안 되는 것

구현(.cpp) — Sidney 의 몫이다

## 작업 기록 · 2026-09-08

공개 헤더 6개를 작성했다. World 소속과 세대를 가진 엔티티 ID, 동결한 컴포넌트
레지스트리와 문서 코덱, 이동 전용 읽기 가드, 소유하는 렌더 패킷, 조건 평가와
정렬한 명령 적용 경로를 정의했다. ECS 저장소 및 실제 런타임 함수 정의는 추가하지 않았다.

- 각 헤더 단독 컴파일 + 소비자 컴파일 7개 번역 단위: MSVC /W4 /WX 통과
- 결함 주입: 수정 가능한 읽기 포인터는 정적 어설션 실패로 검출
- 결함 주입: RenderView의 World include는 CMake 설정 단계에서 검출
- 결함 원복 후 공식 verify 6단계 통과, 실행 테스트 137개 유지
- 테스트·CLI를 모두 OFF로 설정한 별도 구성에서도 공개 계약 검사 빌드 통과
- 독립 리뷰: 스키마 검증 접근 누락을 소비자 컴파일 실패로 재현한 뒤 Validate API로 해결
  (재검토에서 미해결 지적 없음). 오류 반환 기준과 스팟 원뿔 전체 각도도 명시
- 위키 테스트 2개 및 정적 빌드 통과
- 새 서드파티 패키지와 미디어 애셋 추가 없음

후속 구현은 `Docs/ARCHITECTURE.md`의 수명·실패·정렬 계약을 기준으로 SID-02부터 진행한다.
World 호출이 링크되거나 게임 창이 실행된다는 의미는 아니다.

GitHub [PR #1](https://github.com/Chang-Jin-Lee/AliceEngine-Singularity/pull/1)을 열었다.
기준 브랜치는 `chrono/CHR-10-local-wiki`이며 이 작업의 변경만 비교한다.
병합 전이므로 상태는 `리뷰`로 유지한다.
