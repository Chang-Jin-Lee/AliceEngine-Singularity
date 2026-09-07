# Material

> `schema: alice/material/1`

표면이 빛에 어떻게 반응하는지. 셰이더 코드를 쓰지 않고 값으로 기술한다

## 필드

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|---|---|:---:|---|---|
| `schema` | `string` | ✓ |  | 항상 alice/material/1 |
| `name` | `string` | ✓ |  | 사람이 읽는 이름. 문서 안에서 참조 대상이 된다 |
| `shader` | `pbr | unlit | toon | custom` |  | `"pbr"` | 셰이딩 모델 |
| `customShader` | `string` |  |  | shader 가 custom 일 때의 셰이더 문서 |
| `blend` | `opaque | masked | translucent | additive` |  | `"opaque"` | 합성 방식. opaque 가 가장 빠르다 |
| `doubleSided` | `boolean` |  | `false` | 뒷면도 그릴지. 켜면 드로우 비용이 는다 |
| `params` | `object` |  |  | 셰이딩 파라미터 |
| `textures` | `[object]` |  |  | 텍스처 바인딩 목록 |

## 흔한 실수

**YAML 에서 공백 뒤의 # 는 주석이다. 색상 문자열은 반드시 따옴표로 감싼다**

```yaml
# 이렇게 쓰면 안 된다
baseColor: #ff8800

# 이렇게 쓴다
baseColor: "#ff8800"
```

## 확인

```bash
alice schema show alice/material/1
alice new material MyThing
```
