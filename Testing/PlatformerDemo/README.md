# Platformer Physics Demo

외부 아트 없이 Alice 엔진에서 좌우 이동·점프·낙하·착지·벽/천장 충돌을 확인하는 예제다. 기존 도형 배치 조사에서 실제 물리 실행 예제로 확장했다. 플레이어 한 명과 고정 사각형 지형을 지원한다.

## 실행

Windows PowerShell에서 저장소 루트 기준으로 실행한다.

```powershell
.\Scripts\build.ps1 -BuildDir Testing/build -WarningsAsErrors
.\Testing\build\bin\alice.exe check Testing/PlatformerDemo --json
.\Testing\build\bin\Alice.Editor.exe --project Testing/PlatformerDemo
```

씬을 열고 **Play**, 뷰포트를 클릭한다. A/D로 좌우 이동하고 Space로 점프한다. 공중 점프는 허용하지 않는다. Space를 계속 누르고 있어도 자동 재점프하지 않는다. 바닥과 두 발판을 밟고 오른쪽 도착 표식까지 이동해 본다. 도착 표식은 장식이며 승리 판정은 없다. 바닥 밖으로 떨어지면 Stop → Play로 재시작한다. Stop하면 원본 씬으로 돌아온다. 물리 실행 중 Inspector 수정은 진단으로 거부하며 Stop 후 편집한다.

짧은 CLI 명령이 필요한 AI 터미널에서는 해당 세션에 경로를 추가한다.

```powershell
$env:Path = (Resolve-Path Testing/build/bin).Path + ';' + $env:Path
alice doctor --json
alice verbs --json
alice schema show alice/component/character2d/1 --json
```

`alice`는 콘텐츠 도구다. 게임 실행용 `alice run`은 없으며 창은 `Alice.Editor.exe`로 연다. 일반 빌드 경로를 사용했다면 실행 파일 경로도 `build/bin`으로 바꾼다.

## AI가 조절하는 물리

플레이어에는 `tags: [player]`와 다음 컴포넌트를 둔다. `character2d`가 있으면 플랫폼어 모드다. 없으면 기존 CubePlayground의 Transform 미리보기가 동작한다.

```yaml
components:
  character2d: { speed: 4, jumpSpeed: 7, gravityScale: 1 }
  collider: { shape: box }
```

| 설정 | 기본값 | 허용 범위 |
|---|---|---|
| speed | 4 m/s | 0~100 |
| jumpSpeed | 7 m/s | 0~100 |
| gravityScale | 1 | 0~10 |
| environment.gravity | [0, -9.81, 0] | X/Z=0, Y=-100~0 |

고정 지형은 collider만 둔다. collider.size 기본값은 [1,1,1]이고 크기와 center는 Transform.scale을 곱해 해석한다. 샘플은 단위 collider를 사용해 표시 큐브와 충돌 크기가 일치한다. center나 size를 바꾸면 표시 큐브와 충돌 경계가 다를 수 있다. Collider 전용 시각화는 아직 없다.

입력 문서는 MoveX(axis)와 Jump(button) 두 액션이 필요하다. MoveX는 key.ad/key.qe, Jump는 key.space/key.w를 지원한다. 일반 behavior 실행기는 별도 작업이며 physics.* 등록 동사 전체를 실행하는 구현은 아니다.

## 계산 및 확장 계약

