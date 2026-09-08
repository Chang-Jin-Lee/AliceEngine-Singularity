# ALI-02 · Runtime 모듈 인터페이스 설계

```
상태: 진행중
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

- [ ] `Engine/Runtime/World.h` 등 공개 헤더가 존재하고 컴파일된다
- [ ] 렌더러가 쓸 추출 인터페이스(`RenderView` 같은 것)가 정의되어 있다
- [ ] `Docs/ARCHITECTURE.md` 에 이유와 대안이 기록된다
- [ ] 구현은 없어도 된다 — 인터페이스와 문서가 산출물이다

## 건드리는 파일

```
Engine/Runtime/*.h
Engine/CMakeLists.txt
Docs/ARCHITECTURE.md
```

## 건드리면 안 되는 것

구현(.cpp) — Sidney 의 몫이다
