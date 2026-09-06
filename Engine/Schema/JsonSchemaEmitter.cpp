// SPDX-License-Identifier: MIT
#include "JsonSchemaEmitter.h"

#include "Doc/Writer.h"
#include "Foundation/FileSystem.h"
#include "Foundation/Log.h"
#include "Foundation/StringUtil.h"

namespace alice::schema {
namespace {

constexpr const char* kChannel = "schema";
constexpr const char* kDialect = "https://json-schema.org/draft/2020-12/schema";

/// JSON Schema 트리를 doc::Value 로 짓는다.
/// 문자열을 직접 이어붙이지 않는 이유: 이스케이프와 쉼표를 손으로 다루면 반드시 틀린다.
/// Value 로 지으면 Doc 모듈의 직렬화기가 정확성을 책임진다.
Value Emit(const Schema& schema, const Registry& registry, bool topLevel);

void EmitCommon(Value& out, const Schema& schema) {
    if (!schema.title.empty())       out.Set("title", Value{schema.title});
    if (!schema.description.empty()) out.Set("description", Value{schema.description});

    if (!schema.examples.empty()) {
        Value examples = Value::MakeSeq();
        for (const Value& e : schema.examples) examples.Push(e);
        out.Set("examples", std::move(examples));
    }

    // pitfall 은 JSON Schema 표준에 없다. x- 확장으로 남겨 AI 도구가 읽을 수 있게 한다.
    if (!schema.pitfalls.empty()) {
        Value pitfalls = Value::MakeSeq();
        for (const Schema::Pitfall& p : schema.pitfalls) {
            Value item = Value::MakeMap();
            item.Set("wrong", Value{p.wrong});
            item.Set("right", Value{p.right});
            item.Set("why",   Value{p.why});
            pitfalls.Push(std::move(item));
        }
        out.Set("x-alice-pitfalls", std::move(pitfalls));
    }
}

Value EmitMap(const Schema& schema, const Registry& registry) {
    Value out = Value::MakeMap();
    out.Set("type", Value{"object"});

    Value properties = Value::MakeMap();
    Value required   = Value::MakeSeq();

    for (const Field& field : schema.fields) {
        Value prop = field.type ? Emit(*field.type, registry, false) : Value::MakeMap();
        if (!prop.IsMap()) prop = Value::MakeMap();

        if (!field.description.empty() && !prop.Has("description")) {
            prop.Set("description", Value{field.description});
        }
        if (!field.defaultValue.IsNull()) prop.Set("default", field.defaultValue);
        if (!field.deprecatedBy.empty()) {
            prop.Set("deprecated", Value{true});
            prop.Set("x-alice-use-instead", Value{field.deprecatedBy});
        }
        if (!field.aliases.empty()) {
            Value aliases = Value::MakeSeq();
            for (const std::string& a : field.aliases) aliases.Push(Value{a});
            prop.Set("x-alice-aliases", std::move(aliases));
        }

        properties.Set(field.name, std::move(prop));
        if (field.required) required.Push(Value{field.name});
    }

    out.Set("properties", std::move(properties));
    if (required.Size() > 0) out.Set("required", std::move(required));

    if (schema.additionalFields) {
        out.Set("additionalProperties",
                schema.additionalFieldType ? Emit(*schema.additionalFieldType, registry, false)
                                           : Value{true});
    } else {
        out.Set("additionalProperties", Value{false});
    }
    return out;
}

Value Emit(const Schema& schema, const Registry& registry, bool topLevel) {
    Value out;

    switch (schema.type) {
        case Type::Ref: {
            out = Value::MakeMap();
            out.Set("$ref", Value{"./" + SchemaFileName(schema.refId)});
            // 참조 대상의 설명을 끌어와 에디터 툴팁이 비지 않게 한다.
            if (SchemaPtr target = registry.Find(schema.refId)) {
                if (!target->description.empty()) {
                    out.Set("description", Value{target->description});
                }
            }
            return out;
        }

        case Type::Union: {
            out = Value::MakeMap();
            Value anyOf = Value::MakeSeq();
            for (const SchemaPtr& option : schema.options) {
                if (option) anyOf.Push(Emit(*option, registry, false));
            }
            out.Set("anyOf", std::move(anyOf));
            EmitCommon(out, schema);
            return out;
        }

        case Type::Map:
            out = EmitMap(schema, registry);
            break;

        case Type::Seq: {
            out = Value::MakeMap();
            out.Set("type", Value{"array"});
            if (schema.items) out.Set("items", Emit(*schema.items, registry, false));
            if (schema.minItems) out.Set("minItems", Value{static_cast<i64>(*schema.minItems)});
            if (schema.maxItems) out.Set("maxItems", Value{static_cast<i64>(*schema.maxItems)});
            break;
        }

        case Type::Bool:
            out = Value::MakeMap();
            out.Set("type", Value{"boolean"});
            break;

        case Type::Int:
            out = Value::MakeMap();
            out.Set("type", Value{"integer"});
            break;

        case Type::Float:
        case Type::Number:
            out = Value::MakeMap();
            out.Set("type", Value{"number"});
            break;

        case Type::Null:
            out = Value::MakeMap();
            out.Set("type", Value{"null"});
            break;

        case Type::String: {
            out = Value::MakeMap();
            out.Set("type", Value{"string"});
            if (!schema.enumValues.empty()) {
                Value values = Value::MakeSeq();
                for (const std::string& v : schema.enumValues) values.Push(Value{v});
                out.Set("enum", std::move(values));
            }
            if (schema.minLength) out.Set("minLength", Value{static_cast<i64>(*schema.minLength)});
            if (schema.maxLength) out.Set("maxLength", Value{static_cast<i64>(*schema.maxLength)});
            if (schema.format != TextFormat::None) {
                // JSON Schema 의 format 은 어휘가 정해져 있다. 우리 것은 확장 키로 낸다.
                out.Set("x-alice-format", Value{ToString(schema.format)});
            }
            break;
        }

        case Type::Any:
            out = Value::MakeMap();
            break;
    }

    if (schema.minValue) out.Set("minimum", Value{*schema.minValue});
    if (schema.maxValue) out.Set("maximum", Value{*schema.maxValue});

    EmitCommon(out, schema);

    if (topLevel && !schema.id.empty()) {
        out.Set("x-alice-schema-id", Value{schema.id});
    }
    return out;
}

} // namespace

std::string SchemaFileName(std::string_view id) {
    std::string name(id);
    for (char& c : name) {
        if (c == '/' || c == '\\' || c == ':') c = '-';
    }
    return name + ".schema.json";
}

std::string EmitJsonSchema(const Schema& schema, const Registry& registry) {
    Value root = Emit(schema, registry, true);

    // $schema 와 $id 가 맨 앞에 오도록 새 맵을 짓는다. 키 순서는 보존되므로 이게 확실하다.
    Value out = Value::MakeMap();
    out.Set("$schema", Value{kDialect});
    if (!schema.id.empty()) out.Set("$id", Value{SchemaFileName(schema.id)});
    for (const doc::MapEntry& e : root.Entries()) out.Set(e.key, e.value);

    return doc::ToJson(out);
}

std::string EmitBundle(const Registry& registry) {
    Value defs = Value::MakeMap();
    for (const SchemaPtr& schema : registry.All()) {
        if (!schema || schema->id.empty()) continue;
        defs.Set(schema->id, Emit(*schema, registry, true));
    }

    Value out = Value::MakeMap();
    out.Set("$schema", Value{kDialect});
    out.Set("$id", Value{"alice-schemas.json"});
    out.Set("title", Value{"AliceEngine-Singularity content schemas"});
    out.Set("description",
            Value{"이 엔진이 아는 모든 문서 타입. 각 항목의 키가 문서의 schema: 필드 값이다"});
    out.Set("$defs", std::move(defs));
    return doc::ToJson(out);
}

Status WriteSchemaFiles(const Registry& registry, const std::string& outputDirectory) {
    Status created = fs::CreateDirectories(outputDirectory);
    if (created.IsErr()) return created;

    Value index = Value::MakeSeq();
    usize written = 0;

    for (const SchemaPtr& schema : registry.All()) {
        if (!schema || schema->id.empty()) continue;

        const std::string fileName = SchemaFileName(schema->id);
        const std::string path     = fs::Join(outputDirectory, fileName);

        Status s = fs::WriteTextFile(path, EmitJsonSchema(*schema, registry));
        if (s.IsErr()) return s;
        ++written;

        Value entry = Value::MakeMap();
        entry.Set("id", Value{schema->id});
        entry.Set("file", Value{fileName});
        entry.Set("title", Value{schema->title});
        entry.Set("description", Value{schema->description});
        entry.Set("fields", Value{static_cast<i64>(schema->fields.size())});
        index.Push(std::move(entry));
    }

    // index.json — AI 가 가장 먼저 읽을 파일. "이 엔진으로 뭘 만들 수 있나"의 답이다.
    Value indexRoot = Value::MakeMap();
    indexRoot.Set("engine", Value{"AliceEngine-Singularity"});
    indexRoot.Set("dialect", Value{kDialect});
    indexRoot.Set("count", Value{static_cast<i64>(written)});
    indexRoot.Set("schemas", std::move(index));

    Status s = fs::WriteTextFile(fs::Join(outputDirectory, "index.json"), doc::ToJson(indexRoot));
    if (s.IsErr()) return s;

    s = fs::WriteTextFile(fs::Join(outputDirectory, "alice-schemas.bundle.json"),
                          EmitBundle(registry));
    if (s.IsErr()) return s;

    ALICE_LOG_INFO(kChannel, "schema.files.written")
        .Msg("JSON Schema 파일을 생성했다")
        .F("directory", outputDirectory)
        .F("count", static_cast<i64>(written));
    return Status::Ok();
}

} // namespace alice::schema
