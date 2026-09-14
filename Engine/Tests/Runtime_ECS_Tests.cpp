// SPDX-License-Identifier: MIT
// Exercise the public ECS contract with owning, over-aligned values so lifetime bugs are observable.
#include "TestFramework.h"
#include "Runtime/World.h"
#include "Schema/Registry.h"
#include "Schema/SchemaBuilder.h"

#include <algorithm>
#include <new>
#include <optional>

using namespace alice;
using namespace alice::runtime;
using doc::Value;

namespace {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // Intentional padding is the condition this over-alignment fixture tests.
#endif
template<int Tag> struct alignas(64) Native {
    static inline int live = 0;
    i64 value = 0;
    std::string label;
    Native() { ++live; }
    Native(Native&& other) noexcept : value(other.value), label(std::move(other.label)) { ++live; }
    ~Native() { --live; }
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

Value Data(i64 number, std::string label = "owned") {
    Value value = Value::MakeMap();
    value.Set("value", Value{number});
    value.Set("label", Value{std::move(label)});
    return value;
}

schema::SchemaPtr DataSchema(const std::string& id) {
    using B = schema::SchemaBuilder;
    return B::Map(id).Field("value", B::Int()).Require().Field("label", B::String()).Build();
}

template<int Tag> ComponentDescriptor Descriptor(const std::string& name, const std::string& id) {
    using T = Native<Tag>;
    ComponentDescriptor d;
    d.name = name; d.schemaId = id; d.nativeType = typeid(T);
    d.size = sizeof(T); d.alignment = alignof(T);
    d.construct = [](void* p) noexcept { new (p) T(); };
    d.destroy = [](void* p) noexcept { static_cast<T*>(p)->~T(); };
    d.moveConstruct = [](void* dst, void* src) noexcept { new (dst) T(std::move(*static_cast<T*>(src))); };
    d.decode = [](const Value& v, void* p, const SourceLocation&) -> Status {
        auto& out = *static_cast<T*>(p);
        out.value = v["value"].AsInt(); out.label = v["label"].AsString();
        if (out.value == -99) return MakeError("runtime.test.decode", "codec rejected value", "use a value other than -99");
        return Status::Ok();
    };
    d.encode = [](const void* p, const SourceLocation&) -> Result<Value> {
        const auto& in = *static_cast<const T*>(p);
        return Data(in.value, in.label);
    };
    return d;
}

struct Fixture {
    std::shared_ptr<schema::Registry> schemas = std::make_shared<schema::Registry>();
    std::shared_ptr<ComponentRegistry> registry = std::make_shared<ComponentRegistry>();
    ComponentId a, b;
    std::unique_ptr<World> world;
    Fixture() {
        schemas->Register(DataSchema("test/a/1")); schemas->Register(DataSchema("test/b/1"));
        a = registry->Register(Descriptor<1>("a", "test/a/1")).Value();
        b = registry->Register(Descriptor<2>("b", "test/b/1")).Value();
        ALICE_ASSERT(registry->Freeze(schemas).IsOk(), "fixture registry must freeze");
        world = std::move(World::Create(registry).Value());
    }
};

const SourceLocation kSource{"scene.yaml", "actors[3].components.a", Mark{17, 4, 82}};

template<typename R> void CheckError(test::Context& aliceCtx, const R& result,
                                   const char* code, bool located = false) {
    ALICE_REQUIRE(result.IsErr());
    ALICE_CHECK_STR(result.Error().code, code);
    ALICE_CHECK(!result.Error().hint.empty());
    if (located) {
        ALICE_CHECK_STR(result.Error().file, kSource.file);
        ALICE_CHECK_STR(result.Error().path, kSource.path);
        ALICE_CHECK_EQ(result.Error().mark.line, kSource.mark.line);
        ALICE_CHECK_EQ(result.Error().mark.column, kSource.mark.column);
    }
}
}

ALICE_TEST(RuntimeECS, RegistryRejectsInvalidDescriptorsAndDuplicates) {
    ComponentRegistry registry;
    for (int which = 0; which < 10; ++which) {
        auto d = Descriptor<1>("a", "test/a/1");
        switch (which) {
            case 0: d.name.clear(); break;
            case 1: d.schemaId.clear(); break;
            case 2: d.size = 0; break;
            case 3: d.alignment = 3; break;
            case 4: d.size = 65; break;
            case 5: d.nativeType = typeid(void); break;
            case 6: d.construct = nullptr; break;
            case 7: d.destroy = nullptr; break;
            case 8: d.moveConstruct = nullptr; break;
            case 9: d.decode = nullptr; break;
        }
        CheckError(aliceCtx, registry.Register(d, kSource), "runtime.registry.invalid_descriptor", true);
    }
    auto missingEncode = Descriptor<1>("a", "test/a/1"); missingEncode.encode = nullptr;
    CheckError(aliceCtx, registry.Register(missingEncode), "runtime.registry.invalid_descriptor");
    ALICE_REQUIRE(registry.Register(Descriptor<1>("a", "test/a/1")).IsOk());
    CheckError(aliceCtx, registry.Register(Descriptor<2>("a", "test/b/1")), "runtime.registry.duplicate");
    CheckError(aliceCtx, registry.Register(Descriptor<2>("b", "test/a/1")), "runtime.registry.duplicate");
    CheckError(aliceCtx, registry.Register(Descriptor<1>("b", "test/b/1")), "runtime.registry.duplicate");
    CheckError(aliceCtx, registry.Find("missing"), "runtime.component.unknown");
}

ALICE_TEST(RuntimeECS, FreezeResolvesNestedReferencesAndCanRecover) {
    auto registry = std::make_shared<ComponentRegistry>();
    CheckError(aliceCtx, World::Create(registry), "runtime.registry.not_frozen");
    CheckError(aliceCtx, World::Create(nullptr), "runtime.registry.not_frozen");
    auto schemas = std::make_shared<schema::Registry>();
    schemas->Register(schema::SchemaBuilder::Map("test/a/1")
        .Field("nested", schema::SchemaBuilder::Ref("test/missing/1").Build()).Build());
    ALICE_REQUIRE(registry->Register(Descriptor<1>("a", "test/a/1")).IsOk());
    CheckError(aliceCtx, registry->Freeze(nullptr), "runtime.registry.schema_unresolved");
    CheckError(aliceCtx, registry->Freeze(schemas), "runtime.registry.schema_unresolved");
    ALICE_CHECK(!registry->IsFrozen());
    schemas->Register(DataSchema("test/missing/1"));
    ALICE_REQUIRE(registry->Freeze(schemas).IsOk());
    CheckError(aliceCtx, registry->Freeze(schemas), "runtime.registry.frozen");
    CheckError(aliceCtx, registry->Register(Descriptor<2>("b", "test/b/1")), "runtime.registry.frozen");
}

ALICE_TEST(RuntimeECS, ValidationAppendsDiagnosticsAndPreservesLocations) {
    Fixture f;
    DiagnosticBag bag;
    bag.Add(MakeError("test.previous", "earlier error", "earlier hint"));
    ALICE_CHECK(f.registry->Validate(f.a, Data(4), bag, kSource).IsOk());
    ALICE_CHECK_EQ(static_cast<u64>(bag.Size()), 1u);
    Value bad = Data(4); bad["value"] = Value{"wrong"}; bad["value"].mark = Mark{27, 8, 102};
    bad["label"] = Value{static_cast<i64>(7)}; bad["label"].mark = Mark{28, 9, 123};
    const auto result = f.registry->Validate(f.a, bad, bag, kSource);
    ALICE_REQUIRE(result.IsErr()); ALICE_REQUIRE(bag.Size() >= 3);
    ALICE_CHECK_STR(bag.Items()[0].code, "test.previous");
    ALICE_CHECK_STR(result.Error().code, bag.Items()[1].code);
    ALICE_CHECK_STR(bag.Items()[1].file, "scene.yaml");
    ALICE_CHECK_STR(bag.Items()[1].path, "actors[3].components.a.value");
    ALICE_CHECK_EQ(bag.Items()[1].mark.line, 27u);
    ALICE_CHECK(!bag.Items()[1].hint.empty());
    Fixture foreign;
    CheckError(aliceCtx, f.registry->Describe(foreign.a), "runtime.component.unknown");

    // Warnings must not exhaust validation before malformed data reaches the decoder.
    using B = schema::SchemaBuilder;
    auto schemas = std::make_shared<schema::Registry>();
    schemas->Register(B::Map("test/warnings/1").Field("items",
        B::Seq(B::Map().Field("old", B::Int()).Deprecate("value").Build()).Build()).Build());
    auto registry = std::make_shared<ComponentRegistry>();
    static int decodeCalls = 0;
    decodeCalls = 0;
    auto descriptor = Descriptor<3>("warnings", "test/warnings/1");
    descriptor.decode = [](const Value&, void*, const SourceLocation&) -> Status {
        ++decodeCalls;
        return Status::Ok();
    };
    const auto component = registry->Register(std::move(descriptor)).Value();
    ALICE_REQUIRE(registry->Freeze(schemas).IsOk());
    Value items = Value::MakeSeq();
    for (int i = 0; i < 200; ++i) {
        Value item = Value::MakeMap(); item.Set("old", Value{static_cast<i64>(1)});
        items.Push(std::move(item));
    }
    Value invalid{"not a map"}; invalid.mark = Mark{250, 6, 1024};
    items.Push(std::move(invalid));
    Value document = Value::MakeMap(); document.Set("items", std::move(items));
    DiagnosticBag all;
    const auto validated = registry->Validate(component, document, all, kSource);
    ALICE_CHECK(validated.IsErr());
    ALICE_CHECK_EQ(static_cast<u64>(all.Size()), 201u);
    if (validated.IsErr()) {
        ALICE_CHECK_STR(validated.Error().path, "actors[3].components.a.items[200]");
        ALICE_CHECK_STR(validated.Error().file, kSource.file);
        ALICE_CHECK_EQ(validated.Error().mark.line, 250u);
        ALICE_CHECK(!validated.Error().hint.empty());
    }
    auto world = std::move(World::Create(registry).Value());
    const auto entity = world->CreateEntity().Value();
    ALICE_CHECK(world->SetComponent(entity, component, document, kSource).IsErr());
    ALICE_CHECK_EQ(decodeCalls, 0);
}

ALICE_TEST(RuntimeECS, DeferredDestructionInvalidatesOnlyAtCommit) {
    Fixture f;
    const EntityId old = f.world->CreateEntity().Value();
    ALICE_REQUIRE(f.world->SetComponent(old, f.a, Data(7)).IsOk());
    ALICE_REQUIRE(f.world->DestroyEntity(old).IsOk());
    ALICE_REQUIRE(f.world->DestroyEntity(old).IsOk());
    {
        auto view = f.world->AcquireRead();
        ALICE_CHECK(view->IsAlive(old));
        ALICE_CHECK_EQ(view->Get<Native<1>>(old, f.a).Value()->value, 7);
    }
    ALICE_REQUIRE(f.world->CommitDestructions().IsOk());
    const EntityId fresh = f.world->CreateEntity().Value();
    ALICE_CHECK_EQ(old.index, fresh.index);
    ALICE_CHECK(old.generation != fresh.generation);
    CheckError(aliceCtx, f.world->SetComponent(old, f.a, Data(3), kSource), "runtime.entity.invalid", true);
    CheckError(aliceCtx, f.world->DestroyEntity(old, kSource), "runtime.entity.invalid", true);
    Fixture other;
    const EntityId alien = other.world->CreateEntity().Value();
    CheckError(aliceCtx, f.world->DestroyEntity(alien), "runtime.entity.invalid");
    auto view = f.world->AcquireRead();
    ALICE_CHECK(!view->IsAlive(old)); ALICE_CHECK(!view->IsAlive(alien)); ALICE_CHECK(!view->IsAlive({}));
    ALICE_CHECK(view->IsAlive(fresh));
    CheckError(aliceCtx, view->Get<Native<1>>(fresh, f.a), "runtime.component.missing");
}

ALICE_TEST(RuntimeECS, NativeStorageIsContiguousAlignedAndSurvivesSwapErase) {
    ALICE_CHECK_EQ(Native<1>::live, 0);
    {
        Fixture f;
        std::vector<EntityId> entities;
        for (i64 i = 0; i < 96; ++i) {
            entities.push_back(f.world->CreateEntity().Value());
            ALICE_REQUIRE(f.world->SetComponent(entities.back(), f.a, Data(i, Fmt("label {}", i))).IsOk());
        }
        ALICE_CHECK_EQ(Native<1>::live, 96);
        ALICE_REQUIRE(f.world->RemoveComponent(entities[1], f.a).IsOk());
        ALICE_CHECK_EQ(Native<1>::live, 95);
        auto view = f.world->AcquireRead();
        std::vector<std::uintptr_t> addresses;
        for (usize i = 0; i < entities.size(); ++i) {
            if (i == 1) continue;
            auto value = view->Get<Native<1>>(entities[i], f.a);
            ALICE_REQUIRE(value.IsOk());
            ALICE_CHECK_EQ(value.Value()->value, static_cast<i64>(i));
            ALICE_CHECK_STR(value.Value()->label, Fmt("label {}", static_cast<u64>(i)));
            addresses.push_back(reinterpret_cast<std::uintptr_t>(value.Value()));
            ALICE_CHECK_EQ(static_cast<u64>(addresses.back() % alignof(Native<1>)), 0u);
        }
        std::sort(addresses.begin(), addresses.end());
        for (usize i = 1; i < addresses.size(); ++i)
            ALICE_CHECK_EQ(static_cast<u64>(addresses[i] - addresses[i - 1]), static_cast<u64>(sizeof(Native<1>)));
    }
    ALICE_CHECK_EQ(Native<1>::live, 0);
}

ALICE_TEST(RuntimeECS, QueryMatchesIntersectionAndValidatesEveryRequirement) {
    Fixture f;
    std::vector<EntityId> expected;
    for (i64 i = 0; i < 128; ++i) {
        const auto entity = f.world->CreateEntity().Value();
        if (i % 2 == 0) ALICE_REQUIRE(f.world->SetComponent(entity, f.a, Data(i)).IsOk());
        if (i % 3 == 0) ALICE_REQUIRE(f.world->SetComponent(entity, f.b, Data(i)).IsOk());
        if (i % 6 == 0) expected.push_back(entity);
    }
    auto view = f.world->AcquireRead();
    const ComponentId required[] = {f.b, f.a, f.a};
    auto found = view->Query(required);
    ALICE_REQUIRE(found.IsOk()); ALICE_CHECK(found.Value() == expected);
    ALICE_CHECK_EQ(static_cast<u64>(view->Query({}).Value().size()), 128u);
    ALICE_CHECK(view->FindComponent("a").Value() == f.a);
    Fixture empty;
    auto emptyView = empty.world->AcquireRead();
    const ComponentId malformed[] = {empty.a, {}};
    CheckError(aliceCtx, emptyView->Query(malformed), "runtime.component.unknown");
    const ComponentId absent[] = {empty.a};
    ALICE_CHECK(emptyView->Query(absent).Value().empty());
}

ALICE_TEST(RuntimeECS, EveryMutationIsBlockedUntilLastReadLeaseIsReleased) {
    Fixture f;
    const auto entity = f.world->CreateEntity().Value();
    ALICE_REQUIRE(f.world->SetComponent(entity, f.a, Data(3)).IsOk());
    std::optional<WorldReadView> first(std::move(f.world->AcquireRead().Value()));
    std::optional<WorldReadView> second(std::move(f.world->AcquireRead().Value()));
    bool edited = false;
    CheckError(aliceCtx, f.world->CreateEntity(kSource), "runtime.world.read_locked", true);
    CheckError(aliceCtx, f.world->DestroyEntity(entity, kSource), "runtime.world.read_locked", true);
    CheckError(aliceCtx, f.world->CommitDestructions(), "runtime.world.read_locked");
    CheckError(aliceCtx, f.world->SetComponent(entity, f.a, Data(4), kSource), "runtime.world.read_locked", true);
    CheckError(aliceCtx, f.world->RemoveComponent(entity, f.a, kSource), "runtime.world.read_locked", true);
    CheckError(aliceCtx, f.world->EditComponent(entity, f.a, typeid(Native<1>),
        [](void*, void* context) noexcept { *static_cast<bool*>(context) = true; }, &edited, kSource),
        "runtime.world.read_locked", true);
    ALICE_CHECK(!edited);
    ALICE_REQUIRE(f.world->EncodeComponent(entity, f.a).IsOk());
    first = std::move(second);
    second.reset();
    CheckError(aliceCtx, f.world->CreateEntity(), "runtime.world.read_locked");
    first.reset();
    ALICE_REQUIRE(f.world->RemoveComponent(entity, f.a).IsOk());
    CheckError(aliceCtx, f.world->RemoveComponent(entity, f.a), "runtime.component.missing");
}

ALICE_TEST(RuntimeECS, ReadLeaseOwnsStorageAfterWorldAndRegistryDestruction) {
    Fixture f;
    const auto entity = f.world->CreateEntity().Value();
    ALICE_REQUIRE(f.world->SetComponent(entity, f.a, Data(23, "survives")).IsOk());
    std::optional<WorldReadView> view(std::move(f.world->AcquireRead().Value()));
    const Native<1>* pointer = view->Get<Native<1>>(entity, f.a).Value();
    f.world.reset(); f.registry.reset(); f.schemas.reset();
    ALICE_CHECK(view->IsAlive(entity)); ALICE_CHECK_STR(pointer->label, "survives");
    ALICE_CHECK(view->FindComponent("a").Value() == f.a);
    ALICE_CHECK_EQ(Native<1>::live, 1);
    view.reset();
    ALICE_CHECK_EQ(Native<1>::live, 0);
}

ALICE_TEST(RuntimeECS, DecodeFailureIsAtomicAndEditsCheckNativeType) {
    Fixture f;
    const auto entity = f.world->CreateEntity().Value();
    ALICE_REQUIRE(f.world->SetComponent(entity, f.a, Data(12, "before")).IsOk());
    CheckError(aliceCtx, f.world->SetComponent(entity, f.a, Data(-99, "partial"), kSource), "runtime.test.decode", true);
    Value invalid = Data(8); invalid["value"] = Value{"not an integer"};
    ALICE_REQUIRE(f.world->SetComponent(entity, f.a, invalid, kSource).IsErr());
    ALICE_CHECK_EQ(Native<1>::live, 1);
    auto encoded = f.world->EncodeComponent(entity, f.a);
    ALICE_REQUIRE(encoded.IsOk()); ALICE_CHECK(encoded.Value().DeepEquals(Data(12, "before")));
    auto edit = [](void* data, void*) noexcept { static_cast<Native<1>*>(data)->value += 2; };
    CheckError(aliceCtx, f.world->EditComponent(entity, f.a, typeid(int), edit, nullptr), "runtime.component.type_mismatch");
    CheckError(aliceCtx, f.world->EditComponent(entity, f.a, typeid(Native<1>), nullptr, nullptr), "runtime.component.invalid_edit");
    ALICE_REQUIRE(f.world->EditComponent(entity, f.a, typeid(Native<1>), edit, nullptr).IsOk());
    auto view = f.world->AcquireRead();
    ALICE_CHECK_EQ(view->Get<Native<1>>(entity, f.a).Value()->value, 14);
    CheckError(aliceCtx, view->Get<int>(entity, f.a), "runtime.component.type_mismatch");
}

ALICE_TEST(RuntimeECS, BatchDestroyKeepsSurvivorsAndReusedSlotsIndependent) {
    Fixture f;
    std::vector<EntityId> ids;
    for (i64 i = 0; i < 80; ++i) {
        ids.push_back(f.world->CreateEntity().Value());
        ALICE_REQUIRE(f.world->SetComponent(ids.back(), f.a, Data(i)).IsOk());
        ALICE_REQUIRE(f.world->SetComponent(ids.back(), f.b, Data(i)).IsOk());
        if (i % 2 == 0) ALICE_REQUIRE(f.world->DestroyEntity(ids.back()).IsOk());
    }
    ALICE_REQUIRE(f.world->CommitDestructions().IsOk());
    ALICE_REQUIRE(f.world->CommitDestructions().IsOk());
    ALICE_CHECK_EQ(Native<1>::live, 40); ALICE_CHECK_EQ(Native<2>::live, 40);
    for (int i = 0; i < 40; ++i) {
        auto id = f.world->CreateEntity().Value();
        ALICE_REQUIRE(f.world->SetComponent(id, f.b, Data(800)).IsOk());
    }
    auto view = f.world->AcquireRead();
    const ComponentId both[] = {f.a, f.b};
    auto survivors = view->Query(both).Value();
    ALICE_CHECK_EQ(static_cast<u64>(survivors.size()), 40u);
    for (usize i = 0; i < survivors.size(); ++i) {
        ALICE_CHECK(survivors[i] == ids[i * 2 + 1]);
        ALICE_CHECK_EQ(view->Get<Native<1>>(survivors[i], f.a).Value()->value, static_cast<i64>(i * 2 + 1));
    }
    for (usize i = 0; i < ids.size(); i += 2) ALICE_CHECK(!view->IsAlive(ids[i]));
}

ALICE_TEST(RuntimeECS, EditCallbackCannotReenterWorld) {
    Fixture f;
    auto id = f.world->CreateEntity().Value();
    ALICE_REQUIRE(f.world->SetComponent(id, f.a, Data(5)).IsOk());
    struct Context { World* world; bool readRejected = false; bool writeRejected = false; } context{f.world.get()};
    ALICE_REQUIRE(f.world->EditComponent(id, f.a, typeid(Native<1>), [](void*, void* p) noexcept {
        auto& c = *static_cast<Context*>(p);
        auto read = c.world->AcquireRead(); auto write = c.world->CreateEntity();
        c.readRejected = read.IsErr() && read.Error().code == "runtime.world.reentrant";
        c.writeRejected = write.IsErr() && write.Error().code == "runtime.world.reentrant";
    }, &context).IsOk());
    ALICE_CHECK(context.readRejected && context.writeRejected);
    ALICE_REQUIRE(f.world->AcquireRead().IsOk());
}
