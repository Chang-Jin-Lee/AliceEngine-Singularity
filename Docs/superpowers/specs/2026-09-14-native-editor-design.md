# Windows scene editor — CHR-11

사용자의 우선순위는 실행하면 독립 창이 뜨는 에디터다. 현재 Windows 개발 환경과 코어
서드파티 0 원칙을 따라 Win32 컨트롤과 GDI로 첫 편집기를 구현한다. 웹 앱은 별도 런타임이
필요하고 범용 GPU UI는 아직 없는 렌더러에 의존하므로 이번 단계에서는 선택하지 않는다.

Tools/AliceEditor의 문서 모델은 Doc/Schema/Verbs를 소비하며 Windows UI와 분리한다.
기존 엔진 공개 헤더는 변경하지 않는다. 문서가 유일한 편집 원본이며 Runtime scene loader를
대체하지 않는다. 뷰포트는 실제 transform을 회전·투영한 배치용 도형이고 미디어를 포함하지 않는다.

창 구성: 위 도구 모음, 왼쪽 hierarchy/project, 가운데 scene/document, 오른쪽 inspector,
아래 console. 액터 선택·추가·삭제, 이름·position/rotation/scale 편집, 원문 편집, 검증,
저장, undo/redo를 제공한다. 실제 게임 실행 버튼은 구현할 때 추가한다.

저장 전 문서 전체를 검증한다. 파싱/스키마/동사 진단의 코드·file·mark·hint를 표시한다.
외부 파일 내용이 로드 시점과 다르면 덮어쓰지 않는다. 같은 디렉터리의 임시 파일을 원자적으로
교체하며 실패하면 원본과 편집 상태를 유지한다. 다른 파일 열기/창 닫기에는 미저장 확인을 한다.
Undo는 메모리 원문 스냅샷 128개로 제한한다. 큰 문서는 복사 비용이 있다.

검증: 플랫폼 독립 모델 테스트, 결함 주입 후 실패 확인, Windows 실제 HWND/메시지 루프
smoke 검사, 창 캡처를 통한 레이아웃 확인, 공식 verify와 세 플랫폼 CI, 위키 빌드.
Windows만 GUI를 빌드한다. macOS/Linux는 문서 모델 테스트와 기존 CLI를 유지한다.
