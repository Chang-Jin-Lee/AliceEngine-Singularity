# SPDX-License-Identifier: MIT
# 엔진 모듈 목록과 **의존성 그래프**.
#
# 이 파일이 아키텍처 문서다. 화살표는 아래로만 흐른다. 위로 흐르는 순간 CMake 가 막는다.
#
#   Foundation                 로그 · 프로파일러 · 진단 · 파일시스템   (의존 없음)
#      ├─ Doc                  문서 값 모델 · YAML/JSON 파서 · 직렬화
#      │    └─ Schema          스키마 정의 · 검증 · JSON Schema 생성
#      │         └─ Verbs      동사 레지스트리 · 조건식 · 2차 검사
#      └─ RHI                  그래픽 추상화 (백엔드 중립 인터페이스)
#
# 아직 없는 모듈(Runtime / Asset / Bridge / Editor)은 백로그에 있다 → Agents/Backlog/

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
