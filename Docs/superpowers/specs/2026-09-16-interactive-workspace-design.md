# CHR-12 · Interactive workspace

사용자는 새 씬 생성, 왼쪽 편집기/오른쪽 AI용 터미널, 마우스로 크기를 조절하는 영역,
플레이어 큐브의 입력과 Transform을 시험할 예제 씬을 요청했다.

- New scene은 저장 대화상자로 YAML 파일을 새로 생성하며 기존 파일은 덮어쓰지 않는다.
- Windows ConPTY와 PowerShell을 오른쪽 패널에 연결한다. 키 입력/출력/리사이즈를 실제
  프로세스에 전달하고 종료 시 자식 프로세스와 파이프를 정리한다. 외부 패키지 없음.
- 전체 편집기/터미널, hierarchy/center/inspector, hierarchy/project, console 경계를
  드래그로 조절한다. 최소 크기를 두어 조작 가능한 영역을 유지한다.
- CubePlayground의 유효한 project/scene/input YAML을 제공한다. 기본 도형만 사용한다.
  플레이어는 player 태그로 지정한다. Play에서 변환을 ECS World로 복사하고 입력을 적용한다.
  Stop하면 편집 문서로 복귀하며 실행 중 값은 저장하지 않는다. Inspector는 실행 중 ECS의
  변환을 조절할 수 있다. 입력은 뷰포트에 포커스가 있을 때만 받는다.
- 이 실행은 에디터의 큐브 미리보기 컨트롤러다. 일반 behavior 평가기·물리·GPU 렌더러와
  구분한다. 기존 Runtime/Schema/RHI 공개 헤더는 변경하지 않는다.
- 앞으로 기능 변경은 예제 씬, 실행 방법, 자동 검증과 실제 화면 확인을 함께 제공한다.

검증: 새 파일 배타적 생성, 분할 최소 크기, Play 입력/시간/Stop 격리, 터미널 VT 파싱,
실제 ConPTY 왕복과 실제 창 smoke 테스트. 공식 verify, 샘플 fmt/check, 위키, CI.
