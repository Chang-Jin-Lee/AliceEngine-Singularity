// SPDX-License-Identifier: MIT
// Editing must preserve disk state on invalid input and on external changes.
#include "TestFramework.h"
#include "AliceEditor/DocumentModel.h"
#include "Foundation/FileSystem.h"
#include <chrono>
#include <filesystem>
#include <limits>

using namespace alice;
namespace {
const char* kScene = "# artist note\nschema: alice/scene/1\nname: Test\nactors:\n  - name: Cube\n    transform: { position: [1, 2, 3] }\n";
struct File {
    std::filesystem::path directory;
    std::string path;
    File() {
        std::error_code ec;
        directory = std::filesystem::temp_directory_path(ec) / ("alice-editor-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(directory, ec);
        const auto utf8 = (directory / "scene.yaml").generic_u8string();
        path.assign(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        ALICE_ASSERT(fs::WriteTextFile(path, kScene).IsOk(), "test file must exist");
    }
    ~File() { std::error_code ec; std::filesystem::remove_all(directory, ec); }
};
}

ALICE_TEST(Editor, EditUndoRedoSaveRoundTrip) {
    File file; editor::DocumentModel model;
    ALICE_REQUIRE(model.Open(file.path).IsOk());
    ALICE_REQUIRE(model.IsValid()); ALICE_REQUIRE(model.IsScene());
    auto actor = model.Actors().at(0);
    actor.name = "Edited"; actor.position[0] = 9;
    ALICE_REQUIRE(model.EditActor(0, actor).IsOk());
    ALICE_CHECK(model.IsDirty()); ALICE_CHECK(model.Undo());
    ALICE_CHECK(!model.IsDirty()); ALICE_CHECK(model.Redo());
    ALICE_REQUIRE(model.Save().IsOk()); ALICE_CHECK(!model.IsDirty());
    editor::DocumentModel reloaded;
    ALICE_REQUIRE(reloaded.Open(file.path).IsOk());
    ALICE_CHECK_STR(reloaded.Actors()[0].name, "Edited");
    ALICE_CHECK_EQ(reloaded.Actors()[0].position[0], 9.0);
    ALICE_CHECK(reloaded.Text().find("artist note") != std::string::npos);
}
ALICE_TEST(Editor, InvalidTextCannotOverwriteFile) {
    File file; editor::DocumentModel model;
    ALICE_REQUIRE(model.Open(file.path).IsOk());
    model.SetText("schema: alice/scene/1\nname: 123\n");
    ALICE_CHECK(!model.IsValid()); ALICE_CHECK(model.Save().IsErr());
    ALICE_CHECK_STR(fs::ReadTextFile(file.path).Value(), kScene);
    ALICE_REQUIRE(!model.Diagnostics().Empty());
    for (const auto& d : model.Diagnostics().Items()) {
        ALICE_CHECK_STR(d.file, file.path); ALICE_CHECK(d.mark.Valid()); ALICE_CHECK(!d.hint.empty());
    }
    ALICE_CHECK(model.Undo()); ALICE_CHECK(model.IsValid());
}
ALICE_TEST(Editor, ExternalChangesAreNotOverwritten) {
    File file; editor::DocumentModel model;
    ALICE_REQUIRE(model.Open(file.path).IsOk());
    ALICE_REQUIRE(model.AddActor().IsOk());
    ALICE_REQUIRE(fs::WriteTextFile(file.path, "external edit").IsOk());
    auto saved = model.Save(); ALICE_REQUIRE(saved.IsErr());
    ALICE_CHECK_STR(saved.Error().code, "editor.save.conflict");
    ALICE_CHECK_STR(saved.Error().file, file.path); ALICE_CHECK(!saved.Error().hint.empty());
    ALICE_CHECK(model.IsDirty());
    ALICE_CHECK_STR(fs::ReadTextFile(file.path).Value(), "external edit");
}
ALICE_TEST(Editor, ActorChangesAndFailedOpenAreAtomic) {
    File file; editor::DocumentModel model;
    ALICE_REQUIRE(model.Open(file.path).IsOk());
    ALICE_REQUIRE(model.AddActor().IsOk());
    ALICE_CHECK_EQ(static_cast<u64>(model.Actors().size()), 2u);
    ALICE_CHECK(model.DeleteActor(77).IsErr());
    ALICE_REQUIRE(model.DeleteActor(0).IsOk());
    ALICE_CHECK(model.Undo());
    const auto before = model.Text();
    ALICE_CHECK(model.Open(file.path + ".missing").IsErr());
    ALICE_CHECK_STR(model.Text(), before);
    auto actor = model.Actors()[0]; actor.name.clear();
    ALICE_CHECK(model.EditActor(0, actor).IsErr());
    ALICE_CHECK_STR(model.Text(), before);
    actor.name = "Finite"; actor.scale[0] = std::numeric_limits<double>::quiet_NaN();
    ALICE_CHECK(model.EditActor(0, actor).IsErr());
    ALICE_CHECK_STR(model.Text(), before);
}

ALICE_TEST(Editor, ExistingTemporaryFileIsPreserved) {
    File file; editor::DocumentModel model;
    ALICE_REQUIRE(model.Open(file.path).IsOk());
    ALICE_REQUIRE(model.AddActor().IsOk());
    const auto temporary = file.path + ".alice-editor.tmp";
    ALICE_REQUIRE(fs::WriteTextFile(temporary, "recovery data").IsOk());
    const auto saved = model.Save(); ALICE_REQUIRE(saved.IsErr());
    ALICE_CHECK_STR(saved.Error().code, "editor.save.temporary_failed");
    ALICE_CHECK_STR(fs::ReadTextFile(temporary).Value(), "recovery data");
    ALICE_CHECK_STR(fs::ReadTextFile(file.path).Value(), kScene);
    ALICE_CHECK(model.IsDirty());
}

ALICE_TEST(Editor, SyntaxErrorsAndUnknownSchemasHaveRepairHints) {
    File file; editor::DocumentModel model;
    ALICE_REQUIRE(model.Open(file.path).IsOk());
    for (const char* text : {"schema: [", "schema: unknown/type/1\nname: Test\n"}) {
        model.SetText(text); ALICE_CHECK(!model.IsValid());
        ALICE_REQUIRE(!model.Diagnostics().Empty());
        for (const auto& d : model.Diagnostics().Items()) {
            ALICE_CHECK(!d.code.empty()); ALICE_CHECK_STR(d.file, file.path);
            ALICE_CHECK(d.mark.Valid()); ALICE_CHECK(!d.hint.empty());
        }
        ALICE_CHECK(model.Save().IsErr());
        ALICE_CHECK_STR(fs::ReadTextFile(file.path).Value(), kScene);
    }
}
