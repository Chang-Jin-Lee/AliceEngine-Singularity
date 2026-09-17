// Physics input must survive render frames that contain no simulation step.
#include "TestFramework.h"
#include "AliceEditor/PlaySession.h"
#include "AliceEditor/PlatformerSession.h"
#include "Foundation/FileSystem.h"
#include <chrono>
#include <filesystem>
#include <cmath>
using namespace alice;
namespace {
const std::string kPhysicsScene =
    "schema: alice/scene/1\nname: Physics\nactors:\n"
    "  - name: Player\n    tags: [player]\n    transform: { position: [0, 2, 0] }\n"
    "    components:\n      character2d: { speed: 4, jumpSpeed: 7, gravityScale: 1 }\n"
    "      collider: { shape: box, size: [1, 1, 1] }\n"
    "  - name: Floor\n    transform: { position: [0, -0.5, 0] }\n"
    "    components:\n      collider: { shape: box, size: [20, 1, 1] }\n";
const std::string kPhysicsInput =
    "schema: alice/input/1\nname: Controls\nactions:\n"
    "  - { name: MoveX, type: axis, bindings: [key.ad] }\n"
    "  - { name: Jump, type: button, bindings: [key.space] }\n";
struct Files {
    std::filesystem::path dir;
    std::string scene, input;
    editor::DocumentModel model;
    Files() {
        dir = std::filesystem::temp_directory_path() / ("alice-physics-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(dir);
        auto text = (dir / "main.scene.yaml").generic_u8string(); scene.assign(reinterpret_cast<const char*>(text.data()), text.size());
        text = (dir / "input.yaml").generic_u8string(); input.assign(reinterpret_cast<const char*>(text.data()), text.size());
        ALICE_ASSERT(fs::WriteTextFile(scene, kPhysicsScene).IsOk(), "scene fixture");
        ALICE_ASSERT(fs::WriteTextFile(input, kPhysicsInput).IsOk(), "input fixture");
        ALICE_ASSERT(model.Open(scene).IsOk(), "open fixture");
    }
    ~Files() { std::error_code ec; std::filesystem::remove_all(dir, ec); }
};
}
ALICE_TEST(EditorPhysics, LandsJumpsAndPreservesSource) {
    Files files; editor::PlaySession play;
    ALICE_REQUIRE(files.model.IsValid());
    ALICE_REQUIRE(play.Start(files.model, files.input).IsOk());
    for (int i = 0; i < 120; ++i) ALICE_REQUIRE(play.Tick(1.0/60, {}).IsOk());
    const auto floorY = play.Actors()[0].position[1];
    ALICE_CHECK(std::abs(floorY - 0.5) < 0.001);
    ALICE_REQUIRE(play.Tick(1.0/120, {"key.space"}).IsOk());
    ALICE_REQUIRE(play.Tick(1.0/120, {}).IsOk());
    ALICE_CHECK(play.Actors()[0].position[1] > floorY + 0.05);
    for (int i = 0; i < 120; ++i) ALICE_REQUIRE(play.Tick(1.0/60, {"key.space"}).IsOk());
    ALICE_CHECK(std::abs(play.Actors()[0].position[1] - floorY) < 0.001);
    ALICE_REQUIRE(play.Tick(0.1, {"key.d"}).IsOk());
    ALICE_CHECK(std::abs(play.Actors()[0].position[0] - 0.4) < 1e-6);
    const auto edit = play.EditActor(0, play.Actors()[0]);
    ALICE_CHECK(edit.IsErr());
    ALICE_CHECK(!edit.Error().hint.empty());
    play.Stop();
    ALICE_CHECK_STR(files.model.Text(), kPhysicsScene);
    ALICE_CHECK(!files.model.IsDirty());
}
ALICE_TEST(EditorPhysics, RejectsUnsupportedShapeWithRepairLocation) {
    Files files; auto scene = kPhysicsScene;
    scene.replace(scene.find("shape: box"), 10, "shape: sphere");
    ALICE_REQUIRE(fs::WriteTextFile(files.scene, scene).IsOk());
    ALICE_REQUIRE(files.model.Open(files.scene).IsOk());
    editor::PlaySession play;
    const auto status = play.Start(files.model, files.input);
    ALICE_REQUIRE(status.IsErr());
    ALICE_CHECK_STR(status.Error().code, "physics.content.unsupported");
    ALICE_CHECK_STR(status.Error().file, files.scene);
    ALICE_CHECK(status.Error().path.find("collider.shape") != std::string::npos);
    ALICE_CHECK(status.Error().mark.Valid());
    ALICE_CHECK(!status.Error().hint.empty());
    ALICE_CHECK(!play.IsPlaying());
}
ALICE_TEST(EditorPhysics, FixedStepIgnoresRenderPartitionAndClampsPause) {
    Files files; editor::PlaySession a, b;
    ALICE_REQUIRE(a.Start(files.model, files.input).IsOk());
    ALICE_REQUIRE(b.Start(files.model, files.input).IsOk());
    for (int i=0; i<60; ++i) ALICE_REQUIRE(a.Tick(1.0/60, {"key.d"}).IsOk());
    for (int i=0; i<120; ++i) ALICE_REQUIRE(b.Tick(1.0/120, {"key.d"}).IsOk());
    ALICE_CHECK(a.Actors()[0].position == b.Actors()[0].position);
    const auto x = a.Actors()[0].position[0];
    ALICE_REQUIRE(a.Tick(100, {"key.d"}).IsOk());
    ALICE_CHECK(std::abs(a.Actors()[0].position[0] - x - 0.4) < 1e-6);
}
ALICE_TEST(EditorPhysics, InvalidTerrainPointsToTerrainInsteadOfPlayer) {
    Files files; auto scene = kPhysicsScene;
    scene.replace(scene.find("[20, 1, 1]"), 10, "[0.0000001, 1, 1]");
    ALICE_REQUIRE(fs::WriteTextFile(files.scene, scene).IsOk());
    ALICE_REQUIRE(files.model.Open(files.scene).IsOk());
    ALICE_REQUIRE(files.model.IsValid());
    editor::PlaySession play;
    const auto status = play.Start(files.model, files.input);
    ALICE_REQUIRE(status.IsErr());
    ALICE_CHECK_STR(status.Error().path, "actors[1].components.collider");
    ALICE_CHECK_STR(status.Error().file, files.scene);
    ALICE_CHECK(status.Error().mark.line >= 10);
    ALICE_CHECK(!status.Error().hint.empty());
}
ALICE_TEST(EditorPhysics, ReloadResetsBindingsAndFailurePreservesState) {
    Files files; editor::DocumentModel input;
    ALICE_REQUIRE(input.Open(files.input).IsOk());
    editor::PlatformerSession session;
    ALICE_REQUIRE(session.Load(files.model.Document(), 0, input.Document()).IsOk());
    ALICE_REQUIRE(session.Tick(0.1, {"key.d"}));
    auto remapped = kPhysicsInput;
    remapped.replace(remapped.find("key.ad"), 6, "key.qe");
    ALICE_REQUIRE(fs::WriteTextFile(files.input, remapped).IsOk());
    ALICE_REQUIRE(input.Open(files.input).IsOk());
    ALICE_REQUIRE(session.Load(files.model.Document(), 0, input.Document()).IsOk());
    auto position = session.Tick(0.1, {"key.d"});
    ALICE_REQUIRE(position);
    ALICE_CHECK_EQ(position->x, 0.0);
    position = session.Tick(0.1, {"key.e"});
    ALICE_REQUIRE(position);
    const auto before = position.Value();
    auto invalidInput = input.Document();
    invalidInput.root["actions"].At(0)["name"] = doc::Value{"Invalid"};
    ALICE_REQUIRE(session.Load(files.model.Document(), 0, invalidInput).IsErr());
    position = session.Tick(0, {});
    ALICE_REQUIRE(position);
    ALICE_CHECK_EQ(position->x, before.x);
    ALICE_CHECK_EQ(position->y, before.y);
}
ALICE_TEST(EditorPhysics, UnsupportedOptionsHaveRepairDiagnostics) {
    for (const auto& [from, to] : std::vector<std::pair<std::string,std::string>>{
        {"shape: box, size: [20, 1, 1]", "shape: box, size: [20, 1, 1], isTrigger: true"},
        {"shape: box, size: [20, 1, 1]", "shape: box, size: [20, 1, 1], layer: ignored"},
        {"position: [0, -0.5, 0]", "position: [0, -0.5, 0], rotation: [0, 0, 10]"},
        {"position: [0, -0.5, 0]", "position: [0, -0.5, 1]"},
        {"name: Floor", "name: Floor\n    active: false"},
        {"name: Floor", "name: Floor\n    behaviors: [not-executed.behavior.yaml]"},
        {"character2d: { speed: 4", "rigidbody: {}\n      character2d: { speed: 4"},
        {"name: Physics", "name: Physics\nenvironment: { gravity: [1, -9.81, 0] }"}
    }) {
        Files files; auto scene = kPhysicsScene;
        scene.replace(scene.find(from), from.size(), to);
        ALICE_REQUIRE(fs::WriteTextFile(files.scene, scene).IsOk());
        ALICE_REQUIRE(files.model.Open(files.scene).IsOk());
        ALICE_REQUIRE(files.model.IsValid());
        editor::PlaySession play; const auto status = play.Start(files.model, files.input);
        ALICE_REQUIRE(status.IsErr());
        ALICE_CHECK_STR(status.Error().code, "physics.content.unsupported");
        ALICE_CHECK_STR(status.Error().file, files.scene);
        ALICE_CHECK(status.Error().mark.Valid());
        ALICE_CHECK(!status.Error().path.empty() && !status.Error().hint.empty());
    }
}
ALICE_TEST(EditorPhysics, AirJumpDoesNotResetVerticalSpeed) {
    Files files; editor::PlaySession a,b;
    ALICE_REQUIRE(a.Start(files.model, files.input).IsOk());
    ALICE_REQUIRE(b.Start(files.model, files.input).IsOk());
    for (int i=0;i<120;++i) { ALICE_REQUIRE(a.Tick(1.0/60, {}).IsOk()); ALICE_REQUIRE(b.Tick(1.0/60, {}).IsOk()); }
    ALICE_REQUIRE(a.Tick(1.0/60, {"key.space"}).IsOk());
    ALICE_REQUIRE(b.Tick(1.0/60, {"key.space"}).IsOk());
    ALICE_REQUIRE(a.Tick(1.0/60, {}).IsOk()); ALICE_REQUIRE(b.Tick(1.0/60, {}).IsOk());
    ALICE_REQUIRE(a.Tick(1.0/60, {"key.space"}).IsOk()); ALICE_REQUIRE(b.Tick(1.0/60, {}).IsOk());
    ALICE_CHECK(a.Actors()[0].position == b.Actors()[0].position);
}
ALICE_TEST(EditorPhysics, CharacterOnNonPlayerDoesNotSilentlyUseTransformPreview) {
    Files files; auto scene = kPhysicsScene;
    scene.replace(scene.find("    tags: [player]\n"), 19, "");
    scene.replace(scene.find("name: Floor"), 11, "name: Floor\n    tags: [player]");
    ALICE_REQUIRE(fs::WriteTextFile(files.scene, scene).IsOk());
    ALICE_REQUIRE(files.model.Open(files.scene).IsOk());
    ALICE_REQUIRE(files.model.IsValid());
    editor::PlaySession play; const auto status = play.Start(files.model, files.input);
    ALICE_REQUIRE(status.IsErr());
    ALICE_CHECK_STR(status.Error().code, "physics.content.unsupported");
    ALICE_CHECK(status.Error().path.find("character2d") != std::string::npos);
}
