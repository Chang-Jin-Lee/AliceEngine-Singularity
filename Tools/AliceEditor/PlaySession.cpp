// SPDX-License-Identifier: MIT
// Preview state belongs to a disposable ECS world, never to the editable document.
#include "AliceEditor/PlaySession.h"
#include "Runtime/World.h"
#include "Schema/Registry.h"
#include <algorithm>
#include <cmath>
#include <new>

namespace alice::editor {
namespace {
using runtime::SourceLocation;
struct Transform {
    std::array<double, 3> position{0, 0, 0}, rotation{0, 0, 0}, scale{1, 1, 1};
};
struct Binding { std::string negative, positive; };
constexpr std::array<const char*, 5> kActions{"MoveX", "MoveZ", "MoveY", "RotateY", "Scale"};

Diagnostic Error(const char* code, const char* message, const SourceLocation& source, const char* hint) {
    auto d = MakeError(code, message, hint);
    d.file = source.file; d.path = source.path;
    d.mark = source.mark.Valid() ? source.mark : Mark{1, 1, 0};
    return d;
}
SourceLocation Location(const doc::Document& document, std::string path, const doc::Value& value) {
    return {document.path.empty() ? "<scene>" : document.path, std::move(path), value.mark};
}
bool ValidTransform(const Transform& transform) {
    for (usize i = 0; i < 3; ++i) {
        if (!std::isfinite(transform.position[i]) || std::abs(transform.position[i]) > 10000 ||
            !std::isfinite(transform.rotation[i]) || std::abs(transform.rotation[i]) > 360000 ||
            !std::isfinite(transform.scale[i]) || transform.scale[i] < 0.05 || transform.scale[i] > 100)
            return false;
    }
    return true;
}
Diagnostic TransformError(const SourceLocation& source) {
    return Error("editor.play.transform_invalid", "Transform is outside the preview's finite bounds", source,
        "Use finite positions within +/-10000, rotations within +/-360000 degrees, and scale axes from 0.05 to 100");
}
runtime::ComponentDescriptor TransformDescriptor() {
    runtime::ComponentDescriptor descriptor;
    descriptor.name = "transform"; descriptor.schemaId = "alice/component/transform/1";
    descriptor.nativeType = typeid(Transform); descriptor.size = sizeof(Transform);
    descriptor.alignment = alignof(Transform);
    descriptor.construct = [](void* dst) noexcept { new (dst) Transform(); };
    descriptor.destroy = [](void* object) noexcept { static_cast<Transform*>(object)->~Transform(); };
    descriptor.moveConstruct = [](void* dst, void* src) noexcept {
        new (dst) Transform(std::move(*static_cast<Transform*>(src)));
    };
    descriptor.decode = [](const doc::Value& value, void* dst, const SourceLocation& source) -> Status {
        auto& transform = *static_cast<Transform*>(dst);
        for (usize i = 0; i < 3; ++i) {
            transform.position[i] = value["position"].At(i).AsFloat(0);
            transform.rotation[i] = value["rotation"].At(i).AsFloat(0);
            transform.scale[i] = value["scale"].At(i).AsFloat(1);
        }
        if (!ValidTransform(transform)) return TransformError(source);
        return Status::Ok();
    };
    descriptor.encode = [](const void* object, const SourceLocation&) -> Result<doc::Value> {
        const auto& transform = *static_cast<const Transform*>(object);
        auto value = doc::Value::MakeMap();
        for (const auto& [name, vector] : {
            std::pair{"position", &transform.position}, std::pair{"rotation", &transform.rotation},
            std::pair{"scale", &transform.scale}}) {
            auto values = doc::Value::MakeSeq();
            for (const auto number : *vector) values.Push(doc::Value(number));
            value.Set(name, std::move(values));
        }
        return value;
    };
    return descriptor;
}
Binding KeyPair(const std::string& binding) {
    if (binding == "key.ad") return {"key.a", "key.d"};
    if (binding == "key.ws") return {"key.w", "key.s"};
    if (binding == "key.qe") return {"key.q", "key.e"};
    if (binding == "key.fr") return {"key.f", "key.r"};
    if (binding == "key.rf") return {"key.r", "key.f"};
    if (binding == "key.control_space") return {"key.control", "key.space"};
    return {};
}
}

struct PlaySession::Impl {
    // This snapshot owns repair locations and makes the source's lifetime irrelevant to play.
    doc::Document original;
    std::unique_ptr<runtime::World> world;
    runtime::ComponentId transform;
    std::vector<runtime::EntityId> entities;
    std::vector<std::string> names;
    std::array<std::vector<Binding>, 5> bindings;
    usize player = 0;
    SourceLocation ActorSource(usize index, const char* field = "transform") const {
        const auto& actors = original.root["actors"];
        if (index >= actors.Size()) return Location(original, "actors", actors);
        const auto& actor = actors.At(index);
        const auto& value = actor[field];
        return Location(original, "actors[" + std::to_string(index) + "]." + field,
            value.mark.Valid() ? value : actor);
    }
};
PlaySession::PlaySession() = default;
PlaySession::~PlaySession() = default;

Status PlaySession::Start(const DocumentModel& document, const std::string& inputPath) {
    const auto source = Location(document.Document(), "actors", document.Document().root["actors"]);
    if (IsPlaying()) return Error("editor.play.already_started", "Preview is already running", source,
        "Stop the current preview before starting another scene");
    if (!document.IsValid() || !document.IsScene()) return Error("editor.play.scene_invalid",
        "Preview requires a valid scene document", source, "Open an alice/scene/1 document and fix its validation errors");
    auto next = std::make_unique<Impl>();
    next->original = document.Document();
    usize playerCount = 0;
    const auto& actorValues = next->original.root["actors"];
    for (usize i = 0; i < actorValues.Size(); ++i) {
        for (const auto& tag : actorValues.At(i)["tags"].Items()) {
            if (tag.AsString() == "player") { next->player = i; ++playerCount; break; }
        }
    }
    if (playerCount == 0) return Error("editor.play.player_missing", "No player actor was found", source,
        "Add tags: [player] to exactly one top-level actor in this scene");
    if (playerCount > 1) return Error("editor.play.player_multiple", "More than one player actor was found", source,
        "Keep the player tag on exactly one top-level actor");

    DocumentModel input;
    const auto opened = input.Open(inputPath);
    if (!opened) return Error("editor.play.input_unreadable", "Could not read the preview input map",
        {inputPath, "document", {1, 1, 0}}, "Create a readable alice/input/1 document at input/default.input.yaml in the project");
    if (!input.IsValid()) {
        for (const auto& d : input.Diagnostics().Items()) if (d.severity == Severity::Error) {
            auto located = d;
            if (located.file.empty()) located.file = inputPath;
            if (located.path.empty()) located.path = "document";
            if (!located.mark.Valid()) located.mark = {1, 1, 0};
            if (located.hint.empty()) located.hint = located.code == "doc.parse.bad_escape"
                ? "Correct the quoted string escape; a Unicode escape requires four hexadecimal digits after \\u"
                : "Correct the indicated input document field or YAML syntax and validate against alice/input/1 again";
            return located;
        }
    }
    const auto& inputDocument = input.Document();
    if (inputDocument.SchemaId() != "alice/input/1") return Error("editor.play.input_invalid",
        "Preview requires an input map", Location(inputDocument, "schema", inputDocument.root["schema"]),
        "Use schema: alice/input/1 and define MoveX, MoveZ, MoveY, RotateY and Scale axis actions");
    const auto& actions = inputDocument.root["actions"];
    std::array<bool, 5> seen{};
    for (usize index = 0; index < actions.Size(); ++index) {
        const auto& action = actions.At(index);
        const auto path = "actions[" + std::to_string(index) + "]";
        const auto found = std::find(kActions.begin(), kActions.end(), action["name"].AsString());
        if (found == kActions.end()) return Error("editor.play.action_unsupported",
            "This action is not supported by the cube preview", Location(inputDocument, path + ".name", action["name"]),
            "Use only MoveX, MoveZ, MoveY, RotateY and Scale in this preview input map");
        const auto slot = static_cast<usize>(found - kActions.begin());
        if (seen[slot]) return Error("editor.play.action_duplicate", "Preview action is defined more than once",
            Location(inputDocument, path + ".name", action["name"]), "Keep one definition per preview action");
        seen[slot] = true;
        if (action["type"].AsString() != "axis") return Error("editor.play.action_type",
            "Preview actions require axis input", Location(inputDocument, path + ".type", action["type"]),
            "Set type: axis for each preview action");
        const auto deadzone = action["deadzone"].AsFloat(0);
        if (!std::isfinite(deadzone) || deadzone < 0 || deadzone >= 1) return Error("editor.play.deadzone_invalid",
            "Keyboard deadzone must be finite and below one", Location(inputDocument, path + ".deadzone", action["deadzone"]),
            "Use a deadzone from 0 inclusive to 1 exclusive for digital key pairs");
        const auto& bindings = action["bindings"];
        for (usize bindingIndex = 0; bindingIndex < bindings.Size(); ++bindingIndex) {
            auto binding = KeyPair(bindings.At(bindingIndex).AsString());
            if (binding.negative.empty()) return Error("editor.play.binding_unsupported",
                "This binding is not supported by the cube preview",
                Location(inputDocument, path + ".bindings[" + std::to_string(bindingIndex) + "]", bindings.At(bindingIndex)),
                "Use key.ad, key.ws, key.control_space, key.qe, key.fr or key.rf");
            next->bindings[slot].push_back(std::move(binding));
        }
    }
    if (std::find(seen.begin(), seen.end(), false) != seen.end()) return Error("editor.play.action_missing",
        "The preview input map is missing required actions", Location(inputDocument, "actions", actions),
        "Define MoveX, MoveZ, MoveY, RotateY and Scale as axis actions with supported key pairs");

    auto schemas = std::make_shared<schema::Registry>();
    schema::RegisterCoreSchemas(*schemas);
    auto components = std::make_shared<runtime::ComponentRegistry>();
    auto registered = components->Register(TransformDescriptor(), source);
    if (!registered) return registered.Error();
    next->transform = registered.Value();
    ALICE_TRY(components->Freeze(schemas));
    auto world = runtime::World::Create(components);
    if (!world) return world.Error();
    next->world = std::move(world.Value());
    for (usize index = 0; index < actorValues.Size(); ++index) {
        const auto actorSource = next->ActorSource(index);
        auto entity = next->world->CreateEntity(actorSource);
        if (!entity) return entity.Error();
        const auto& transform = actorValues.At(index)["transform"];
        ALICE_TRY(next->world->SetComponent(entity.Value(), next->transform,
            transform.IsNull() ? doc::Value::MakeMap() : transform, actorSource));
        next->entities.push_back(entity.Value());
        next->names.push_back(actorValues.At(index)["name"].AsString());
    }
    m_impl = std::move(next);
    return Status::Ok();
}
void PlaySession::Stop() { m_impl.reset(); }
bool PlaySession::IsPlaying() const { return m_impl != nullptr; }

std::vector<Actor> PlaySession::Actors() const {
    std::vector<Actor> actors;
    if (!m_impl) return actors;
    auto lease = m_impl->world->AcquireRead();
    if (!lease) return actors;
    actors.reserve(m_impl->entities.size());
    for (usize index = 0; index < m_impl->entities.size(); ++index) {
        auto value = lease.Value().Get<Transform>(m_impl->entities[index], m_impl->transform);
        if (!value) return {};
        actors.push_back({m_impl->names[index], value.Value()->position, value.Value()->rotation, value.Value()->scale});
    }
    return actors;
}
Status PlaySession::EditActor(usize index, const Actor& actor) {
    if (!m_impl) return Error("editor.play.not_started", "There is no running preview",
        {"<preview>", "actors", {1, 1, 0}}, "Start a valid scene preview before editing runtime transforms");
    if (index >= m_impl->entities.size()) return Error("editor.play.actor_invalid", "Preview actor does not exist",
        m_impl->ActorSource(index), "Select an existing actor from the preview hierarchy");
    const auto source = m_impl->ActorSource(index);
    if (actor.name.empty()) return Error("editor.play.actor_name_empty", "Preview actor name cannot be empty", m_impl->ActorSource(index, "name"),
        "Enter a nonempty actor name");
    Transform transform{actor.position, actor.rotation, actor.scale};
    if (!ValidTransform(transform)) return TransformError(source);
    ALICE_TRY(m_impl->world->EditComponent(m_impl->entities[index], m_impl->transform, typeid(Transform),
        [](void* object, void* context) noexcept { *static_cast<Transform*>(object) = *static_cast<const Transform*>(context); },
        &transform, source));
    m_impl->names[index] = actor.name;
    return Status::Ok();
}
Status PlaySession::Tick(double dt, const std::vector<std::string>& heldKeys) {
    if (!m_impl) return Error("editor.play.not_started", "There is no running preview",
        {"<preview>", "actors", {1, 1, 0}}, "Start a valid scene preview before ticking runtime transforms");
    const auto source = m_impl->ActorSource(m_impl->player);
    if (!std::isfinite(dt) || dt < 0) return Error("editor.play.time_invalid", "Preview delta time must be finite and nonnegative",
        source, "Supply an elapsed time in seconds that is finite and at least zero");
    dt = std::min(dt, 0.1);
    std::array<double, 5> axes{};
    const auto held = [&heldKeys](const std::string& key) { return std::find(heldKeys.begin(), heldKeys.end(), key) != heldKeys.end(); };
    for (usize axis = 0; axis < axes.size(); ++axis) {
        for (const auto& binding : m_impl->bindings[axis])
            axes[axis] += static_cast<double>(held(binding.positive)) - static_cast<double>(held(binding.negative));
        axes[axis] = std::clamp(axes[axis], -1.0, 1.0);
    }
    const auto length = std::sqrt(axes[0]*axes[0] + axes[1]*axes[1] + axes[2]*axes[2]);
    if (length > 1) for (usize axis = 0; axis < 3; ++axis) axes[axis] /= length;
    struct Delta { std::array<double, 3> position; double yaw, scale; };
    Delta delta{{axes[0] * 4 * dt, axes[2] * 4 * dt, axes[1] * 4 * dt}, axes[3] * 90 * dt, axes[4] * dt};
    return m_impl->world->EditComponent(m_impl->entities[m_impl->player], m_impl->transform, typeid(Transform),
        [](void* object, void* context) noexcept {
            auto& transform = *static_cast<Transform*>(object);
            const auto& change = *static_cast<const Delta*>(context);
            for (usize i = 0; i < 3; ++i) {
                transform.position[i] = std::clamp(transform.position[i] + change.position[i], -10000.0, 10000.0);
                transform.scale[i] = std::clamp(transform.scale[i] + change.scale, 0.05, 100.0);
            }
            transform.rotation[1] = std::clamp(transform.rotation[1] + change.yaw, -360000.0, 360000.0);
        }, &delta, source);
}
} // namespace alice::editor
