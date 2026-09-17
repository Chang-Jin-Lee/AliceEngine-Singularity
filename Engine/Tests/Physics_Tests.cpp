// SPDX-License-Identifier: MIT
// Thin obstacles and separated contact axes make accidental discrete collision implementations fail.
#include "TestFramework.h"
#include "Runtime/Physics/Platformer.h"
#include <array>
#include <cmath>
#include <limits>
#include <memory>
using namespace alice;
using namespace alice::runtime;
using namespace alice::runtime::physics;
namespace alice::examples {
Result<std::unique_ptr<IMotionSolver>> MakeSlowMotionSolver(std::span<const Box> terrain);
}

ALICE_TEST(Physics, CustomProviderChangesMotionAndKeepsCollisionContract) {
    std::array<Box,1> terrain{{{{0,-1},{10,1}}}};
    auto provider=alice::examples::MakeSlowMotionSolver(terrain);
    ALICE_REQUIRE(provider);
    auto motion=provider.Value()->Move({{0,3},{0.5,0.5}},{2,-1});
    ALICE_REQUIRE(motion);
    ALICE_CHECK_EQ(motion->box.center.x,1.0);
    ALICE_CHECK_EQ(motion->box.center.y,2.5);
    auto landing=provider.Value()->Move(motion->box,{0,-100});
    ALICE_REQUIRE(landing);
    ALICE_CHECK(landing->grounded);
    auto restored=provider.Value()->Move(motion->box,{0,-100});
    ALICE_REQUIRE(restored);
    ALICE_CHECK_EQ(restored->box.center.y,landing->box.center.y);
}

ALICE_TEST(Physics, SweptThinFloorAndRest) {
    StaticWorld world;
    std::array<Box, 1> floor{{{{0,-0.01},{10,0.01}}}};
    ALICE_REQUIRE(world.Build(floor));
    auto hit = world.Move({{0,10},{0.5,0.5}}, {0,-100});
    ALICE_REQUIRE(hit);
    ALICE_CHECK(std::abs(hit->box.center.y - 0.5) < 1e-7);
    ALICE_CHECK(hit->blockedY && hit->grounded);
    auto rest = world.Move(hit->box, {1,0});
    ALICE_REQUIRE(rest);
    ALICE_CHECK(rest->grounded);
    ALICE_CHECK(std::abs(rest->box.center.x - 1) < 1e-7);
}
ALICE_TEST(Physics, WallCeilingAndJumpAway) {
    StaticWorld world;
    std::array<Box,2> terrain{{{{2,0},{0.1,10}},{{0,3},{10,0.1}}}};
    ALICE_REQUIRE(world.Build(terrain));
    auto wall = world.Move({{0,0},{0.5,0.5}}, {100,1});
    ALICE_REQUIRE(wall);
    ALICE_CHECK(wall->blockedX && !wall->grounded);
    ALICE_CHECK(std::abs(wall->box.center.x - 1.4) < 1e-7);
    ALICE_CHECK(std::abs(wall->box.center.y - 1) < 1e-7);
    auto ceiling = world.Move({{0,0},{0.5,0.5}}, {0,100});
    ALICE_REQUIRE(ceiling);
    ALICE_CHECK(ceiling->blockedY && !ceiling->grounded);
    auto away = world.Move(ceiling->box, {0,-1});
    ALICE_REQUIRE(away);
    ALICE_CHECK(!away->blockedY);
}
ALICE_TEST(Physics, OverlapResolutionAndDiagnostic) {
    StaticWorld world;
    std::array<Box,1> floor{{{{0,-1},{10,1}}}};
    ALICE_REQUIRE(world.Build(floor));
    auto resolved = world.Move({{0,0.4},{0.5,0.5}}, {});
    ALICE_REQUIRE(resolved);
    ALICE_CHECK(std::abs(resolved->box.center.y - 0.5) < 1e-7);
    SourceLocation source{"bad.scene.yaml","actors[0]",{12,4,0}};
    auto invalid = world.Move({{0,0},{-1,1}}, {},source);
    ALICE_REQUIRE(!invalid);
    ALICE_CHECK_EQ(invalid.Error().mark.line,12u);
    ALICE_CHECK_EQ(invalid.Error().file,source.file);
    ALICE_CHECK(!invalid.Error().hint.empty());
}

