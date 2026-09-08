# ALI-05 · 플랫폼별 실수 정규화 차이 제거

```
상태: 진행중
크기: S
선행: 없음
담당: Codex (Alice)
```

## 무엇을

Foundation의 FormatDouble이 모든 플랫폼에서 같은 짧은 왕복 표현을 출력하도록 한다.

## 왜

ALI-02 PR의 CI run 34201219891에서 세 플랫폼 모두 빌드/테스트는 통과했지만
Linux·macOS의 `alice fmt Samples --check`가 8개 문서 차이로 실패했다.
기존 FormatDouble은 MSVC에서 to_chars, 나머지에서 snprintf %.17g를 사용한다.
예를 들어 0.35를 서로 다른 길이로 써서 같은 샘플이 플랫폼마다 수정된다.
Runtime 계약 PR과 섞지 않고 Foundation 수정으로 분리한다.

## 완료 기준

- [ ] 기존 비-MSVC 출력 경로로 회귀 테스트와 샘플 정규화 실패를 재현한다
- [ ] 짧은 소수, 부호 있는 0, 큰 값과 작은 값의 왕복·타입 보존을 검증한다
- [ ] 공식 verify 6단계와 위키 생성물 검사가 통과한다
- [ ] GitHub Windows·Linux·macOS 및 위키 CI가 통과한다

## 건드리는 파일

`Engine/Foundation/StringUtil.cpp`, `Engine/Foundation/StringUtil.h`,
`Engine/Tests/Foundation_Tests.cpp`, `Agents/README.md`, `Docs/STATUS.md`, 위키 생성물.

## 건드리면 안 되는 것

샘플 값을 바꿔 검사에 맞추거나 fmt 검사를 끄지 않는다. 실수 파싱 정책과 다른 모듈 구현은 제외한다.

## 접근과 대안

C++ 표준 라이브러리의 부동소수 to_chars를 모든 플랫폼에서 사용한다.
17자리 표현으로 샘플을 전부 다시 쓰는 방법은 읽기 쉬운 소수를 잃으므로 배제한다.
자체 부동소수 변환기나 외부 라이브러리는 이 문제에 불필요하다.
지원 표준 라이브러리에 부동소수 to_chars가 있어야 한다는 요구는 명시한다.
