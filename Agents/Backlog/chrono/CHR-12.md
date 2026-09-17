# CHR-12 · 새 씬·분할 터미널·큐브 입력 예제

```
상태: 리뷰
크기: L
선행: CHR-11
담당: Codex (Chrono)
```

PR: [#6](https://github.com/Chang-Jin-Lee/AliceEngine-Singularity/pull/6), base `main`.

## 요구와 완료 기준

사용자는 새 씬 버튼, 왼쪽 편집기/오른쪽 AI용 터미널, 마우스 크기 조절,
플레이어 큐브 입력·Transform 예제와 이후에도 예제로 보여주는 개발 방식을 요청했다.

- [x] New scene으로 기존 파일을 덮어쓰지 않고 유효한 씬 생성
- [x] 오른쪽 실제 PowerShell 터미널, 키보드 입력·출력·리사이즈·종료 처리
- [x] 편집기/터미널 및 내부 패널 경계 마우스 드래그 조절
- [x] CubePlayground 씬, Play/Stop, 이동 입력 및 위치·회전·크기 편집 검증
- [x] 실행 중 상태는 ECS에 격리하고 Stop 시 편집 원본 복원
- [x] 예제 기반 개발 규칙, README/위키/샘플 설명과 권리 기록
- [ ] 모델·터미널·UI 회귀, 실제 창 확인, 공식 verify와 CI, 커밋·푸시·PR

## 경계

Tools/AliceEditor/*, Engine/Tests/*, Samples/CubePlayground/*,
Scripts/editor.ps1, 관련 CMake·문서·백로그·위키.
기존 엔진 모듈 공개 헤더와 ECS/RHI 내부는 변경하지 않는다.
터미널은 Windows ConPTY를 사용한다. 예제 플레이는 Tools의 명시적 미리보기 컨트롤러이며
일반 behavior 실행기나 메시 렌더러를 구현했다고 주장하지 않는다. 외부 의존성·미디어 없음.

## 검증 기록

2026-09-17: 공식 verify 6단계, Windows 테스트 173개, 실제 창 smoke 20항목,
위키 테스트 2개와 정적 빌드 68페이지를 통과했다. CubePlayground 3문서는 오류·경고 0이다.
실제 창에서 한글 출력과 `alice check . --json` 왕복을 확인했다.
신규 씬 JSON 문법, 실패 시 미적용 입력 보존, 잘못된 Unicode 진단, 액터 위치 진단을
리뷰로 찾아 수정했다. 레이아웃 최소 폭·입력 시간 제한·진단 힌트·UTF-8 처리·새 씬 실패 후
입력 초기화 변형은 각각 회귀 테스트 실패를 확인한 뒤 복원했다.
최종 독립 리뷰에서 추가 중대 결함은 발견되지 않았다. 실제 AI CLI별 전체 화면 UI와
실제 IME 조합 이벤트 호환성은 별도 수동 확인 범위로 남긴다.

첫 CI의 Windows·위키·개발 규칙은 통과했다. GCC가 지적한 테스트 루프의 불필요한 복사와
Clang이 지적한 테스트 임시 액터 목록의 수명을 수정해 재검증한다.