ALICE_TEST(Physics, CornerContactAndLedgeExit) {
    StaticWorld world;
    std::array<Box,2> terrain{{{{0,-0.5},{1,0.5}},{{2,1},{0.5,2}}}};
    ALICE_REQUIRE(world.Build(terrain));
    auto corner=world.Move({{0,2},{0.5,0.5}},{2,-3});
    ALICE_REQUIRE(corner);
    ALICE_CHECK(corner->blockedX && corner->blockedY);
    ALICE_CHECK(std::abs(corner->box.center.x-1.0)<1e-7);
    ALICE_CHECK(std::abs(corner->box.center.y-0.5)<1e-7);
    StaticWorld ledge;
    ALICE_REQUIRE(ledge.Build(std::span<const Box>{terrain.data(),1}));
    auto depart=ledge.Move({{0,0.5},{0.5,0.5}},{2,0});
    ALICE_REQUIRE(depart);
    ALICE_CHECK(!depart->grounded);
    auto fall=ledge.Move(depart->box,{0,-1});
    ALICE_REQUIRE(fall);
    ALICE_CHECK(fall->box.center.y<0);
}
ALICE_TEST(Physics, InvalidBuildPreservesTerrainAndFiniteContract) {
    StaticWorld world;
    std::array<Box,1> terrain{{{{0,-1},{10,1}}}};
    ALICE_REQUIRE(world.Build(terrain));
    terrain[0].half.y=0;
    SourceLocation source{"bad.yaml","terrain",{7,2,0}};
    auto status=world.Build(terrain,source);
    ALICE_REQUIRE(!status);
    ALICE_CHECK_EQ(status.Error().mark.line,7u);
    ALICE_CHECK(!status.Error().hint.empty());
    auto landing=world.Move({{0,5},{0.5,0.5}},{0,-10});
    ALICE_REQUIRE(landing);
    ALICE_CHECK(landing->grounded);
    for(double bad:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),1e10}) {
        auto invalid=world.Move({{0,5},{0.5,0.5}},{bad,0},source);
        ALICE_REQUIRE(!invalid);
        ALICE_CHECK_EQ(invalid.Error().path,source.path);
        ALICE_CHECK(!invalid.Error().hint.empty());
    }
}
ALICE_TEST(Physics, IndexedMatchesExhaustiveAcrossDeterministicQueries) {
    std::vector<Box> terrain;
    for(int i=0;i<1000;++i) terrain.push_back({{static_cast<double>(i%50)*3,
        static_cast<double>(i/50)*3},{0.5,0.5}});
    StaticWorld indexed,exhaustive(false);
    ALICE_REQUIRE(indexed.Build(terrain)); ALICE_REQUIRE(exhaustive.Build(terrain));
    usize indexedTests=0,exhaustiveTests=0;
    for(int i=0;i<200;++i) {
        const Box box{{static_cast<double>((i*17)%150)+0.8,static_cast<double>((i*11)%60)+1.0},{0.2,0.3}};
        const Vec2 delta{static_cast<double>(i%7)-3,static_cast<double>(i%11)-5};
        auto a=indexed.Move(box,delta),b=exhaustive.Move(box,delta);
        ALICE_REQUIRE(a); ALICE_REQUIRE(b);
        ALICE_CHECK_EQ(a->box.center.x,b->box.center.x);
        ALICE_CHECK_EQ(a->box.center.y,b->box.center.y);
        ALICE_CHECK_EQ(a->blockedX,b->blockedX); ALICE_CHECK_EQ(a->blockedY,b->blockedY);
        ALICE_CHECK_EQ(a->grounded,b->grounded);
        indexedTests+=a->tests; exhaustiveTests+=b->tests;
    }
    ALICE_CHECK(indexedTests*10<exhaustiveTests);
}
ALICE_TEST(Physics, TrappedSpawnIsBoundedAndDiagnosed) {
    StaticWorld world;
    std::array<Box,2> terrain{{{{-0.6,0},{0.5,10}},{{0.6,0},{0.5,10}}}};
    ALICE_REQUIRE(world.Build(terrain));
    auto trapped=world.Move({{0,0},{0.5,0.5}},{},{"scene.yaml","spawn",{8,1,0}});
    ALICE_REQUIRE(!trapped);
    ALICE_CHECK_EQ(trapped.Error().code,"physics.overlap.unresolved");
    ALICE_CHECK_EQ(trapped.Error().mark.line,8u);
    ALICE_CHECK(!trapped.Error().hint.empty());
}

ALICE_TEST(Physics, EmptyWorldAndFractionalRestStayStable) {
    StaticWorld world;
    ALICE_REQUIRE(world.Build({}));
    auto free=world.Move({{2,4},{0.3,0.7}},{5,-8});
    ALICE_REQUIRE(free);
    ALICE_CHECK_EQ(free->box.center.x,7.0);
    ALICE_CHECK_EQ(free->box.center.y,-4.0);
    ALICE_CHECK(!free->grounded && !free->blockedX && !free->blockedY);
    std::array<Box,1> terrain{{{{0,0.1},{100,0.2}}}};
    ALICE_REQUIRE(world.Build(terrain));
    Box body{{0,2},{0.3,0.7}};
    for(int step=0;step<600;++step) {
        auto motion=world.Move(body,{0.01,-0.1});
        ALICE_REQUIRE(motion);
        body=motion->box;
        ALICE_CHECK(body.center.y>=1.0-1e-8);
        if(step>20) ALICE_CHECK(motion->grounded);
    }
    auto jump=world.Move(body,{0,0.2});
    ALICE_REQUIRE(jump);
    ALICE_CHECK(!jump->grounded && !jump->blockedY);
}

ALICE_TEST(Physics, TangentCornerDoesNotBlockMotion) {
    StaticWorld world;
    std::array<Box,1> terrain{{{{2,0},{0.5,0.5}}}};
    ALICE_REQUIRE(world.Build(terrain));
    auto tangent=world.Move({{0,0},{0.5,0.5}},{2,2});
    ALICE_REQUIRE(tangent);
    ALICE_CHECK_EQ(tangent->box.center.x,2.0);
    ALICE_CHECK_EQ(tangent->box.center.y,2.0);
    ALICE_CHECK(!tangent->blockedX && !tangent->blockedY);
}
