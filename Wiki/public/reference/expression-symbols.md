# 조건식 심볼

`when:` 에서 읽을 수 있는 이름 전부입니다. 여기 없는 이름을 쓰면 `alice check` 가 잡습니다.

| 이름 | 종류 | 타입 | 설명 |
|---|---|---|---|
| `actor.count` | 함수(1~1) | `int` | 해당 태그를 가진 액터 수 |
| `actor.distance` | 함수(1~1) | `float` | 이 액터와 그 이름의 액터 사이 거리(미터) |
| `actor.exists` | 함수(1~1) | `bool` | 그 이름의 액터가 씬에 있는지 |
| `anim.finished` | 함수(1~1) | `bool` | 그 이름의 클립이 끝났는지 |
| `anim.normalizedTime` | 값 | `float` | 현재 클립의 진행도 0..1 |
| `anim.state` | 값 | `string` | 현재 애니메이션 상태 이름 |
| `input.axis` | 함수(1~1) | `float` | 축 행동의 값 (-1..1) |
| `input.held` | 함수(1~1) | `bool` | 지금 눌려 있는 행동인지 |
| `input.pressed` | 함수(1~1) | `bool` | 이번 프레임에 눌리기 시작한 행동인지. 인자는 input 문서의 행동 이름 |
| `input.released` | 함수(1~1) | `bool` | 이번 프레임에 떼어진 행동인지 |
| `math.abs` | 함수(1~1) | `float` | 절댓값 |
| `math.ceil` | 함수(1~1) | `int` | 올림 |
| `math.clamp` | 함수(3~3) | `float` | 값을 [최소, 최대] 안으로 자른다 |
| `math.cos` | 함수(1~1) | `float` | 코사인(라디안) |
| `math.floor` | 함수(1~1) | `int` | 내림 |
| `math.lerp` | 함수(3~3) | `float` | 선형 보간 |
| `math.max` | 함수(2~2) | `float` | 둘 중 큰 값 |
| `math.min` | 함수(2~2) | `float` | 둘 중 작은 값 |
| `math.pi` | 값 | `float` | 원주율 |
| `math.random` | 함수(0~2) | `float` | 난수. 인자 없으면 0..1, 둘이면 그 범위. **비결정적이라 리플레이가 어긋난다** |
| `math.round` | 함수(1~1) | `int` | 반올림 |
| `math.sin` | 함수(1~1) | `float` | 사인(라디안) |
| `math.sqrt` | 함수(1~1) | `float` | 제곱근 |
| `physics.grounded` | 값 | `bool` | 이 액터가 바닥에 닿아 있는지 |
| `physics.speed` | 값 | `float` | 속도의 크기(m/s) |
| `physics.touching` | 함수(1~1) | `bool` | 인자로 준 태그를 가진 것과 닿아 있는지 |
| `physics.velocityY` | 값 | `float` | 수직 속도(m/s). 낙하 판정에 쓴다 |
| `scene.name` | 값 | `string` | 현재 씬 이름 |
| `self.active` | 값 | `bool` | 이 액터가 켜져 있는지 |
| `self.hasTag` | 함수(1~1) | `bool` | 이 액터가 해당 태그를 가졌는지 |
| `self.name` | 값 | `string` | 이 액터의 이름 |
| `self.position` | 값 | `vec3` | 이 액터의 월드 위치 |
| `time.delta` | 값 | `float` | 이전 프레임과의 간격(초) |
| `time.frame` | 값 | `int` | 프레임 번호 |
| `time.now` | 값 | `float` | 게임 시작으로부터의 초 |
| `time.since` | 함수(1~1) | `float` | 인자로 준 시각 이후 흐른 초 |

