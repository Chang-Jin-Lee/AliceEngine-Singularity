# SID-07 · 최소 2D 플랫폼어 물리

```
상태: 진행중
크기: L
선행: SID-02
담당: Codex (Sidney, 사용자 승인으로 스키마·에디터·테스트 연결 포함)
```

## 무엇을
고정 지형과 사각형 캐릭터용 고정 스텝 물리, 교체 가능한 C++ 제공자, YAML 설정, Play 연결을 구현한다.
## 왜
문서 검증만 가능했던 플랫폼어 예제를 실제 이동·점프·착지로 검증하기 위해서다.
## 완료 기준
- [ ] swept AABB, 정적 공간 인덱스, 접지·벽·천장 충돌 테스트
- [ ] 사용자 제공자 교체 예제와 테스트
- [ ] Testing/PlatformerDemo 콘텐츠 검사 및 실제 창 Play smoke
- [ ] 성능 측정·제한·사용법 문서
- [ ] 전체 verify 성공 및 PR 리뷰
## 건드리는 파일
Engine/Runtime/Physics/*, Engine/Schema/CoreSchemas.cpp, Tools/AliceEditor/*, Engine/Tests/*, 빌드 목록, 생성 스키마, Testing/*.
## 건드리면 안 되는 것
RHI 구현 및 일반 behavior 평가기. 다른 진행중 작업은 수정하지 않는다.
