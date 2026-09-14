// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Doc/Document.h
//
// 파일 하나 = 문서 하나. 문서는 첫 줄에서 **자기가 무엇인지** 밝힌다.
//
//   schema: alice/actor/1
//   name: Player
//   ...
//
// 이 한 줄이 있는 이유:
//   • AI 가 파일을 열자마자 어떤 스키마로 검증해야 하는지 안다. 추측하지 않는다.
//   • 에디터/IDE 가 자동완성을 붙일 수 있다.
//   • 포맷이 바뀌어도 버전 번호로 마이그레이션 경로를 만들 수 있다.
//
// 확장자는 사람을 위한 것이고(`player.actor.yaml`), 판정 기준은 언제나 schema 필드다.
#pragma once

#include "Foundation/Result.h"
#include "Value.h"

namespace alice::doc {

enum class Syntax : u8 {
    Auto = 0,   ///< 확장자와 내용으로 판정
    Yaml,
    Json,
};

const char* ToString(Syntax s) noexcept;

struct Document {
    std::string path;        ///< 원본 파일 경로 (메모리 문서면 논리적 이름)
    std::string text;        ///< 원본 텍스트 — 진단 스니펫과 diff 에 쓴다
    Syntax      syntax = Syntax::Yaml;
    Value       root;

    /// root["schema"] 의 값. 없으면 빈 문자열.
    std::string SchemaId() const;

    /// "alice/actor/1" → ("alice/actor", 1). 버전이 없으면 0.
    static bool SplitSchemaId(std::string_view id, std::string& outBase, u32& outVersion);
};

/// 확장자와 첫 비어있지 않은 글자로 형식을 추정한다.
Syntax DetectSyntax(std::string_view path, std::string_view text);

/// 텍스트에서 문서를 만든다. sourceName 은 진단에 실릴 이름이다.
bool ParseDocument(std::string_view text,
                   Syntax syntax,
                   std::string sourceName,
                   Document& outDocument,
                   DiagnosticBag& diagnostics);

/// 파일에서 읽어 파싱한다.
bool LoadDocument(const std::string& path,
                  Document& outDocument,
                  DiagnosticBag& diagnostics,
                  Syntax syntax = Syntax::Auto);

/// 문서를 파일로 쓴다. format 이 Auto 면 문서 자신의 형식을 쓴다.
Status SaveDocument(const Document& document,
                    const std::string& path,
                    Syntax syntax = Syntax::Auto);

/// 문서를 텍스트로 직렬화한다.
std::string SerializeDocument(const Document& document, Syntax syntax = Syntax::Auto);

} // namespace alice::doc
