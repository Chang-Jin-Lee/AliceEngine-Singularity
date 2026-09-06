# SPDX-License-Identifier: MIT
# RHI — 백엔드 중립 그래픽 계층.
#
# 백엔드는 CMake 옵션으로 켜고 끈다. 켜진 것만 컴파일되고, 각자 자기를 런타임에 등록한다.
# 콘텐츠와 게임 코드에는 #ifdef 가 하나도 없다 — 그게 "쉽고 간단하게 전 플랫폼"의 의미다.
#
# 지금 있는 것 : Null (헤드리스 검증 백엔드)
# 백로그       : D3D11 → MON-02, D3D12 → MON-03, Vulkan → MON-04, Metal → MON-05

set(ALICE_RHI_SOURCES
    RHITypes.h
    RHITypes.cpp
    RHIDevice.h
    RHIBackend.h
    RHIBackend.cpp
)

set(ALICE_RHI_LIBS "")
set(ALICE_RHI_DEFINES "")

if(ALICE_RHI_NULL)
    list(APPEND ALICE_RHI_SOURCES NullDevice.cpp)
    list(APPEND ALICE_RHI_DEFINES ALICE_RHI_HAS_NULL=1)
endif()

# 아래 백엔드들은 소스가 생기면 여기에 붙는다. 지금은 옵션만 있고 구현이 없으므로
# 켜져 있어도 조용히 건너뛴다 — 있지도 않은 파일을 참조해 설정 단계에서 깨지는 것보다 낫다.
if(ALICE_RHI_D3D11 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/D3D11/D3D11Device.cpp")
    file(GLOB ALICE_D3D11_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/D3D11/*.cpp"
                                  "${CMAKE_CURRENT_SOURCE_DIR}/D3D11/*.h")
    list(APPEND ALICE_RHI_SOURCES ${ALICE_D3D11_SOURCES})
    list(APPEND ALICE_RHI_LIBS d3d11 dxgi dxguid)
    list(APPEND ALICE_RHI_DEFINES ALICE_RHI_HAS_D3D11=1)
endif()

if(ALICE_RHI_D3D12 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/D3D12/D3D12Device.cpp")
    file(GLOB ALICE_D3D12_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/D3D12/*.cpp"
                                  "${CMAKE_CURRENT_SOURCE_DIR}/D3D12/*.h")
    list(APPEND ALICE_RHI_SOURCES ${ALICE_D3D12_SOURCES})
    list(APPEND ALICE_RHI_LIBS d3d12 dxgi dxguid)
    list(APPEND ALICE_RHI_DEFINES ALICE_RHI_HAS_D3D12=1)
endif()

if(ALICE_RHI_VULKAN AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vulkan/VulkanDevice.cpp")
    find_package(Vulkan)
    if(Vulkan_FOUND)
        file(GLOB ALICE_VULKAN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Vulkan/*.cpp"
                                       "${CMAKE_CURRENT_SOURCE_DIR}/Vulkan/*.h")
        list(APPEND ALICE_RHI_SOURCES ${ALICE_VULKAN_SOURCES})
        list(APPEND ALICE_RHI_LIBS Vulkan::Vulkan)
        list(APPEND ALICE_RHI_DEFINES ALICE_RHI_HAS_VULKAN=1)
    else()
        message(WARNING "ALICE_RHI_VULKAN 이 켜져 있지만 Vulkan SDK 를 찾지 못했다. 건너뛴다")
    endif()
endif()

if(ALICE_RHI_METAL AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Metal/MetalDevice.mm")
    file(GLOB ALICE_METAL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Metal/*.mm"
                                  "${CMAKE_CURRENT_SOURCE_DIR}/Metal/*.h")
    list(APPEND ALICE_RHI_SOURCES ${ALICE_METAL_SOURCES})
    list(APPEND ALICE_RHI_LIBS "-framework Metal" "-framework QuartzCore" "-framework Foundation")
    list(APPEND ALICE_RHI_DEFINES ALICE_RHI_HAS_METAL=1)
endif()

alice_module(Alice.RHI
    FOLDER "Engine"
    DEPS Alice.Foundation ${ALICE_RHI_LIBS}
    SOURCES ${ALICE_RHI_SOURCES}
)

# 헤더가 백엔드 유무를 알아야 링커 훅을 선언할 수 있다 → PUBLIC.
target_compile_definitions(Alice.RHI PUBLIC ${ALICE_RHI_DEFINES})
