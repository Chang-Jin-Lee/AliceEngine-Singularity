# Behavior

> `schema: alice/behavior/1`

액터의 동작을 규칙으로 적는다. **이 엔진에는 게임플레이 스크립트 언어가 없다.** 언리얼의 C++/블루프린트, 유니티의 C# 이 있던 자리를 이 문서가 대신한다. 규칙은 선언적이고, 순서에 의존하지 않으며, 조건이 참이 될 때만 실행된다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `schema` | `string` | ✓ |  | 항상 alice/behavior/1 |
| `name` | `string` | ✓ |  | 사람이 읽는 이름. 문서 안에서 참조 대상이 된다 |
| `rules` | `[object]` | ✓ |  | 규칙 목록 |
| `variables` | `object` |  |  | 이 행동이 쓰는 지역 변수와 초깃값 |

## 예시

```json
{
  "when": "input.pressed(\"Jump\") and physics.grounded"
}
```

## 흔한 실수

**do 는 항상 목록이고, 각 항목은 동사 하나를 키로 갖는 맵이다**

```yaml
# 이렇게 쓰면 안 된다
do: audio.play

# 이렇게 쓴다
do:
  - audio.play: { sound: ... }
```

## 확인

```bash
alice schema show alice/behavior/1
alice new behavior MyThing
```