- XY 평면, Y 위쪽, 미터·초. 1/60초 고정 스텝, 프레임당 최대 6회. 긴 프레임은 0.1초로 제한하고 physics.time.clamped 로그를 남긴다. 최대 낙하 속도는 100m/s다.
- 이동 경로 전체를 검사하는 swept AABB와 접촉 방향 속도 제거로 얇은 지형 관통을 막는다. 회전·경사면·동적 물체 상호 충돌·이동 발판·일방향 발판·마찰/반발·트리거는 미지원이다.
- 정적 BVH는 로드 시 생성한다. 쿼리는 작업 배열 할당 없이 트리를 조회한다. 프로파일러가 켜져 있으면 별도 계측 비용이 발생한다. 존은 Physics.Step(에디터 스텝 묶음), Physics.Move(캐릭터 충돌), Physics.Query(공간 검색)다.
- 코어 좌표/이동량/반크기는 최대 1e6, 반크기 최소 1e-6, 접촉 허용 오차 1e-8이다. 편집기 Transform은 기존 범위(초기 좌표 ±10000, scale 0.05~100)도 적용한다. 시작 겹침은 최대 16회 보정하고 불가능하면 위치와 힌트가 있는 오류를 반환한다. 이동 충돌 반복은 최대 4회다.
- 일반 rigidbody, 비기본 layer, trigger, 회전 collider, Z≠0, 자식 계층, 비활성 액터, behavior는 이 모드에서 진단으로 거부한다. player 외의 character2d도 거부한다. 중복·잘못된 입력 바인딩을 조용히 무시하지 않는다.

사용자 물리는 [Runtime/Physics/Platformer.h](../../Engine/Runtime/Physics/Platformer.h)의 IMotionSolver를 구현한다. 입력은 현재 Box·이동량·진단 위치, 결과는 보정된 Box·막힌 축·접지·검사 횟수다. 제공자는 캐릭터 상태를 숨겨 저장하지 않는다. 호출자가 위치·속도·입력 상태를 소유하고 저장/복원한다. 지형은 Build 성공 후 교체되며 실패 시 이전 지형이 유지된다.

[CustomPhysicsExample.cpp](CustomPhysicsExample.cpp)는 이동량을 절반으로 조절하면서 기본 충돌을 재사용하는 실제 컴파일·테스트 예제다. 네이티브 호출자가 제공자를 선택한다. 에디터가 YAML 이름으로 임의 제공자나 DLL을 불러오는 기능, 임의 물리 수식 DSL은 이번 범위가 아니다. YAML 튜닝과 C++ 제공자 교체를 구분한다.

## 검증과 성능

```powershell
.\Testing\build\bin\Alice.Tests.exe --filter=Physics
ctest --test-dir Testing/build --output-on-failure -R Alice.Editor
.\Testing\build\bin\Alice.PhysicsBench.exe
.\Scripts\verify.ps1 -BuildDir Testing/build
```

전체 테스트 191개와 Windows 실제 창 smoke 2개를 확인했다. 새 씬 smoke는 낙하·착지·점프·이동·Stop 복구를 검사한다. 실제 창 자동 검사이며 사람이 키보드로 전체 코스를 완주했다는 뜻은 아니다. [검증 기록](VERIFICATION.md)과 [성능 원시 결과](../Measurements/platformer-2026-09-17.json)를 참고한다.

i9-14900HX, Windows, MSVC 19.51, RelWithDebInfo /O2, 단일 스레드, 프로파일러 off에서 지형 10,000개·독립 캐릭터 쿼리 100개 묶음의 평균은 BVH 사용 시 희소 배치 **26.285µs**, 밀집 배치 **100.526µs**였다. 동일 충돌 코드의 전수 검색은 각각 11,498.9µs, 18,008.7µs였다. 준비 20회·측정 100회, 같은 시작 위치를 반복 조회하는 작은 벤치마크다. 게임 전체 프레임이나 Box2D 대비 성능 수치가 아니다. 밀집 배치도 겹친 지형의 최악 조건은 아니다.

## 제작 기록

작성자: Codex, 사용자 승인 작업 SID-07, 2026-09-17. YAML·C++ 예제·문서·벤치마크는 직접 작성했다. 입력/프로젝트 구조는 저장소 CubePlayground를 참고했다. 외부 이미지·모델·텍스처·음원·폰트와 타 엔진 소스는 포함하지 않았다. [MIT 라이선스 원문](../../LICENSE)을 적용한다. 외부 원본 애셋 URL은 없다. 알고리즘 참고 자료와 대안은 [PHYSICS_DESIGN.md](PHYSICS_DESIGN.md)에 기록했다.
