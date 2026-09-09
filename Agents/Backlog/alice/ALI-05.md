# ALI-05 · 플랫폼별 실수 정규화 차이 제거

```
상태: 리뷰
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

- [x] 기존 비-MSVC 출력 경로로 회귀 테스트와 샘플 정규화 실패를 재현한다
- [x] 짧은 소수, 부호 있는 0, 큰 값과 작은 값의 왕복·타입 보존을 검증한다
- [x] 공식 verify 6단계와 위키 생성물 검사가 통과한다
- [x] GitHub Windows·Linux·macOS 및 위키 CI가 통과한다

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

## 재현과 로컬 검증 · 2026-09-08

기존 snprintf 경로를 Windows에서도 임시 활성화하여 새 회귀 검사를 실행했다.
0.1 → 0.10000000000000001, 0.35 → 0.34999999999999998 등 4개 기대값이 실패했고,
샘플 정규화는 CI와 동일한 8개 파일에서 실패했다. 샘플은 수정하지 않았다.
소스 원복 후 출력 경로를 통일하자 해당 회귀 검사와 공식 verify 6단계가 통과했다.
기존 NumberRoundTrip 검사를 보강하여 테스트 수는 137개로 유지했다.
위키 레퍼런스 재생성 후 변경 없음, 위키 테스트 2개 통과. 독립 리뷰에서 지적 없음.

GitHub [PR #2](https://github.com/Chang-Jin-Lee/AliceEngine-Singularity/pull/2),
[CI run 34228935631](https://github.com/Chang-Jin-Lee/AliceEngine-Singularity/actions/runs/34228935631):
Windows·Ubuntu·macOS, 위키, 개발 규칙 5개 작업 전부 통과했다 (`80710f4`).
ALI-02를 기준으로 한 별도 수정 PR이며 아직 병합하지 않았다.
