// Reject unsupported content instead of giving a successful but misleading physics preview.
#include "AliceEditor/PlatformerSession.h"
#include "Foundation/Log.h"
#include "Foundation/Profiler.h"
#include <algorithm>
#include <cmath>
namespace alice::editor {
namespace {
using runtime::physics::Box;
using runtime::physics::Vec2;
Diagnostic Error(const doc::Document& doc, const doc::Value& value, const std::string& path,
                 const char* message, const char* hint) {
    auto d = MakeError("physics.content.unsupported", message, hint);
    d.file = doc.path; d.path = path; d.mark = value.mark.Valid() ? value.mark : Mark{1,1,0};
    return d;
}
double Number(const doc::Value& values, usize axis, double fallback) { return values.At(axis).AsFloat(fallback); }
}
Status PlatformerSession::Load(const doc::Document& scene, usize player, const doc::Document& input) {
    PlatformerSession next;
    ALICE_TRY(next.LoadDocument(scene, player, input));
    *this = std::move(next);
    return Status::Ok();
}
Status PlatformerSession::LoadDocument(const doc::Document& scene, usize player, const doc::Document& input) {
    const auto& actors = scene.root["actors"];
    const auto& gravity = scene.root["environment"]["gravity"];
    if (Number(gravity,0,0) != 0 || Number(gravity,2,0) != 0 ||
        !std::isfinite(Number(gravity,1,-9.81)) || Number(gravity,1,-9.81) < -100 || Number(gravity,1,-9.81) > 0)
        return Error(scene, gravity, "environment.gravity", "Platformer gravity must point down the Y axis",
            "Use [0, y, 0] with y between -100 and 0 metres per second squared");
    std::vector<Box> terrain;
    for (usize i=0; i<actors.Size(); ++i) {
        const auto& actor = actors.At(i); const auto& components = actor["components"];
        const auto base = "actors[" + std::to_string(i) + "]";
        if (actor["children"].Size() != 0) return Error(scene, actor["children"], base+".children",
            "Platformer preview requires top-level actors", "Flatten the actor hierarchy before using this preview");
        if (actor["behaviors"].Size() != 0) return Error(scene, actor["behaviors"], base+".behaviors",
            "General behavior execution is not available", "Use character2d settings and MoveX/Jump input for this preview");
        if (!actor["active"].AsBool(true)) return Error(scene, actor["active"], base+".active",
            "Inactive actors are not supported in this preview", "Remove this actor or set active: true");
        if (components.Has("rigidbody")) return Error(scene, components["rigidbody"], base+".components.rigidbody",
            "General rigid bodies are not supported by the platformer solver", "Use character2d on the player and only collider on fixed terrain");
        if (i != player && components.Has("character2d")) return Error(scene, components["character2d"], base+".components.character2d",
            "The preview supports one controlled character", "Keep character2d only on the actor tagged player");
        const auto& collider = components["collider"];
        if (collider.IsNull()) {
            if (i == player) return Error(scene, actor, base+".components.collider", "The player needs a box collider",
                "Add collider: { shape: box, size: [1, 1, 1] } beside character2d");
            continue;
        }
        const auto path = base+".components.collider";
        if (collider["shape"].AsString() != "box") return Error(scene, collider["shape"], path+".shape",
            "Only axis-aligned boxes are supported", "Set shape: box and positive size: [x, y, z]");
        if (collider["isTrigger"].AsBool(false) || (collider.Has("layer") && collider["layer"].AsString() != "default") || collider.Has("asset") || collider.Has("radius") || collider.Has("height"))
            return Error(scene, collider, path, "This collider uses unsupported platformer options",
                "Use a solid box on the default layer; remove asset, radius and height");
        const auto& transform = actor["transform"];
        for (usize axis=0; axis<3; ++axis) {
            if (Number(transform["rotation"],axis,0) != 0) return Error(scene, transform["rotation"], base+".transform.rotation",
                "Rotated colliders are not supported", "Use rotation: [0, 0, 0]");
            if (!std::isfinite(Number(collider["size"],axis,1)) || Number(collider["size"],axis,1) <= 0)
                return Error(scene, collider["size"], path+".size", "Collider size must be positive and finite", "Use positive finite dimensions on every axis");
        }
        if (Number(transform["position"],2,0) != 0 || Number(collider["center"],2,0) != 0)
            return Error(scene, actor, base+".transform", "Platformer colliders must lie in the XY plane", "Set position.z and collider.center.z to 0");
        const Vec2 scale{Number(transform["scale"],0,1),Number(transform["scale"],1,1)};
        const Vec2 offset{Number(collider["center"],0,0)*scale.x,Number(collider["center"],1,0)*scale.y};
        Box box{{Number(transform["position"],0,0)+offset.x,Number(transform["position"],1,0)+offset.y},
            {Number(collider["size"],0,1)*scale.x*0.5,Number(collider["size"],1,1)*scale.y*0.5}};
        ALICE_TRY(runtime::physics::CheckBox(box, {scene.path, path, collider.mark}));
        if (i == player) {
            m_body = box; m_offset = offset;
            m_source = {scene.path, path, collider.mark.Valid() ? collider.mark : Mark{1,1,0}};
        } else terrain.push_back(box);
    }
    const auto& config = actors.At(player)["components"]["character2d"];
    m_speed = config["speed"].AsFloat(4); m_jumpSpeed = config["jumpSpeed"].AsFloat(7);
    m_gravity = Number(gravity,1,-9.81)*config["gravityScale"].AsFloat(1);
    ALICE_TRY(m_world.Build(terrain, m_source));
    auto start = m_world.Move(m_body, {}, m_source); if (!start) return start.Error();
    m_body = start->box; m_grounded = start->grounded;
    if (input.SchemaId() != "alice/input/1") return Error(input, input.root["schema"], "schema", "Expected an input map", "Use schema: alice/input/1");
    bool move = false, jump = false;
    const auto& actions = input.root["actions"];
    for (usize i=0; i<actions.Size(); ++i) {
        const auto& action = actions.At(i); const auto path = "actions["+std::to_string(i)+"]";
        const auto name = action["name"].AsString();
        const bool isMove = name == "MoveX";
        if ((!isMove && name != "Jump") || (isMove ? move : jump)) return Error(input, action["name"], path+".name",
            "Unsupported or duplicate platformer action", "Define exactly one MoveX axis and one Jump button");
        if (action["type"].AsString() != (isMove ? "axis" : "button")) return Error(input, action["type"], path+".type",
            "Wrong platformer action type", "Use axis for MoveX and button for Jump");
        const double deadzone = action["deadzone"].AsFloat(0);
        if (!std::isfinite(deadzone) || deadzone < 0 || deadzone >= 1) return Error(input, action["deadzone"], path+".deadzone",
            "Keyboard deadzone must be below one", "Use a value from zero inclusive to one exclusive");
        if (isMove) move = true; else jump = true;
        const auto& bindings = action["bindings"];
        if (bindings.Size() == 0) return Error(input, bindings, path+".bindings", "An input binding is required", "Use key.ad for MoveX and key.space for Jump");
        for (usize j=0; j<bindings.Size(); ++j) {
            const auto token = bindings.At(j).AsString();
            if (isMove && (token == "key.ad" || token == "key.qe"))
                m_moveBindings.push_back(token == "key.ad" ? std::pair<std::string,std::string>{"key.a","key.d"} : std::pair<std::string,std::string>{"key.q","key.e"});
            else if (!isMove && (token == "key.space" || token == "key.w")) m_jumpBindings.push_back(token);
            else return Error(input, bindings.At(j), path+".bindings["+std::to_string(j)+"]", "Unsupported platformer key binding",
                "Use key.ad or key.qe for MoveX, key.space or key.w for Jump");
        }
    }
    if (!move || !jump) return Error(input, actions, "actions", "Missing platformer actions", "Define MoveX (axis) and Jump (button)");
    return Status::Ok();
}
Result<Vec2> PlatformerSession::Tick(double dt, const std::vector<std::string>& keys) {
    ALICE_PROFILE_ZONE("Physics.Step");
    constexpr double step = 1.0/60;
    const auto held = [&](const std::string& key) { return std::find(keys.begin(),keys.end(),key) != keys.end(); };
    bool jumping = false; for (const auto& key : m_jumpBindings) jumping = jumping || held(key);
    m_jumpPending = m_jumpPending || (jumping && !m_jumpHeld); m_jumpHeld = jumping;
    double axis = 0; for (const auto& pair : m_moveBindings) axis += double(held(pair.second))-double(held(pair.first));
    axis = std::clamp(axis,-1.0,1.0);
    if (dt > 0.1) ALICE_LOG_WARN("physics", "physics.time.clamped").Msg("Platformer frame time was clamped").F("seconds",dt).F("limit",0.1);
    m_accumulator += std::min(dt,0.1);
    for (int i=0; i<6 && m_accumulator+1e-12>=step; ++i) {
        double velocity = m_velocityY;
        if (m_jumpPending && m_grounded) velocity = m_jumpSpeed;
        velocity = std::max(-100.0,velocity+m_gravity*step);
        const auto result = m_world.Move(m_body,{axis*m_speed*step,velocity*step},m_source);
        if (!result) return result.Error();
        m_body = result->box; m_grounded = result->grounded;
        m_velocityY = result->blockedY ? 0 : velocity;
        m_jumpPending = false; m_accumulator = std::max(0.0,m_accumulator-step);
    }
    return Vec2{m_body.center.x-m_offset.x,m_body.center.y-m_offset.y};
}
}
