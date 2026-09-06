// SPDX-License-Identifier: MIT
#include "Schema.h"

namespace alice::schema {

const char* ToString(Type t) noexcept {
    switch (t) {
        case Type::Any:    return "any";
        case Type::Null:   return "null";
        case Type::Bool:   return "bool";
        case Type::Int:    return "int";
        case Type::Float:  return "float";
        case Type::Number: return "number";
        case Type::String: return "string";
        case Type::Seq:    return "sequence";
        case Type::Map:    return "map";
        case Type::Ref:    return "ref";
        case Type::Union:  return "union";
    }
    return "any";
}

const char* ToString(TextFormat f) noexcept {
    switch (f) {
        case TextFormat::None:       return "none";
        case TextFormat::Identifier: return "identifier";
        case TextFormat::AssetPath:  return "asset-path";
        case TextFormat::SchemaId:   return "schema-id";
        case TextFormat::VerbId:     return "verb-id";
        case TextFormat::ColorHex:   return "color-hex";
        case TextFormat::Expression: return "expression";
    }
    return "none";
}

const Field* Schema::FindField(std::string_view name) const noexcept {
    for (const Field& f : fields) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

const Field* Schema::FindFieldByAlias(std::string_view name) const noexcept {
    for (const Field& f : fields) {
        for (const std::string& a : f.aliases) {
            if (a == name) return &f;
        }
    }
    return nullptr;
}

std::vector<std::string> Schema::FieldNames() const {
    std::vector<std::string> names;
    names.reserve(fields.size());
    for (const Field& f : fields) {
        if (f.deprecatedBy.empty()) names.push_back(f.name);
    }
    return names;
}

std::vector<std::string> Schema::RequiredFieldNames() const {
    std::vector<std::string> names;
    for (const Field& f : fields) {
        if (f.required) names.push_back(f.name);
    }
    return names;
}

} // namespace alice::schema
