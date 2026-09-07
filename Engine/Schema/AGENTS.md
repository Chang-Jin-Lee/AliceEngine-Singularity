# Schema/ 에서 일할 때

> **이 디렉터리가 엔진의 콘텐츠 모델입니다.** `CoreSchemas.cpp` 하나가
> "이 엔진으로 무엇을 만들 수 있는가"의 전부입니다.

## 새 콘텐츠 타입을 추가하려면

`CoreSchemas.cpp` 에 스키마를 하나 만들고 `RegisterCoreSchemas()` 에 등록하십시오.
그 순간 다음이 **전부 자동으로** 따라옵니다.

- `alice schema list` / `alice schema show`
- `alice new <타입>` 뼈대 생성
- `alice check` 검증
- JSON Schema 생성 → 에디터 자동완성
- 위키 레퍼런스 페이지

어느 하나라도 손으로 갱신해야 한다면 그건 설계 실패입니다.

## 스키마에 반드시 넣을 것

```cpp
B::Map("alice/thing/1")
    .Title("Thing")
    .Describe("한두 문장으로 이게 무엇인지")
    .Field("name", Name(), "이 필드가 무엇인지")   // ← 설명 필수
        .Require()
    .Field("count", B::Int(), "개수")
        .Default(Value{static_cast<i64>(0)})
    .Example(...)                                  // ← 예시가 설명 열 줄보다 낫다
    .Pitfall("틀린 예", "맞는 예", "왜 틀리는지")   // ← 흔한 실수는 여기에
    .Build();
```

`Engine/Tests/Schema_Tests.cpp` 의 `EverySchemaHasDescriptionAndFieldDocs` 가
설명 누락을 실패로 만듭니다.

## 제약은 필드에 걸린다

```cpp
.Field("metallic", B::Float(), "0..1").Range(0.0, 1.0)
```

`Range` 는 **직전 필드**에 걸립니다. 부모 맵이 아닙니다.
(순진하게 구현하면 부모에 걸려 조용히 아무 일도 안 합니다 —
`SchemaBuilder::ConstraintTarget()` 의 주석을 보십시오)

## 필드 이름을 바꿔야 한다면

```cpp
.Field("asset", ...).Alias("model").Alias("meshPath")
```

옛 이름을 `Alias` 로 남기면 그 이름을 쓴 문서에 **경고와 함께 새 이름을 안내**합니다.
그냥 바꾸면 기존 콘텐츠가 "알 수 없는 필드" 에러로 깨집니다.

## 스키마를 바꾼 뒤

```bash
./build/bin/alice schema emit Schemas
cd Wiki && npm run generate
```

CI 가 이 생성물이 최신인지 검사합니다.

## 버전

`alice/actor/1` 의 `1` 은 스키마 버전입니다. **호환이 깨지는 변경**에만 올립니다.

- 필드 추가 (선택) → 버전 유지
- 필드 이름 변경 → `Alias` 로 처리, 버전 유지
- 필수 필드 추가, 타입 변경, 의미 변경 → **새 버전**
