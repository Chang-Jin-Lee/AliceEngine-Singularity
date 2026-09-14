# ALI-04 · 플랫폼 추상화 경계

```
상태: 대기
크기: M
선행: 없음
담당: -
```

## 무엇을

창, 입력, 시간, 파일시스템의 플랫폼 경계를 정한다.
Windows / macOS / Linux / iOS / Android / 콘솔이 같은 인터페이스 뒤에 있어야 한다.

## 왜

요구사항이 "쉽고 간단하게 전 플랫폼 지원"이다. 그게 되려면 플랫폼 코드가
**정확히 한 곳에만** 있어야 한다. 지금은 `Foundation/FileSystem.cpp` 와
`Log.cpp` 에 `#if ALICE_PLATFORM_WINDOWS` 가 흩어져 있다 — 두 곳이지만
벌써 흩어지기 시작한 것이다.

## 완료 기준

- [ ] `Engine/Platform/` 인터페이스가 정의된다
- [ ] 모바일에 없는 것(창 리사이즈 등)이 Capabilities 로 표현된다
- [ ] Foundation 의 플랫폼 분기가 Platform 으로 옮겨진다
- [ ] Null 플랫폼(헤드리스)이 존재한다

## 건드리는 파일

```
Engine/Platform/*.h
Engine/Foundation/*
```

## 건드리면 안 되는 것

각 플랫폼 구현
