# SPDX-License-Identifier: MIT
# 엔진 모듈 목록과 **의존성 그래프**.
#
# 명시적 타깃 의존성과 Runtime 공개 헤더 경계 검사가 모듈 계약을 기록한다.
#
#   Foundation                 로그 · 프로파일러 · 진단 · 파일시스템   (의존 없음)
#      ├─ Doc                  문서 값 모델 · YAML/JSON 파서 · 직렬화
#      │    └─ Schema          스키마 정의 · 검증 · JSON Schema 생성
#      │         └─ Verbs      동사 레지스트리 · 조건식 · 2차 검사
#      └─ RHI                  그래픽 추상화 (백엔드 중립 인터페이스)
#
# Runtime은 공개 계약만 있다. Asset / Bridge / Editor 구현은 백로그에 있다.

set(ALICE_ENGINE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "엔진 소스 루트")

# ── Foundation ──────────────────────────────────────────────────────────────
alice_module(Alice.Foundation
    FOLDER "Engine"
    SOURCES
        Foundation/Core.h
        Foundation/Time.h
        Foundation/Result.h
        Foundation/Diagnostic.h
        Foundation/Diagnostic.cpp
        Foundation/StringUtil.h
        Foundation/StringUtil.cpp
        Foundation/Log.h
        Foundation/Log.cpp
        Foundation/Profiler.h
        Foundation/Profiler.cpp
        Foundation/FileSystem.h
        Foundation/FileSystem.cpp
)

# ── Doc ─────────────────────────────────────────────────────────────────────
# 이 엔진의 콘텐츠는 전부 텍스트 문서다. 그 문서를 다루는 계층.
alice_module(Alice.Doc
    FOLDER "Engine"
    DEPS Alice.Foundation
    SOURCES
        Doc/Value.h
        Doc/Value.cpp
        Doc/YamlParser.h
        Doc/YamlParser.cpp
        Doc/JsonParser.h
        Doc/JsonParser.cpp
        Doc/Writer.h
        Doc/Writer.cpp
        Doc/Document.h
        Doc/Document.cpp
)

# ── Schema ──────────────────────────────────────────────────────────────────
# 문서가 "올바른지"를 판정하고, 틀렸으면 고치는 법까지 알려준다.
alice_module(Alice.Schema
    FOLDER "Engine"
    DEPS Alice.Doc
    SOURCES
        Schema/Schema.h
        Schema/Schema.cpp
        Schema/SchemaBuilder.h
        Schema/SchemaBuilder.cpp
        Schema/Validator.h
        Schema/Validator.cpp
        Schema/Registry.h
        Schema/Registry.cpp
        Schema/CoreSchemas.cpp
        Schema/JsonSchemaEmitter.h
        Schema/JsonSchemaEmitter.cpp
)

# ── Verbs ───────────────────────────────────────────────────────────────────
# 선언적 콘텐츠가 호출할 수 있는 "동사"의 목록 = 이 엔진의 API 표면 전부.
alice_module(Alice.Verbs
    FOLDER "Engine"
    DEPS Alice.Schema
    SOURCES
        Verbs/Verb.h
        Verbs/Verb.cpp
        Verbs/Expression.h
        Verbs/Expression.cpp
        Verbs/SemanticCheck.h
        Verbs/SemanticCheck.cpp
        Verbs/CoreVerbs.cpp
)

# ── RHI ─────────────────────────────────────────────────────────────────────
# 백엔드 중립 그래픽 인터페이스.
add_subdirectory(RHI)

# ALI-02: declarations only. Compile each header even with ALICE_BUILD_TESTS=OFF.
set(alice_runtime_headers
    Runtime/RuntimeTypes.h
    Runtime/ComponentRegistry.h
    Runtime/World.h
    Runtime/RenderView.h
    Runtime/RenderExtraction.h
    Runtime/Rules.h
)
alice_module(Alice.Runtime INTERFACE
    FOLDER "Engine"
    DEPS Alice.Verbs
    SOURCES ${alice_runtime_headers}
)
set(alice_runtime_probes)
foreach(header IN LISTS alice_runtime_headers)
    # Only the new Runtime public surface is checked here; this is not a whole-engine include linter.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${header}")
    file(STRINGS "${header}" includes REGEX "^[ \t]*#[ \t]*include[ \t]*[\"<]")
    foreach(include IN LISTS includes)
        string(REGEX REPLACE "^[ \t]*#[ \t]*include[ \t]*[\"<]([^\">]+)[\">].*$" "\\1" dependency "${include}")
        if(dependency MATCHES "[/\\\\]")
            if(header STREQUAL "Runtime/RenderView.h")
                if(NOT dependency MATCHES "^Foundation/[A-Za-z0-9_]+[.]h$")
                    message(FATAL_ERROR "RenderView must be independent of ECS/RHI: ${dependency}")
                endif()
            elseif(NOT dependency MATCHES "^(Foundation|Doc|Schema|Verbs|Runtime)/[A-Za-z0-9_]+[.]h$")
                message(FATAL_ERROR "Runtime public dependency is not allowed: ${dependency}")
            endif()
        elseif(include MATCHES "\"")
            message(FATAL_ERROR "Runtime project includes must start with a module: ${dependency}")
        endif()
    endforeach()
    get_filename_component(stem "${header}" NAME_WE)
    set(probe "${CMAKE_CURRENT_BINARY_DIR}/RuntimeContracts/${stem}.cpp")
    file(GENERATE OUTPUT "${probe}" CONTENT "#include \"${header}\"\n")
    list(APPEND alice_runtime_probes "${probe}")
endforeach()
configure_file("${PROJECT_SOURCE_DIR}/CMake/RuntimeContracts.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/RuntimeContracts/Consumers.cpp" COPYONLY)
add_library(Alice.Runtime.Contracts OBJECT ${alice_runtime_probes}
    "${CMAKE_CURRENT_BINARY_DIR}/RuntimeContracts/Consumers.cpp")
alice_apply_common_settings(Alice.Runtime.Contracts)
target_link_libraries(Alice.Runtime.Contracts PRIVATE Alice.Runtime)
set_target_properties(Alice.Runtime.Contracts PROPERTIES FOLDER "Tests/Contracts")
