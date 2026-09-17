# CubePlayground

외부 애셋 없이 큐브의 위치·회전·크기와 키 입력을 확인하는 Windows 에디터 예제입니다.

```powershell
.\Scripts\build.ps1
.\build\bin\Alice.Editor.exe --project Samples/CubePlayground
.\build\bin\alice.exe check Samples/CubePlayground --json
```

`scenes/main.scene.yaml`을 열고 **Play**를 누른 뒤 뷰포트를 클릭합니다.
`player` 태그가 있는 최상위 액터 하나에만 입력이 적용됩니다.

| 키 | 동작 | 입력 액션 / binding |
|---|---|---|
| A / D | X축 음 / 양 방향 이동 | MoveX / `key.ad` |
| W / S | Z축 음 / 양 방향 이동 | MoveZ / `key.ws` |
| Ctrl / Space | 아래 / 위 이동 | MoveY / `key.control_space` |
| Q / E | Y축 음 / 양 방향 회전 | RotateY / `key.qe` |
| F / R | 축소 / 확대 | Scale / `key.fr` |

이동은 초당 4단위이고 대각선 속도를 정규화합니다. 회전은 초당 90도,
확대·축소는 초당 1배율입니다. 프레임 시간은 0.1초로 제한합니다.
위치는 ±10000, 회전은 ±360000도, 각 축 크기는 0.05~100 범위입니다.
Inspector에서 실행 중 값을 바꿀 수 있으며, **Stop**하면 원래 편집 문서로 돌아갑니다.
실행 중 변경은 YAML에 저장하지 않습니다. 다른 패널에 포커스가 있으면 이동하지 않습니다.

입력 파일은 프로젝트의 `input/default.input.yaml`입니다. 위 다섯 액션을 `axis`로 정의합니다.
각 액션에 위 표의 키 쌍을 바꿔 배정할 수 있습니다. `key.rf`도 R 음 / F 양 방향으로
지원합니다. 한 쌍에서 두 키를 함께 누르면 상쇄되고, 여러 binding의 합은 -1~1로 제한됩니다.
지원하지 않는 액션·입력 장치·binding은 Play 진단으로 수정 위치와 힌트를 표시합니다.

이 예제는 에디터의 **ECS Transform 미리보기 컨트롤러**입니다. 각 최상위 액터의
Transform은 별도 Runtime World에 복사됩니다. 일반 behavior 실행, 자식 액터 계층,
물리 충돌·중력, GPU 렌더링은 구현하지 않습니다. 바닥과 표식도 에디터의 큐브 표시입니다.
터미널은 실제 PowerShell 세션이며, 설치된 AI CLI는 그 안에서 직접 실행할 수 있습니다.

## 제작 기록

| 파일 | 제작자·제작 경위 | 외부 소재 | 권리·확인일 |
|---|---|---|---|
| `project.yaml` | AliceEngine-Singularity 기여 작업에서 직접 작성 | 없음 | 저장소 MIT 라이선스, 2026-09-16 |
| `scenes/main.scene.yaml` | 같은 작업에서 좌표·큐브 배치를 직접 설계 | 없음 | 저장소 MIT 라이선스, 2026-09-16 |
| `input/default.input.yaml` | 같은 작업에서 키 매핑을 직접 작성 | 없음 | 저장소 MIT 라이선스, 2026-09-16 |
| `README.md` | 같은 작업에서 실행·제약 설명을 직접 작성 | 없음 | 저장소 MIT 라이선스, 2026-09-16 |

원본은 이 저장소의 위 파일이며 외부 원본 URL은 없습니다. 이미지·모델·텍스처·음원·폰트는
추가하지 않았습니다. 라이선스 원문은 저장소 루트의 [`LICENSE`](../../LICENSE)에 있습니다.
