# SPDX-License-Identifier: MIT
# 테스트는 모듈마다 파일 하나. 러너는 하나로 합친다(링크 시간이 훨씬 짧다).

set(ALICE_TEST_SOURCES
    TestFramework.h
    TestFramework.cpp
    Main.cpp
    Foundation_Tests.cpp
    Doc_Value_Tests.cpp
    Doc_Yaml_Tests.cpp
    Doc_Json_Tests.cpp
    Doc_RoundTrip_Tests.cpp
)

set(ALICE_TEST_DEPS Alice.Foundation Alice.Doc)

if(TARGET Alice.Editor.Model)
    list(APPEND ALICE_TEST_SOURCES Editor_Tests.cpp)
    list(APPEND ALICE_TEST_DEPS Alice.Editor.Model)
endif()

if(TARGET Alice.Runtime)
    list(APPEND ALICE_TEST_SOURCES Runtime_ECS_Tests.cpp)
    list(APPEND ALICE_TEST_DEPS Alice.Runtime)
endif()

if(TARGET Alice.Schema)
    list(APPEND ALICE_TEST_SOURCES Schema_Tests.cpp)
    list(APPEND ALICE_TEST_DEPS Alice.Schema)
endif()

if(TARGET Alice.Verbs)
    list(APPEND ALICE_TEST_SOURCES Verbs_Tests.cpp)
    list(APPEND ALICE_TEST_DEPS Alice.Verbs)
endif()

if(TARGET Alice.RHI)
    list(APPEND ALICE_TEST_SOURCES RHI_Tests.cpp)
    list(APPEND ALICE_TEST_DEPS Alice.RHI)
endif()

alice_executable(Alice.Tests
    FOLDER "Tests"
    SOURCES ${ALICE_TEST_SOURCES}
    DEPS    ${ALICE_TEST_DEPS}
)

add_test(NAME Alice.Tests COMMAND Alice.Tests)

if(TARGET Alice.Editor)
    add_test(NAME Alice.Editor.Smoke COMMAND Alice.Editor --smoke-test --project "${PROJECT_SOURCE_DIR}/Samples/FirstLight")
    set_tests_properties(Alice.Editor.Smoke PROPERTIES TIMEOUT 20)
endif()

# CI 와 AI 도구는 JSON 결과를 읽는다.
add_test(NAME Alice.Tests.Json COMMAND Alice.Tests --json)
set_tests_properties(Alice.Tests.Json PROPERTIES DISABLED TRUE)
