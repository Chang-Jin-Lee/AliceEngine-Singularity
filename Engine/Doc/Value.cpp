// SPDX-License-Identifier: MIT
#include "Value.h"

#include "Foundation/StringUtil.h"

#include <cmath>

namespace alice::doc {

const char* ToString(Kind k) noexcept {
    switch (k) {
        case Kind::Null:   return "null";
        case Kind::Bool:   return "bool";
        case Kind::Int:    return "int";
        case Kind::Float:  return "float";
        case Kind::String: return "string";
        case Kind::Seq:    return "sequence";
        case Kind::Map:    return "map";
    }
    return "unknown";
}

// ── 생성 ───────────────────────────────────────────────────────────────────
Value::Value() noexcept : m_kind(Kind::Null) {}
Value::Value(std::nullptr_t) noexcept : m_kind(Kind::Null) {}

Value::Value(bool v) noexcept : m_kind(Kind::Bool) { m_scalar.b = v; }
Value::Value(i32 v) noexcept  : m_kind(Kind::Int)  { m_scalar.i = v; }
Value::Value(i64 v) noexcept  : m_kind(Kind::Int)  { m_scalar.i = v; }
Value::Value(u32 v) noexcept  : m_kind(Kind::Int)  { m_scalar.i = static_cast<i64>(v); }
Value::Value(u64 v) noexcept  : m_kind(Kind::Int)  { m_scalar.i = static_cast<i64>(v); }
Value::Value(f32 v) noexcept  : m_kind(Kind::Float){ m_scalar.f = static_cast<f64>(v); }
Value::Value(f64 v) noexcept  : m_kind(Kind::Float){ m_scalar.f = v; }

Value::Value(std::string v)      : m_kind(Kind::String), m_string(std::move(v)) {}
Value::Value(std::string_view v) : m_kind(Kind::String), m_string(v) {}
Value::Value(const char* v)      : m_kind(Kind::String), m_string(v ? v : "") {}

Value Value::MakeSeq() {
    Value v;
    v.m_kind = Kind::Seq;
    v.m_seq  = std::make_unique<Seq>();
    return v;
}

Value Value::MakeSeq(std::vector<Value> items) {
    Value v;
    v.m_kind = Kind::Seq;
    v.m_seq  = std::make_unique<Seq>(std::move(items));
    return v;
}

Value Value::MakeMap() {
    Value v;
    v.m_kind = Kind::Map;
    v.m_map  = std::make_unique<Map>();
    return v;
}

Value::Value(const Value& other) { CopyFrom(other); }

Value::Value(Value&& other) noexcept
    : mark(other.mark),
      m_kind(other.m_kind),
      m_scalar(other.m_scalar),
      m_string(std::move(other.m_string)),
      m_seq(std::move(other.m_seq)),
      m_map(std::move(other.m_map)) {
    other.m_kind = Kind::Null;
}

Value& Value::operator=(const Value& other) {
    if (this != &other) { Destroy(); CopyFrom(other); }
    return *this;
}

Value& Value::operator=(Value&& other) noexcept {
    if (this != &other) {
        mark     = other.mark;
        m_kind   = other.m_kind;
        m_scalar = other.m_scalar;
        m_string = std::move(other.m_string);
        m_seq    = std::move(other.m_seq);
        m_map    = std::move(other.m_map);
        other.m_kind = Kind::Null;
    }
    return *this;
}

Value::~Value() = default;

void Value::Destroy() noexcept {
    m_kind = Kind::Null;
    m_string.clear();
    m_seq.reset();
    m_map.reset();
}

void Value::CopyFrom(const Value& other) {
    mark     = other.mark;
    m_kind   = other.m_kind;
    m_scalar = other.m_scalar;
    m_string = other.m_string;
    m_seq    = other.m_seq ? std::make_unique<Seq>(*other.m_seq) : nullptr;
    m_map    = other.m_map ? std::make_unique<Map>(*other.m_map) : nullptr;
}

const Value& Value::Null() noexcept {
    static const Value s_null;
    return s_null;
}

// ── 스칼라 ─────────────────────────────────────────────────────────────────
bool Value::AsBool(bool fallback) const noexcept {
    return m_kind == Kind::Bool ? m_scalar.b : fallback;
}

i64 Value::AsInt(i64 fallback) const noexcept {
    if (m_kind == Kind::Int) return m_scalar.i;
    // 정확히 정수인 실수만 받아준다. 3.5 를 3 으로 조용히 깎으면 버그가 숨는다.
    if (m_kind == Kind::Float) {
        const f64 f = m_scalar.f;
        if (std::isfinite(f) && f == std::floor(f)) return static_cast<i64>(f);
    }
    return fallback;
}

f64 Value::AsFloat(f64 fallback) const noexcept {
    if (m_kind == Kind::Float) return m_scalar.f;
    if (m_kind == Kind::Int)   return static_cast<f64>(m_scalar.i);
    return fallback;
}

const std::string& Value::AsString() const noexcept {
    static const std::string s_empty;
    return m_kind == Kind::String ? m_string : s_empty;
}

std::string Value::ToScalarText() const {
    switch (m_kind) {
        case Kind::Null:   return "null";
        case Kind::Bool:   return m_scalar.b ? "true" : "false";
        case Kind::Int:    { std::string o; detail::FormatAppend(o, m_scalar.i); return o; }
        case Kind::Float:  return FormatDouble(m_scalar.f);
        case Kind::String: return m_string;
        case Kind::Seq:    return Fmt("<sequence of {}>", static_cast<u64>(Size()));
        case Kind::Map:    return Fmt("<map of {}>", static_cast<u64>(Size()));
    }
    return "?";
}

// ── 시퀀스 ─────────────────────────────────────────────────────────────────
usize Value::Size() const noexcept {
    if (m_kind == Kind::Seq && m_seq) return m_seq->size();
    if (m_kind == Kind::Map && m_map) return m_map->size();
    return 0;
}

const Value& Value::At(usize index) const noexcept {
    if (m_kind == Kind::Seq && m_seq && index < m_seq->size()) return (*m_seq)[index];
    return Null();
}

Value& Value::At(usize index) {
    ALICE_ASSERT(m_kind == Kind::Seq && m_seq && index < m_seq->size(), "시퀀스 범위를 벗어났다");
    return (*m_seq)[index];
}

void Value::Push(Value v) {
    if (m_kind != Kind::Seq) {
        Destroy();
        m_kind = Kind::Seq;
        m_seq  = std::make_unique<Seq>();
    }
    m_seq->push_back(std::move(v));
}

const Seq& Value::Items() const noexcept {
    static const Seq s_empty;
    return (m_kind == Kind::Seq && m_seq) ? *m_seq : s_empty;
}

Seq& Value::Items() {
    ALICE_ASSERT(m_kind == Kind::Seq && m_seq, "시퀀스가 아니다");
    return *m_seq;
}

// ── 맵 ─────────────────────────────────────────────────────────────────────
const Value* Value::Find(std::string_view key) const noexcept {
    if (m_kind != Kind::Map || !m_map) return nullptr;
    for (const MapEntry& e : *m_map) {
        if (e.key == key) return &e.value;
    }
    return nullptr;
}

Value* Value::Find(std::string_view key) noexcept {
    if (m_kind != Kind::Map || !m_map) return nullptr;
    for (MapEntry& e : *m_map) {
        if (e.key == key) return &e.value;
    }
    return nullptr;
}

bool Value::Has(std::string_view key) const noexcept { return Find(key) != nullptr; }

const Value& Value::operator[](std::string_view key) const noexcept {
    const Value* v = Find(key);
    return v ? *v : Null();
}

Value& Value::operator[](std::string_view key) {
    if (m_kind != Kind::Map) {
        Destroy();
        m_kind = Kind::Map;
        m_map  = std::make_unique<Map>();
    }
    if (Value* v = Find(key)) return *v;
    m_map->push_back(MapEntry{std::string(key), Value{}});
    return m_map->back().value;
}

void Value::Set(std::string key, Value v) {
    if (m_kind != Kind::Map) {
        Destroy();
        m_kind = Kind::Map;
        m_map  = std::make_unique<Map>();
    }
    if (Value* existing = Find(key)) { *existing = std::move(v); return; }
    m_map->push_back(MapEntry{std::move(key), std::move(v)});
}

bool Value::Remove(std::string_view key) {
    if (m_kind != Kind::Map || !m_map) return false;
    for (auto it = m_map->begin(); it != m_map->end(); ++it) {
        if (it->key == key) { m_map->erase(it); return true; }
    }
    return false;
}

const Map& Value::Entries() const noexcept {
    static const Map s_empty;
    return (m_kind == Kind::Map && m_map) ? *m_map : s_empty;
}

Map& Value::Entries() {
    ALICE_ASSERT(m_kind == Kind::Map && m_map, "맵이 아니다");
    return *m_map;
}

std::vector<std::string> Value::Keys() const {
    std::vector<std::string> keys;
    for (const MapEntry& e : Entries()) keys.push_back(e.key);
    return keys;
}

// ── 경로 ───────────────────────────────────────────────────────────────────
const Value* Value::AtPath(std::string_view path) const noexcept {
    const Value* cur = this;
    usize i = 0;

    while (i < path.size() && cur) {
        if (path[i] == '.') { ++i; continue; }

        if (path[i] == '[') {
            const usize close = path.find(']', i);
            if (close == std::string_view::npos) return nullptr;
            i64 index = 0;
            if (!ParseI64(path.substr(i + 1, close - i - 1), index) || index < 0) return nullptr;
            if (!cur->IsSeq() || static_cast<usize>(index) >= cur->Size()) return nullptr;
            cur = &cur->At(static_cast<usize>(index));
            i = close + 1;
            continue;
        }

        // 다음 '.' 또는 '[' 까지가 키다.
        usize end = i;
        while (end < path.size() && path[end] != '.' && path[end] != '[') ++end;
        const std::string_view key = path.substr(i, end - i);
        if (key.empty()) return nullptr;
        cur = cur->Find(key);
        i = end;
    }
    return cur;
}

// ── 비교 ───────────────────────────────────────────────────────────────────
bool Value::DeepEquals(const Value& other) const noexcept {
    if (m_kind != other.m_kind) return false;
    switch (m_kind) {
        case Kind::Null:   return true;
        case Kind::Bool:   return m_scalar.b == other.m_scalar.b;
        case Kind::Int:    return m_scalar.i == other.m_scalar.i;
        case Kind::Float:
            // NaN 은 자기 자신과 같다고 본다. 왕복 테스트에서 NaN 이 나오면 실패해야 하는 건
            // "같지 않아서"가 아니라 애초에 NaN 을 문서에 넣은 쪽이다.
            if (std::isnan(m_scalar.f) && std::isnan(other.m_scalar.f)) return true;
            return m_scalar.f == other.m_scalar.f;
        case Kind::String: return m_string == other.m_string;
        case Kind::Seq: {
            if (Size() != other.Size()) return false;
            for (usize i = 0; i < Size(); ++i) {
                if (!At(i).DeepEquals(other.At(i))) return false;
            }
            return true;
        }
        case Kind::Map: {
            if (Size() != other.Size()) return false;
            const Map& a = Entries();
            const Map& b = other.Entries();
            // 순서까지 같아야 한다. 이 엔진에서 키 순서는 의미 있는 정보다.
            for (usize i = 0; i < a.size(); ++i) {
                if (a[i].key != b[i].key) return false;
                if (!a[i].value.DeepEquals(b[i].value)) return false;
            }
            return true;
        }
    }
    return false;
}

} // namespace alice::doc
