# CHR-12 · 새 씬·분할 터미널·큐브 입력 예제

```
상태: 진행중
크기: L
선행: CHR-11
담당: Codex (Chrono)
```

## 요구와 완료 기준

사용자는 새 씬 버튼, 왼쪽 편집기/오른쪽 AI용 터미널, 마우스 크기 조절,
플레이어 큐브 입력·Transform 예제와 이후에도 예제로 보여주는 개발 방식을 요청했다.

- [ ] New scene으로 기존 파일을 덮어쓰지 않고 유효한 씬 생성
- [ ] 오른쪽 실제 PowerShell 터미널, 키보드 입력·출력·리사이즈·종료 처리
- [ ] 편집기/터미널 및 내부 패널 경계 마우스 드래그 조절
- [ ] CubePlayground 씬, Play/Stop, 이동 입력 및 위치·회전·크기 편집 검증
- [ ] 실행 중 상태는 ECS에 격리하고 Stop 시 편집 원본 복원
- [ ] 예제 기반 개발 규칙, README/위키/샘플 설명과 권리 기록
- [ ] 모델·터미널·UI 회귀, 실제 창 확인, 공식 verify와 CI, 커밋·푸시·PR

## 경계

Tools/AliceEditor/*, Engine/Tests/*, Samples/CubePlayground/*,
Scripts/editor.ps1, 관련 CMake·문서·백로그·위키.
기존 엔진 모듈 공개 헤더와 ECS/RHI 내부는 변경하지 않는다.
터미널은 Windows ConPTY를 사용한다. 예제 플레이는 Tools의 명시적 미리보기 컨트롤러이며
일반 behavior 실행기나 메시 렌더러를 구현했다고 주장하지 않는다. 외부 의존성·미디어 없음.
