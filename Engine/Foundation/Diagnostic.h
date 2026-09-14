// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Foundation/Diagnostic.h
//
// 이 엔진에서 "에러 메시지"는 사람이 읽는 문장이 아니라 **기계가 읽는 레코드**다.
// 콘텐츠가 전부 텍스트 문서(YAML/JSON)이므로, 진단이 정확한 줄·열·경로·수정힌트를
// 들고 있어야 AI가 사람 개입 없이 스스로 고칠 수 있다. 이것이 엔진의 핵심 계약이다.
//
// 진단 하나는 다음 질문에 답할 수 있어야 한다:
//   1. 어느 파일 어느 줄인가            → file, mark
//   2. 문서 안 어느 값인가              → path ("actors[0].transform.position")
//   3. 무엇이 잘못됐는가 (안정 식별자)   → code ("doc.schema.unknown_field")
//   4. 사람에게 뭐라고 설명하나          → message
//   5. **어떻게 고치나**                → hint  ("did you mean 'position'?")
#pragma once

#include "Core.h"

#include <string>
#include <string_view>
#include <vector>

namespace alice {

enum class Severity : u8 {
    Note = 0,
    Warning,
    Error,
};

const char* ToString(Severity s) noexcept;

/// 텍스트 문서 안의 위치. 1-기반(에디터/AI가 그대로 쓸 수 있게).
struct Mark {
    u32 line   = 0;   ///< 0 이면 위치 정보 없음
    u32 column = 0;
    u32 offset = 0;   ///< 파일 시작으로부터의 바이트 오프셋

    bool Valid() const noexcept { return line != 0; }
};

struct Diagnostic {
    Severity    severity = Severity::Error;
    std::string code;      ///< 안정 식별자. 절대 바뀌지 않는다. 예) "doc.parse.tab_indent"
    std::string message;   ///< 한 문장. 마침표 없음. 소문자로 시작.
    std::string hint;      ///< 수정 방법. 비어 있을 수 있다.
    std::string file;      ///< 소스 파일 경로 (문서 경로)
    std::string path;      ///< 문서 내부 경로. 예) "actors[0].components.mesh.asset"
    Mark        mark;      ///< 소스 위치
    std::string snippet;   ///< 해당 줄 원문 (사람이 볼 때만 씀)

    /// "file:line:col: error[code]: message" 형태의 한 줄.
    std::string ToLine() const;

    /// 캐럿(^)까지 그린 여러 줄 표현. 사람용.
    std::string ToPretty() const;

    /// 기계용 JSON 한 줄. AI는 이걸 읽는다.
    std::string ToJson() const;
};

/// 진단 모음. 파싱/검증/임포트가 전부 이걸 채워서 돌려준다.
/// 첫 에러에서 멈추지 않는다 — AI에게는 "한 번에 전부"가 훨씬 낫다.
class DiagnosticBag {
public:
    void Add(Diagnostic d);

    Diagnostic& Error(std::string code, std::string message);
    Diagnostic& Warning(std::string code, std::string message);
    Diagnostic& Note(std::string code, std::string message);

    bool  HasErrors()   const noexcept { return m_errorCount > 0; }
    usize ErrorCount()  const noexcept { return m_errorCount; }
    usize WarningCount() const noexcept { return m_warningCount; }
    bool  Empty()       const noexcept { return m_items.empty(); }
    usize Size()        const noexcept { return m_items.size(); }

    const std::vector<Diagnostic>& Items() const noexcept { return m_items; }
    std::vector<Diagnostic>&       Items()       noexcept { return m_items; }

    void Clear() noexcept;
    void Append(const DiagnosticBag& other);

    /// 모든 진단에 파일 경로를 채운다(파서는 경로를 모를 수 있으므로 상위에서 주입).
    void SetFileIfEmpty(const std::string& file);

    /// 원본 텍스트를 주면 각 진단에 snippet 을 채워 넣는다.
    void AttachSnippets(const std::string& sourceText);

    std::string ToPretty() const;
    /// NDJSON. 줄마다 진단 하나. AI 파이프라인이 그대로 흘려 읽는다.
    std::string ToJsonLines() const;
    /// {"diagnostics":[...]} 형태의 단일 JSON 객체.
    std::string ToJsonObject() const;

private:
    std::vector<Diagnostic> m_items;
    usize m_errorCount   = 0;
    usize m_warningCount = 0;
};

/// 후보 목록 중 name 과 가장 가까운 것을 고른다(오타 힌트용).
/// 편집 거리가 임계값을 넘으면 빈 문자열.
std::string ClosestMatch(std::string_view name, const std::vector<std::string>& candidates);

} // namespace alice
