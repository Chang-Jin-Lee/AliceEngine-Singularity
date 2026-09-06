# SPDX-License-Identifier: MIT
# AliceEngine-Singularity — 루트 빌드 스크립트
#
# 원칙 세 가지.
#   1. 클론하면 바로 빌드된다. 필수 서드파티가 없다. (참고한 엔진 대부분이 여기서 막혔다)
#   2. 모듈 경계를 CMake 타깃으로 강제한다. 문서로 적은 의존성 규칙은 반드시 썩는다.
#      Alice.Doc 이 Alice.RHI 를 참조하면 링크가 아니라 **설정 단계에서** 깨지게 둔다.
#   3. 이 환경(한글 경로 + CP949 로케일 + VS2026 + CMake 4)의 함정을 스크립트가 흡수한다.

cmake_minimum_required(VERSION 3.24)

# ── Windows SDK 고정 ────────────────────────────────────────────────────────
# SDK 10.0.28000 에서 DirectXCollision.inl 의 namespace Internal 이 MathInternal 로
# 개명됐다. 이 비공개 헬퍼를 쓰는 코드(우리도 언젠가 쓴다)가 전부 깨진다.
# project() 앞에서만 의미가 있으므로 여기서 잡는다. 필요하면 -D 로 덮어쓸 수 있다.
if(WIN32 AND NOT DEFINED CMAKE_SYSTEM_VERSION)
    set(CMAKE_SYSTEM_VERSION "10.0.26100.0" CACHE STRING "고정된 Windows SDK 버전")
endif()

project(AliceEngineSingularity
        VERSION 0.1.0
        DESCRIPTION "AI-native game engine"
        LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
include(AliceModule)

# ── 표준 ────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "빌드 타입" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# compile_commands.json 은 clangd 와 AI 도구가 코드베이스를 읽는 표준 입구다. 항상 켠다.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── 옵션 ────────────────────────────────────────────────────────────────────
option(ALICE_BUILD_TESTS  "테스트를 빌드한다"            ON)
option(ALICE_BUILD_TOOLS  "alice CLI 를 빌드한다"        ON)
option(ALICE_WARNINGS_AS_ERRORS "경고를 에러로 취급한다"  OFF)

# 렌더 백엔드. 플랫폼 기본값을 잡되 전부 개별로 끌 수 있다.
option(ALICE_RHI_NULL   "헤드리스 Null 백엔드 (CI/테스트용, 항상 권장)" ON)
if(WIN32)
    option(ALICE_RHI_D3D11  "Direct3D 11 백엔드"  ON)
    option(ALICE_RHI_D3D12  "Direct3D 12 백엔드"  ON)
else()
    option(ALICE_RHI_D3D11  "Direct3D 11 백엔드"  OFF)
    option(ALICE_RHI_D3D12  "Direct3D 12 백엔드"  OFF)
endif()
if(APPLE)
    option(ALICE_RHI_METAL  "Metal 백엔드"        ON)
else()
    option(ALICE_RHI_METAL  "Metal 백엔드"        OFF)
endif()
option(ALICE_RHI_VULKAN "Vulkan 백엔드" OFF)

# ── 하위 디렉터리 ───────────────────────────────────────────────────────────
add_subdirectory(Engine)

if(ALICE_BUILD_TOOLS)
    add_subdirectory(Tools)
endif()

if(ALICE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Engine/Tests)
endif()

# ── 설정 요약 ───────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "  AliceEngine-Singularity ${PROJECT_VERSION}")
message(STATUS "  ---------------------------------------------")
message(STATUS "  컴파일러      : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  빌드 타입     : ${CMAKE_BUILD_TYPE}${CMAKE_CONFIGURATION_TYPES}")
if(WIN32)
    message(STATUS "  Windows SDK  : ${CMAKE_SYSTEM_VERSION}")
endif()
message(STATUS "  RHI 백엔드    : null=${ALICE_RHI_NULL} d3d11=${ALICE_RHI_D3D11} d3d12=${ALICE_RHI_D3D12} vulkan=${ALICE_RHI_VULKAN} metal=${ALICE_RHI_METAL}")
message(STATUS "  테스트/툴     : tests=${ALICE_BUILD_TESTS} tools=${ALICE_BUILD_TOOLS}")
message(STATUS "")
