# SID-05 · World → 문서 덤프 (왕복 닫기)

```
상태: 대기
크기: M
선행: SID-03
담당: -
```

## 무엇을

살아 있는 World 를 `scene` 문서로 되쓴다.

```
scene.yaml → World → scene.yaml   (두 문서가 의미상 같아야 한다)
```

## 왜

**이 작업이 이 엔진의 핵심 주장을 완성한다.**

언리얼/유니티에서 AI 가 콘텐츠를 못 다루는 진짜 이유는 "현재 상태를 되받아 읽을 수 없어서"다.
상태가 바이너리와 C++ 객체 안에 있으면 AI 는 자기가 뭘 바꿨는지 확인할 방법이 없다.

이게 되면 AI 는 이렇게 일할 수 있다:
```
alice dump --scene > current.yaml    # 지금 상태를 읽고
# ... 고치고 ...
alice apply current.yaml             # 되돌려 넣고
alice check current.yaml             # 확인한다
```

## 완료 기준

- [ ] World 를 `alice/scene/1` 문서로 직렬화한다
- [ ] 왕복 테스트: 로딩 → 덤프 → 재로딩이 같은 World 를 만든다
- [ ] 런타임에 생긴 액터(actor.spawn)도 덤프에 포함된다
- [ ] `alice dump` 명령이 붙는다 (Chrono 와 협의)

## 건드리는 파일

```
Engine/Runtime/Scene/SceneDump.*
Engine/Tests/
```

## 건드리면 안 되는 것

문서 직렬화기 — Doc 모듈이 한다
