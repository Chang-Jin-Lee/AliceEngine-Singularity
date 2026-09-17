// SPDX-License-Identifier: MIT
// A preview must visibly respond to content bindings without changing the source scene.
#include "TestFramework.h"
#include "AliceEditor/PlaySession.h"
#include "Foundation/FileSystem.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <limits>

using namespace alice;
namespace {
constexpr const char* kScene =
    "schema: alice/scene/1\nname: Preview\nactors:\n"
    "  - name: Player\n    tags: [player]\n    transform: { position: [0, 1, 0] }\n"
    "  - name: Ground\n    transform: { position: [0, -1, 0], scale: [6, 1, 6] }\n";
constexpr const char* kInput =
    "schema: alice/input/1\nname: Preview\nactions:\n"
    "  - { name: MoveX, type: axis, bindings: [key.ad] }\n"
    "  - { name: MoveZ, type: axis, bindings: [key.ws] }\n"
    "  - { name: MoveY, type: axis, bindings: [key.control_space] }\n"
    "  - { name: RotateY, type: axis, bindings: [key.qe] }\n"
    "  - { name: Scale, type: axis, bindings: [key.fr] }\n";
struct PlayFiles {
    std::filesystem::path directory;
    std::string scenePath, inputPath;
    editor::DocumentModel model;
    PlayFiles() {
        std::error_code ec;
        directory = std::filesystem::temp_directory_path(ec) / ("alice-play-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(directory, ec);
        auto utf8 = (directory / "scene.yaml").generic_u8string();
        scenePath.assign(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        utf8 = (directory / "input.yaml").generic_u8string();
        inputPath.assign(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        ALICE_ASSERT(fs::WriteTextFile(scenePath, kScene).IsOk(), "scene fixture must exist");
        ALICE_ASSERT(fs::WriteTextFile(inputPath, kInput).IsOk(), "input fixture must exist");
        ALICE_ASSERT(model.Open(scenePath).IsOk() && model.IsValid(), "scene fixture must validate");
    }
    ~PlayFiles() { std::error_code ec; std::filesystem::remove_all(directory, ec); }
};
void CheckPlayError(test::Context& aliceCtx, const Status& status, const std::string& file,
                    const char* code) {
    ALICE_REQUIRE(status.IsErr());
    ALICE_CHECK_STR(status.Error().code, code);
    ALICE_CHECK_STR(status.Error().file, file);
    ALICE_CHECK(status.Error().mark.Valid());
    ALICE_CHECK(!status.Error().path.empty());
    ALICE_CHECK(!status.Error().hint.empty());
}
}

ALICE_TEST(EditorPlay, MovesOnlyTaggedPlayerAndStopLeavesSourceUnchanged) {
    PlayFiles files; editor::PlaySession play;
    ALICE_REQUIRE(play.Start(files.model, files.inputPath).IsOk());
    ALICE_REQUIRE(play.IsPlaying());
    ALICE_REQUIRE(play.Tick(0.1, {"key.d"}).IsOk());
    const auto actors = play.Actors(); ALICE_REQUIRE(actors.size() == 2);
    ALICE_CHECK(std::abs(actors[0].position[0] - 0.4) < 1e-9);
    ALICE_CHECK_EQ(actors[1].position[0], 0.0);
    ALICE_CHECK_STR(files.model.Text(), kScene);
    play.Stop(); ALICE_CHECK(!play.IsPlaying()); ALICE_CHECK(play.Actors().empty());
    ALICE_CHECK(!files.model.IsDirty()); ALICE_CHECK_STR(files.model.Text(), kScene);
    ALICE_CHECK_STR(fs::ReadTextFile(files.scenePath).Value(), kScene);
    ALICE_REQUIRE(play.Start(files.model, files.inputPath).IsOk());
    const auto restarted = play.Actors();
    ALICE_CHECK_EQ(restarted[0].position[0], 0.0);
}

ALICE_TEST(EditorPlay, NormalizesMotionClampsTimeAndClearsUnfocusedInput) {
    PlayFiles files; editor::PlaySession play;
    ALICE_REQUIRE(play.Start(files.model, files.inputPath).IsOk());
    ALICE_REQUIRE(play.IsPlaying());
    ALICE_REQUIRE(play.Tick(100.0, {"key.d", "key.w", "key.space"}).IsOk());
    auto player = play.Actors()[0];
    const auto x = player.position[0], y = player.position[1] - 1.0, z = player.position[2];
    ALICE_CHECK(std::abs(std::sqrt(x*x + y*y + z*z) - 0.4) < 1e-9);
    ALICE_CHECK(x > 0 && y > 0 && z < 0);
    ALICE_REQUIRE(play.Tick(0.1, {}).IsOk());
    ALICE_CHECK(play.Actors()[0].position == player.position);
    ALICE_REQUIRE(play.Tick(0.1, {"key.a", "key.d"}).IsOk());
    ALICE_CHECK(play.Actors()[0].position == player.position);
    CheckPlayError(aliceCtx, play.Tick(-1, {}), files.scenePath, "editor.play.time_invalid");
    CheckPlayError(aliceCtx, play.Tick(std::numeric_limits<double>::infinity(), {}),
        files.scenePath, "editor.play.time_invalid");
}

ALICE_TEST(EditorPlay, InspectorEditsRotationScaleAndBindingsAreLiveOnly) {
    PlayFiles files; editor::PlaySession play;
    ALICE_REQUIRE(play.Start(files.model, files.inputPath).IsOk());
    ALICE_REQUIRE(play.IsPlaying());
    auto actor = play.Actors()[0]; actor.position = {10, 20, 30}; actor.rotation[1] = 15;
    actor.scale = {2, 2, 2}; actor.name = "Preview Player";
    ALICE_REQUIRE(play.EditActor(0, actor).IsOk());
    ALICE_REQUIRE(play.Tick(0.1, {"key.e", "key.r"}).IsOk());
    const auto actual = play.Actors()[0];
    ALICE_CHECK_STR(actual.name, actor.name);
    ALICE_CHECK(actual.position == actor.position);
    ALICE_CHECK(std::abs(actual.rotation[1] - 24.0) < 1e-9);
    ALICE_CHECK(std::abs(actual.scale[0] - 2.1) < 1e-9);
    actor.scale[0] = 0;
    CheckPlayError(aliceCtx, play.EditActor(0, actor), files.scenePath, "editor.play.transform_invalid");
    actor.scale[0] = std::numeric_limits<double>::quiet_NaN();
    CheckPlayError(aliceCtx, play.EditActor(0, actor), files.scenePath, "editor.play.transform_invalid");
    CheckPlayError(aliceCtx, play.EditActor(9, actor), files.scenePath, "editor.play.actor_invalid");
    ALICE_CHECK(play.Actors()[0].scale == actual.scale);
    play.Stop(); ALICE_CHECK_STR(files.model.Text(), kScene);
    std::string remapped = kInput;
    remapped.replace(remapped.find("key.ad"), 6, "key.qe");
    ALICE_REQUIRE(fs::WriteTextFile(files.inputPath, remapped).IsOk());
    ALICE_REQUIRE(play.Start(files.model, files.inputPath).IsOk());
    ALICE_REQUIRE(play.Tick(0.1, {"key.d"}).IsOk());
    const auto unmoved = play.Actors();
    ALICE_CHECK_EQ(unmoved[0].position[0], 0.0);
    ALICE_REQUIRE(play.Tick(0.1, {"key.e"}).IsOk());
    ALICE_CHECK(std::abs(play.Actors()[0].position[0] - 0.4) < 1e-9);
}

ALICE_TEST(EditorPlay, InvalidPlayerAndInputMapHaveRepairLocations) {
    PlayFiles files; editor::PlaySession play;
    files.model.SetText("schema: alice/scene/1\nname: Empty\nactors: []\n");
    CheckPlayError(aliceCtx, play.Start(files.model, files.inputPath), files.scenePath, "editor.play.player_missing");
    ALICE_CHECK(!play.IsPlaying());
    files.model.SetText(std::string(kScene) + "  - name: Duplicate\n    tags: [player]\n");
    CheckPlayError(aliceCtx, play.Start(files.model, files.inputPath), files.scenePath, "editor.play.player_multiple");
    files.model.SetText(kScene);
    ALICE_REQUIRE(fs::WriteTextFile(files.inputPath, "schema: alice/input/1\nname: Bad\nactions: []\n").IsOk());
    const auto invalid = play.Start(files.model, files.inputPath);
    ALICE_REQUIRE(invalid.IsErr()); ALICE_CHECK_STR(invalid.Error().file, files.inputPath);
    ALICE_CHECK(invalid.Error().mark.Valid()); ALICE_CHECK(!invalid.Error().hint.empty());
    std::string unsupported = kInput;
    unsupported.replace(unsupported.find("key.ad"), 6, "gamepad.leftStick.x");
    ALICE_REQUIRE(fs::WriteTextFile(files.inputPath, unsupported).IsOk());
    CheckPlayError(aliceCtx, play.Start(files.model, files.inputPath), files.inputPath, "editor.play.binding_unsupported");
    ALICE_CHECK(!play.IsPlaying());
}

ALICE_TEST(EditorPlay, BoundsAndSessionLifecycleRemainAtomic) {
    PlayFiles files; editor::PlaySession play;
    ALICE_CHECK(play.Tick(0.1, {}).IsErr());
    ALICE_CHECK(play.EditActor(0, {}).IsErr());
    ALICE_REQUIRE(play.Start(files.model, files.inputPath).IsOk());
    ALICE_REQUIRE(play.IsPlaying());
    CheckPlayError(aliceCtx, play.Start(files.model, files.inputPath), files.scenePath, "editor.play.already_started");
    auto actor = play.Actors()[0];
    actor.position = {9999.9, 1, 0}; actor.rotation[1] = 359999.9; actor.scale = {99.99, 99.99, 99.99};
    ALICE_REQUIRE(play.EditActor(0, actor).IsOk());
    ALICE_REQUIRE(play.Tick(0.1, {"key.d", "key.e", "key.r"}).IsOk());
    auto actual = play.Actors()[0];
    ALICE_CHECK_EQ(actual.position[0], 10000.0);
    ALICE_CHECK_EQ(actual.rotation[1], 360000.0);
    ALICE_CHECK_EQ(actual.scale[0], 100.0);
    actor.scale = {0.06, 0.06, 0.06};
    ALICE_REQUIRE(play.EditActor(0, actor).IsOk());
    ALICE_REQUIRE(play.Tick(0.1, {"key.f"}).IsOk());
    const auto shrunk = play.Actors();
    ALICE_CHECK_EQ(shrunk[0].scale[0], 0.05);
    play.Stop(); play.Stop(); ALICE_CHECK(!play.IsPlaying());
    files.model.SetText("schema: alice/scene/1\nname: Invalid\nactors: broken\n");
    CheckPlayError(aliceCtx, play.Start(files.model, files.inputPath), files.scenePath, "editor.play.scene_invalid");
    ALICE_CHECK(!play.IsPlaying());
}

ALICE_TEST(EditorPlay, InvalidBindingsAreLocatedAtTheirInputFields) {
    PlayFiles files; editor::PlaySession play;
    struct Invalid { const char* from; const char* to; const char* code; const char* path; };
    const Invalid cases[] = {
        {"MoveX", "Jump", "editor.play.action_unsupported", "actions[0].name"},
        {"MoveX", "MoveZ", "editor.play.action_duplicate", "actions[1].name"},
        {"type: axis", "type: button", "editor.play.action_type", "actions[0].type"},
        {"bindings: [key.ad]", "bindings: [key.ad], deadzone: 1", "editor.play.deadzone_invalid", "actions[0].deadzone"},
        {"key.ad", "gamepad.leftStick.x", "editor.play.binding_unsupported", "actions[0].bindings[0]"}
    };
    for (const auto& invalid : cases) {
        std::string text = kInput;
        text.replace(text.find(invalid.from), std::string(invalid.from).size(), invalid.to);
        ALICE_REQUIRE(fs::WriteTextFile(files.inputPath, text).IsOk());
        const auto status = play.Start(files.model, files.inputPath);
        CheckPlayError(aliceCtx, status, files.inputPath, invalid.code);
        ALICE_REQUIRE(status.IsErr());
        ALICE_CHECK_STR(status.Error().path, invalid.path);
        ALICE_CHECK(status.Error().mark.line >= 4);
        ALICE_CHECK(!play.IsPlaying());
    }
    ALICE_REQUIRE(fs::WriteTextFile(files.inputPath,
        "schema: alice/input/1\nname: Missing\nactions:\n  - { name: MoveX, type: axis, bindings: [key.ad] }\n").IsOk());
    CheckPlayError(aliceCtx, play.Start(files.model, files.inputPath), files.inputPath, "editor.play.action_missing");
    CheckPlayError(aliceCtx, play.Start(files.model, files.inputPath + ".missing"), files.inputPath + ".missing", "editor.play.input_unreadable");
}

ALICE_TEST(EditorPlay, MalformedInputRetainsParserCodeAndRepairHint) {
    PlayFiles files; editor::PlaySession play;
    for (const char* malformed : {"schema: alice/input/1\nname: \"\\u12\"\n",
                                  "schema: alice/input/1\nname: \"\\uZZZZ\"\n"}) {
        ALICE_REQUIRE(fs::WriteTextFile(files.inputPath, malformed).IsOk());
        const auto status = play.Start(files.model, files.inputPath);
        CheckPlayError(aliceCtx, status, files.inputPath, "doc.parse.bad_escape");
        ALICE_REQUIRE(status.IsErr());
        ALICE_CHECK_EQ(status.Error().mark.line, 2u);
        ALICE_CHECK_STR(status.Error().path, "document");
        ALICE_CHECK(!play.IsPlaying());
    }
}

ALICE_TEST(EditorPlay, ActorErrorsPointToTheOriginalRelevantField) {
    PlayFiles files; editor::PlaySession play;
    files.model.SetText("schema: alice/scene/1\nname: Preview\nactors:\n"
        "  - name: Player\n    tags: [player]\n");
    ALICE_REQUIRE(play.Start(files.model, files.inputPath).IsOk());
    auto actor = play.Actors()[0];
    const auto index = play.EditActor(999, actor);
    CheckPlayError(aliceCtx, index, files.scenePath, "editor.play.actor_invalid");
    ALICE_REQUIRE(index.IsErr());
    ALICE_CHECK_STR(index.Error().path, "actors");
    ALICE_CHECK_EQ(index.Error().mark.line, 4u);
    actor.name.clear();
    const auto name = play.EditActor(0, actor);
    CheckPlayError(aliceCtx, name, files.scenePath, "editor.play.actor_name_empty");
    ALICE_REQUIRE(name.IsErr());
    ALICE_CHECK_STR(name.Error().path, "actors[0].name");
    ALICE_CHECK_EQ(name.Error().mark.line, 4u);
    ALICE_CHECK_EQ(name.Error().mark.column, 11u);
    actor.name = "Player"; actor.scale[0] = 0;
    const auto transform = play.EditActor(0, actor);
    CheckPlayError(aliceCtx, transform, files.scenePath, "editor.play.transform_invalid");
    ALICE_REQUIRE(transform.IsErr());
    ALICE_CHECK_STR(transform.Error().path, "actors[0].transform");
    ALICE_CHECK_EQ(transform.Error().mark.line, 4u);
    ALICE_CHECK_EQ(transform.Error().mark.column, 5u);
}
