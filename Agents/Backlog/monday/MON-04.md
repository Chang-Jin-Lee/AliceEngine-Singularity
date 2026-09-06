# MON-04 · Vulkan 백엔드

```
상태: 대기
크기: L
선행: MON-02
담당: -
```

## 무엇을

Vulkan 구현. Windows / Linux / Android 공용.

## 왜

Android 와 Linux 의 유일한 경로다. macOS/iOS 는 MoltenVK 로 대체 가능하지만
Metal 백엔드(MON-05)가 있으면 그쪽이 낫다.

이 환경에는 Vulkan SDK 1.4.357.0 이 설치되어 있다.

## 완료 기준

- [ ] 인스턴스·디바이스·스왑체인
- [ ] VMA 또는 자체 할당자 (도입 시 Alice 승인)
- [ ] 디스크립터 인덱싱(바인드리스)
- [ ] 검증 레이어를 켰을 때 경고 0
- [ ] 타임스탬프 쿼리
- [ ] MON-02 와 같은 화면

## 건드리는 파일

```
Engine/RHI/Vulkan/*
```

## 건드리면 안 되는 것

RHI 공개 인터페이스
