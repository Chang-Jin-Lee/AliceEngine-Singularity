---
title: Windows 에디터 실행과 사용법
synced: Docs/EDITOR.md
---

# Windows 에디터 실행과 사용법

`Alice.Editor.exe`를 실행하면 독립된 데스크톱 창이 열립니다. 프로젝트 문서 목록, 씬 액터,
속성 편집, 배치 뷰포트와 검증 콘솔을 제공합니다. Windows SDK 이외의 UI 라이브러리나
외부 미디어 애셋은 사용하지 않습니다.

## 실행

저장소 루트에서 PowerShell로 실행합니다. 실행 파일이 없으면 자동으로 빌드합니다.

```powershell
pwsh -NoProfile -File .\Scripts\editor.ps1
```

이미 빌드했다면 탐색기에서 `build\bin\Alice.Editor.exe`를 더블클릭해도 됩니다.
기본 프로젝트는 `Samples/CubePlayground`이며, `scenes/main.scene.yaml`을 먼저 엽니다.

```powershell
# 다른 콘텐츠 폴더 열기
pwsh -NoProfile -File .\Scripts\editor.ps1 -Project D:\MyGame\Content

# 에디터를 닫은 뒤 최신 소스로 재빌드하고 실행
pwsh -NoProfile -File .\Scripts\editor.ps1 -Build
```

현재 네이티브 창은 Windows 전용입니다. macOS/Linux에서는 CLI와 문서 모델 테스트를 사용할 수 있습니다.

## 편집 순서

새 씬은 상단 **New scene**을 누르고 새 파일 이름을 지정하면 만듭니다. 기존 파일은
덮어쓰지 않습니다. 빈 씬에서 **+ Actor**를 누르면 기본 큐브 모양으로 표시되는 액터가 생깁니다.
플레이 예시는 **Cube demo**로 다시 열 수 있습니다.

1. 왼쪽 Project 목록에서 문서를 더블클릭합니다. 다른 폴더는 **Open project**, 개별 파일은 **Open file**로 엽니다.
2. Scene 탭에서 왼쪽 Hierarchy의 액터를 선택합니다. 뷰포트의 피벗 근처를 클릭해도 선택됩니다.
3. Inspector에서 이름·Position·Rotation·Scale을 수정하고 **Apply changes**를 누릅니다.
4. **Validate** 또는 `F5`로 검사합니다. Console에서 진단 코드, 위치, 수정 힌트를 확인합니다.
5. **Save** 또는 `Ctrl+S`로 파일에 저장합니다. 창 제목의 `*`가 사라지면 저장된 상태입니다.

`+ Actor`와 `Delete`는 최상위 액터를 추가·삭제합니다. Scene 뷰는 마우스 휠로 확대/축소,
가운데 버튼 드래그로 이동하고 **Frame origin**으로 원점 뷰를 복원합니다.

## 패널과 AI 터미널

화면 왼쪽은 편집기, 오른쪽은 실제 PowerShell 터미널입니다. 사이의 세로선을 드래그하면
비율을 바꿀 수 있습니다. Hierarchy/Scene, Scene/Inspector, Hierarchy/Project, Console 경계도
마우스로 조절됩니다. 패널의 최소 크기를 유지하므로 입력 칸이 사라지지 않습니다.

터미널은 시작한 프로젝트 폴더에서 실행되며 `build/bin`이 PATH에 추가됩니다.
`alice verbs --json`, `alice schema list --json`, `alice check . --json`을 바로 실행할 수 있습니다.
설치된 AI CLI도 이 터미널에서 직접 실행할 수 있습니다. AI 프로그램 설치·로그인은 별도로 필요합니다.
터미널에서 문서를 수정한 뒤 **Reload**를 눌러 편집기에 반영합니다. 미저장 편집은 먼저 확인합니다.
프로젝트를 바꿔도 실행 중인 터미널의 작업 폴더는 유지됩니다. 필요하면 터미널에서 `Set-Location`으로 이동합니다.
터미널에 포커스가 있으면 편집기의 단축키나 큐브 이동 입력을 받지 않습니다.

`Ctrl+V`, `Shift+Insert` 또는 오른쪽 클릭으로 붙여넣습니다. 마우스로 영역을 선택한 뒤
`Ctrl+Shift+C`로 복사합니다(선택이 없으면 보이는 화면 복사). `Ctrl+C`는 실행 중인 명령 중단입니다.
휠 또는 `Shift+PageUp/PageDown`으로 최근 2,000줄을 보고, 입력하면 현재 프롬프트로 돌아옵니다.
한글 IME 입력을 지원합니다. 실행 셸은 Windows PowerShell이며 프로필을 자동 실행하지 않습니다.

