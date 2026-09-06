# SPDX-License-Identifier: MIT
# 모든 엔진 모듈이 지나가는 단 하나의 관문.
#
# 여기가 "모듈 경계를 빌드로 강제한다"는 규칙이 실제로 사는 곳이다.
# alice_module() 은 의존성을 PUBLIC 으로만 걸고, include 경로를 소스 루트 하나로 고정한다.
# 그래서 어떤 모듈도 상대 경로(../../)로 남의 내부를 들여다볼 수 없다.

include_guard(GLOBAL)

# 엔진 공통 컴파일 설정을 붙인다.
function(alice_apply_common_settings target)
    target_compile_features(${target} PUBLIC cxx_std_20)

    # 소스 루트 하나만 include 경로로 준다.
    # 모든 include 는 "Foundation/Log.h" 처럼 모듈 이름부터 시작해야 한다.
    target_include_directories(${target} PUBLIC "${ALICE_ENGINE_SOURCE_DIR}")

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-      # 표준 준수. 이걸 끄면 다른 컴파일러에서 반드시 터진다.
            /Zc:__cplusplus   # 없으면 __cplusplus 가 199711L 로 거짓말한다
            /Zc:preprocessor  # 표준 전처리기. __VA_ARGS__ 동작이 GCC/Clang 과 같아진다
            /MP               # 병렬 컴파일
            /utf-8            # CP949 로케일에서 C4819 를 막는다. 이 환경에서 필수다
            /wd4127           # conditional expression is constant — if constexpr 대체 불가한 곳이 있다
        )
        target_compile_definitions(${target} PUBLIC
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
        )
        if(ALICE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wno-missing-field-initializers
        )
        if(ALICE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    target_compile_definitions(${target} PUBLIC
        $<$<CONFIG:Debug>:ALICE_DEBUG=1>
        ALICE_VERSION_STRING="${PROJECT_VERSION}"
    )
endfunction()

# alice_module(<이름>
#     SOURCES  <파일...>
#     DEPS     <다른 alice 모듈 또는 외부 타깃...>
#     [INTERFACE]            헤더 전용 모듈
#     [FOLDER <IDE 폴더>]
# )
function(alice_module name)
    cmake_parse_arguments(ARG "INTERFACE" "FOLDER" "SOURCES;DEPS" ${ARGN})

    if(ARG_INTERFACE)
        add_library(${name} INTERFACE)
        target_include_directories(${name} INTERFACE "${ALICE_ENGINE_SOURCE_DIR}")
        target_compile_features(${name} INTERFACE cxx_std_20)
        if(ARG_DEPS)
            target_link_libraries(${name} INTERFACE ${ARG_DEPS})
        endif()
        # 헤더 전용이어도 IDE 에 보이게 더미 타깃을 만든다.
        if(ARG_SOURCES)
            add_custom_target(${name}.headers SOURCES ${ARG_SOURCES})
            set_target_properties(${name}.headers PROPERTIES
                FOLDER "${ARG_FOLDER}" EXCLUDE_FROM_ALL TRUE)
        endif()
        return()
    endif()

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "alice_module(${name}): SOURCES 가 비어 있다")
    endif()

    add_library(${name} STATIC ${ARG_SOURCES})
    alice_apply_common_settings(${name})

    if(ARG_DEPS)
        target_link_libraries(${name} PUBLIC ${ARG_DEPS})
    endif()

    set_target_properties(${name} PROPERTIES
        FOLDER "${ARG_FOLDER}"
        POSITION_INDEPENDENT_CODE ON)

    # IDE 트리를 소스 디렉터리 구조 그대로 만든다.
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ARG_SOURCES})
endfunction()

# alice_executable(<이름> SOURCES ... DEPS ... [FOLDER ...])
function(alice_executable name)
    cmake_parse_arguments(ARG "" "FOLDER" "SOURCES;DEPS" ${ARGN})

    add_executable(${name} ${ARG_SOURCES})
    alice_apply_common_settings(${name})

    if(ARG_DEPS)
        target_link_libraries(${name} PRIVATE ${ARG_DEPS})
    endif()

    set_target_properties(${name} PROPERTIES FOLDER "${ARG_FOLDER}")
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ARG_SOURCES})
endfunction()
