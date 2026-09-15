# Native Editor Implementation Plan

> Use superpowers:executing-plans to implement this plan.

**Goal:** Alice.Editor.exe에서 씬 문서를 열고 시각적으로 편집·저장한다.
**Architecture:** 플랫폼 독립 DocumentModel + Tools 내부 Win32 창 + GDI 배치 뷰포트.
**Tech Stack:** C++20, Windows SDK, 기존 Doc/Schema/Verbs. 새 서드파티·미디어 없음.
**Spec:** [설계](../specs/2026-09-14-native-editor-design.md)

- [x] Tools/AliceEditor/DocumentModel.h 및 Editor_Tests.cpp로 로드, 편집, undo/redo,
  잘못된 문서 저장 차단, 외부 수정 보호를 명세하고 구현 전 실패를 확인한다.
- [x] DocumentModel.cpp와 AtomicSave.cpp를 구현하고 회귀 테스트를 통과한다.
- [x] Win32Editor.cpp와 SceneViewport.cpp를 연결하고 기본 FirstLight를 열어 실제 창을 표시한다.
- [x] 실제 창 smoke 검사, 저장/진단 회귀 결함 주입, 독립 리뷰와 화면 QA를 수행한다.
- [x] Scripts/editor.ps1, Docs/EDITOR.md, README 및 생성 위키를 갱신한다.
- [ ] 공식 verify, 위키 빌드, 커밋·푸시·PR 및 CI를 완료한다.