## 큐브 입력 예시

1. **Cube demo**를 누릅니다. Player Cube, Ground, 두 Landmark가 보입니다.
2. Player Cube를 선택하고 Inspector에서 Position·Rotation·Scale을 바꿉니다. **Apply changes**와
   **Save**로 문서에 저장하고 **Reload**로 유지되는지 확인할 수 있습니다.
3. **Play**를 누르고 Scene 영역을 클릭합니다.

| 키 | 동작 |
|---|---|
| W / A / S / D | XZ 평면 이동 |
| Space / Ctrl | 위 / 아래 이동 |
| Q / E | Y축 회전 |
| F / R | 축소 / 확대 |

Play 중에도 Inspector로 위치·회전·크기를 바꿀 수 있습니다. 이때 값은 Runtime World의 임시
Transform에만 적용됩니다. **Stop**을 누르면 Play 직전의 문서 상태로 돌아갑니다. 파일 저장과
문서 전환은 Play 중 비활성화됩니다. 터미널·속성 입력 중이거나 다른 창으로 이동하면 이동 입력이 멈춥니다.

이 예시의 입력 맵은 `Samples/CubePlayground/input/default.input.yaml`입니다. 최상위 액터 하나에
`tags: [player]`가 필요하며, 다섯 필수 axis 액션과 지원하는 키 쌍은 [샘플 설명](https://github.com/Chang-Jin-Lee/AliceEngine-Singularity/blob/main/Samples/CubePlayground/README.md)에 있습니다.
예제 전용 미리보기이므로 FirstLight의 일반 게임 규칙 실행과는 별개입니다.

## 문서와 변경 이력

**Document** 탭에서 YAML/JSON 원문을 편집할 수 있습니다. 컴포넌트와 중첩 자식 액터는
이 탭에서 편집합니다. 원문을 편집하는 동안 Inspector는 비활성화되어 서로의 수정을 덮어쓰지 않습니다.
Validate, Save 또는 탭 전환 시 원문을 문서 모델에 반영합니다.

`Ctrl+Z` / `Ctrl+Y`로 적용된 문서 변경을 취소·복원합니다. 적용 전 Inspector 입력은
Undo로 되돌릴 수 있지만 Redo 이력에는 들어가지 않습니다. 원문 이력도 글자 단위가 아닌
모델에 적용한 문서 단위입니다. 최근 128개 변경을 메모리에 보관합니다.

오류가 있는 문서는 저장하지 않습니다. 외부 프로그램이 파일을 수정했거나 삭제했다면 저장을
거부하므로 편집 내용을 복사해 보관한 뒤 파일을 다시 열어 변경을 합치십시오. 파일을 바꾸거나
창을 닫을 때 미저장 변경이 있으면 저장·버리기·취소를 선택합니다.

저장은 같은 폴더의 `.alice-editor.tmp`를 완성한 뒤 원본과 교체합니다. 기존 임시 파일이
있으면 덮어쓰지 않습니다. 먼저 그 파일의 복구 필요 여부를 확인하십시오.

## 현재 범위

- 뷰포트는 문서의 최상위 액터 변환을 나타내는 와이어 도형입니다. 실제 메시·재질·조명 렌더링은 후속 작업입니다.
- 큐브 Play/Stop은 실제 ECS Transform을 사용하는 에디터 예시입니다. 범용 씬 로더·물리·behavior 실행은 후속 작업입니다.
- 중첩 자식의 계층 UI, 드래그 이동 도구, 자유 도킹은 아직 없습니다. 현재 분할선으로 패널 크기를 조절합니다.
- 문서 검증은 미디어 파일의 존재나 배포 권한을 보증하지 않습니다. FirstLight의 참조 메시·음원은 저장소에 없습니다.
- 저장 충돌 검사는 저장 직전의 내용을 비교합니다. 다른 프로세스와의 완전한 동시 편집 잠금은 제공하지 않습니다.

## 검증

```powershell
pwsh -NoProfile -File .\Scripts\verify.ps1
.\build\bin\Alice.Tests.exe --filter=Editor
pwsh -NoProfile -File .\Scripts\editor.ps1 -SmokeTest
```

SmokeTest는 실제 창을 잠깐 열고 속성 입력·Undo·오류 복구·탭 전환·원문 입력·분할선·터미널·Play/Stop을 검사한 뒤
종료합니다. 결과는 `build/bin/editor-smoke.json`에 기록합니다. CTest의 `Alice.Editor.Smoke`도
같은 검사를 실행합니다. 이 검사는 프로젝트 문서를 저장하지 않습니다.
