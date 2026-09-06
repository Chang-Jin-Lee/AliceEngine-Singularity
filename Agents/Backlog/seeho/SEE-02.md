# SEE-02 · CI 파이프라인

```
상태: 대기
크기: M
선행: 없음
담당: -
```

## 무엇을

GitHub Actions. Windows / Linux / macOS 세 플랫폼.

## 왜

"세 플랫폼에서 경고 0"은 CI 가 없으면 지켜지지 않는다.
특히 MSVC 에서만 빌드하면 `/permissive-` 를 켜도 GCC/Clang 에서 터지는 것이 남는다.

## 완료 기준

- [ ] 세 플랫폼 빌드 (경고를 에러로)
- [ ] 테스트 실행
- [ ] `alice check Samples --json` 오류 0
- [ ] `alice fmt Samples --check` 통과
- [ ] PR 에 Agent 가 적혀 있는지 검사
- [ ] 빌드 시간 5분 이내

## 건드리는 파일

```
.github/workflows/ci.yml
```

## 건드리면 안 되는 것

엔진 코드
