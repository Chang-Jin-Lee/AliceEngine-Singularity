# ALI-01 · 모듈 경계와 의존성 그래프

```
상태: 완료
크기: M
선행: 없음
담당: -
```

## 무엇을

`Foundation → Doc → Schema → Verbs` 와 `Foundation → RHI` 의 단방향 그래프를 세우고,
CMake 타깃으로 **강제**한다. 문서로만 적힌 규칙은 반드시 썩기 때문이다.

## 왜

참고한 엔진 대부분이 "레이어"를 문서로만 정의해 두었고, 실제로는 아래층이 위층을
포함하고 있었다. 그러면 어느 모듈만 떼어 쓸 수도, 테스트할 수도 없다.
여기서는 `Alice.Doc` 이 `Alice.RHI` 를 참조하면 **설정 단계에서** 깨진다.

## 완료 기준

- [x] `alice_module()` 이 include 경로를 소스 루트 하나로 고정한다
- [x] 상대 경로(`../../`) include 가 불가능하다
- [x] `Engine/CMakeLists.txt` 를 읽으면 의존성 그래프가 보인다
- [x] 각 모듈이 단독으로 빌드된다

## 건드리는 파일

```
CMake/AliceModule.cmake
Engine/CMakeLists.txt
```

## 건드리면 안 되는 것

모듈 내부 구현
