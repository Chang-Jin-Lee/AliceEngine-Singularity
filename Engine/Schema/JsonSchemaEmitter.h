// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Schema/JsonSchemaEmitter.h
//
// 내부 스키마를 **표준 JSON Schema** 로 뽑는다.
//
// 왜 굳이 두 벌인가:
//   내부 스키마는 우리 진단(오타 제안, pitfall, alias)을 위해 존재한다. JSON Schema 는
//   그런 걸 표현하지 못한다. 대신 바깥 세계 전부가 JSON Schema 를 읽는다 —
//   VS Code YAML 확장, JetBrains, 온갖 린터, 그리고 대부분의 AI 도구 체인.
//
//   그래서 소스는 하나(C++ 스키마)로 두고, 바깥용 표현을 **생성**한다.
//   손으로 두 벌을 유지하면 반드시 어긋난다.
//
// 생성물의 쓰임:
//   Schemas/alice-actor-1.schema.json   ← 에디터 자동완성
//   Schemas/index.json                  ← 스키마 목록 (AI 가 먼저 읽는 파일)
#pragma once

#include "Foundation/Result.h"
#include "Registry.h"

namespace alice::schema {

/// 스키마 하나를 JSON Schema(draft 2020-12) 문서로.
std::string EmitJsonSchema(const Schema& schema, const Registry& registry);

/// 레지스트리 전체를 $defs 로 묶은 단일 문서로.
std::string EmitBundle(const Registry& registry);

/// 스키마 id 를 파일명으로. "alice/actor/1" → "alice-actor-1.schema.json"
std::string SchemaFileName(std::string_view id);

/// 디렉터리에 스키마 파일 일체를 쓴다. index.json 과 .vscode 매핑 힌트도 함께 만든다.
Status WriteSchemaFiles(const Registry& registry, const std::string& outputDirectory);

} // namespace alice::schema
