# CHR-11 · 실행 가능한 Windows 씬 에디터

```
상태: 진행중
크기: L
선행: CHR-10, SID-02
담당: Codex (Chrono)
```

## 무엇을 / 왜

사용자가 CLI를 넘어 유니티처럼 창이 뜨는 에디터를 우선 요청했다.
Windows 네이티브 창에서 기존 콘텐츠 문서를 열고 씬을 보고 수정·검증·저장한다.

## 완료 기준

- [ ] Alice.Editor.exe를 실행하면 독립 에디터 창이 열린다
- [ ] 프로젝트 문서 목록, 씬 계층, 속성, 뷰포트, 콘솔이 실제 문서와 연결된다
- [ ] 액터 이름·변환 편집, 추가·삭제, Undo/Redo, 문서 원문 편집이 가능하다
- [ ] 스키마·동사 검증의 위치와 힌트를 보여주며 잘못된 문서 저장을 막는다
- [ ] 미저장 변경 확인, 외부 변경 충돌 검사, 안전한 파일 교체를 제공한다
- [ ] 문서 모델 회귀 테스트와 실제 Windows 창 smoke 검증을 통과한다
- [ ] README·위키 실행 방법 갱신, 커밋·푸시·PR 및 CI 확인

## 파일과 경계

Tools/AliceEditor/*, Tools/CMakeLists.txt, Engine/Tests/Editor_Tests.cpp,
Engine/Tests/CMakeLists.txt, Scripts/editor.ps1, Docs/EDITOR.md,
README·AGENTS·상태 문서·Wiki 및 이 작업의 설계/계획.
기존 모듈 공개 헤더는 변경하지 않는다. 에디터의 Win32/GDI 사용은 Tools 안에 격리한다.
씬 뷰는 배치용 도형 미리보기이며 메시 로딩·게임플레이 실행은 후속 Runtime/Renderer 작업이다.
외부 의존성·미디어 애셋을 추가하지 않는다.
