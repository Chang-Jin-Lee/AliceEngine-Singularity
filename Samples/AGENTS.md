# Samples/ 에서 일할 때

> 여기는 **코드가 아니라 콘텐츠**입니다. 문법은 [`Docs/CONTENT_FORMAT.md`](../Docs/CONTENT_FORMAT.md).

## FirstLight

코드 한 줄 없이 문서 10개로 된 완전한 프로젝트입니다.
이 엔진의 주장이 실제로 성립하는지 보여주는 증거이므로, **항상 검증을 통과해야 합니다.**

```bash
./build/bin/alice check Samples          # 0 오류
./build/bin/alice fmt Samples --check    # 정규화 상태
```

## 문서를 고칠 때

```bash
alice schema show alice/material/1   # 어떤 필드가 있는지
alice verbs                          # 어떤 동작이 있는지
alice verbs --symbols                # 조건식에서 뭘 읽을 수 있는지
```

**추측하지 마십시오.** 위 명령이 답을 줍니다.

## 고치고 나면 반드시

```bash
alice check Samples && alice fmt Samples
```

`fmt` 는 주석과 빈 줄을 보존합니다. 짧은 인자 맵은 `{ a: 1, b: 2 }` 한 줄로 유지합니다.
두 번 돌려도 같은 결과가 나옵니다(멱등).

## 가장 흔한 실수

```yaml
tint: #ff8800       # 값이 통째로 사라진다 (YAML 이 주석으로 먹는다)
tint: "#ff8800"     # 이렇게

position: 0         # 에러
position: [0, 0, 0] # 이렇게

health: "100"       # 문자열이 된다
health: 100         # 이렇게

enabled: no         # 문자열 "no" 다. 불리언이 아니다
enabled: false      # 이렇게
```

## 새 샘플을 추가하려면

```bash
alice new scene MyLevel -o Samples/MyProject/scenes/main.scene.yaml
```

그리고 `alice check` 가 0오류가 될 때까지 고칩니다.
샘플은 **읽는 사람을 위한 것**이므로 주석을 넉넉히 다십시오. `fmt` 가 보존합니다.
