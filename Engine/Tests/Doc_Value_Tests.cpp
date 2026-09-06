// SPDX-License-Identifier: MIT
#include "TestFramework.h"

#include "Doc/Value.h"

using namespace alice;
using namespace alice::doc;

ALICE_TEST(DocValue, ScalarKinds) {
    ALICE_CHECK(Value{}.IsNull());
    ALICE_CHECK(Value{true}.IsBool());
    ALICE_CHECK(Value{static_cast<i64>(3)}.IsInt());
    ALICE_CHECK(Value{3.0}.IsFloat());
    ALICE_CHECK(Value{"text"}.IsString());

    // 3 과 3.0 은 다른 값이다. 스키마가 이 차이를 봐야 하기 때문에 섞지 않는다.
    ALICE_CHECK(!Value{static_cast<i64>(3)}.DeepEquals(Value{3.0}));
}

ALICE_TEST(DocValue, IntFloatCoercionIsAsymmetric) {
    // 정수는 실수로 읽어도 안전하다.
    ALICE_CHECK(Value{static_cast<i64>(7)}.AsFloat() == 7.0);
    // 실수는 정확히 정수일 때만 정수로 읽힌다. 3.5 를 3 으로 깎으면 버그가 숨는다.
    ALICE_CHECK(Value{3.0}.AsInt(-1) == 3);
    ALICE_CHECK(Value{3.5}.AsInt(-1) == -1);
}

ALICE_TEST(DocValue, MapPreservesInsertionOrder) {
    Value m = Value::MakeMap();
    m.Set("zebra", Value{1});
    m.Set("apple", Value{2});
    m.Set("mango", Value{3});

    const auto keys = m.Keys();
    ALICE_REQUIRE(keys.size() == 3);
    ALICE_CHECK_STR(keys[0], "zebra");
    ALICE_CHECK_STR(keys[1], "apple");
    ALICE_CHECK_STR(keys[2], "mango");

    // 덮어써도 자리는 그대로여야 한다. 그래야 diff 가 한 줄만 바뀐다.
    m.Set("zebra", Value{9});
    ALICE_CHECK_STR(m.Keys()[0], "zebra");
    ALICE_CHECK(m["zebra"].AsInt() == 9);
    ALICE_CHECK(m.Size() == 3);
}

ALICE_TEST(DocValue, MissingKeyReturnsSharedNull) {
    const Value m = Value::MakeMap();
    ALICE_CHECK(m["nope"].IsNull());
    // 체이닝해도 크래시하지 않아야 한다.
    ALICE_CHECK(m["a"]["b"]["c"].IsNull());
}

ALICE_TEST(DocValue, DeepCopyIsIndependent) {
    Value original = Value::MakeMap();
    original.Set("items", Value::MakeSeq());
    original["items"].Push(Value{1});

    Value copy = original;
    copy["items"].Push(Value{2});

    ALICE_CHECK(original["items"].Size() == 1);
    ALICE_CHECK(copy["items"].Size() == 2);
}

ALICE_TEST(DocValue, PathAccess) {
    Value root = Value::MakeMap();
    Value actor = Value::MakeMap();
    Value transform = Value::MakeMap();
    Value pos = Value::MakeSeq();
    pos.Push(Value{1.0});
    pos.Push(Value{2.0});
    pos.Push(Value{3.0});
    transform.Set("position", std::move(pos));
    actor.Set("transform", std::move(transform));
    Value actors = Value::MakeSeq();
    actors.Push(std::move(actor));
    root.Set("actors", std::move(actors));

    const Value* v = root.AtPath("actors[0].transform.position[1]");
    ALICE_REQUIRE(v != nullptr);
    ALICE_CHECK(v->AsFloat() == 2.0);

    ALICE_CHECK(root.AtPath("actors[9].transform") == nullptr);
    ALICE_CHECK(root.AtPath("actors[0].missing") == nullptr);
}

ALICE_TEST(DocValue, DeepEqualsComparesKeyOrder) {
    Value a = Value::MakeMap();
    a.Set("x", Value{1});
    a.Set("y", Value{2});

    Value b = Value::MakeMap();
    b.Set("y", Value{2});
    b.Set("x", Value{1});

    // 같은 내용이라도 순서가 다르면 다른 문서다. 이 엔진에서 키 순서는 의미가 있다.
    ALICE_CHECK(!a.DeepEquals(b));

    Value c = Value::MakeMap();
    c.Set("x", Value{1});
    c.Set("y", Value{2});
    ALICE_CHECK(a.DeepEquals(c));
}
