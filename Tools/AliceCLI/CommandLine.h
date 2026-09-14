// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Tools/AliceCLI/CommandLine.h
//
// 인자 파서. 아주 작지만 규칙이 하나 있다: **모든 명령이 --json 을 받는다.**
//
// 사람에게는 표와 색깔을, AI 에게는 같은 내용을 JSON 으로 준다. 두 출력이 같은
// 데이터에서 나오므로 어긋날 수 없다. "AI 친화"는 별도의 API 를 만드는 게 아니라
// 있는 출력을 기계가 읽을 수 있게 만드는 일이다.
#pragma once

#include "Foundation/Core.h"

#include <map>
#include <string>
#include <vector>

namespace alice::cli {

class Args {
public:
    Args(int argc, char** argv);

    /// 첫 번째 비플래그 인자(명령 이름). 없으면 빈 문자열.
    const std::string& Command() const noexcept { return m_command; }
    /// 명령 뒤에 붙은 위치 인자들.
    const std::vector<std::string>& Positional() const noexcept { return m_positional; }

    bool        Has(std::string_view flag) const;
    std::string Get(std::string_view flag, std::string fallback = {}) const;
    i64         GetInt(std::string_view flag, i64 fallback) const;

    /// 정의되지 않은 플래그. 오타를 잡아 알려주기 위해 보관한다.
    const std::vector<std::string>& Flags() const noexcept { return m_flagOrder; }

    const std::string& ProgramName() const noexcept { return m_program; }

private:
    std::string                        m_program;
    std::string                        m_command;
    std::vector<std::string>           m_positional;
    std::map<std::string, std::string> m_flags;
    std::vector<std::string>           m_flagOrder;
};

/// 출력 모드. 전역으로 한 번 정해지고 모든 명령이 따른다.
struct OutputMode {
    bool json  = false;
    bool quiet = false;
    bool color = true;
};

OutputMode& Output();

/// 사람용 출력. json 모드에서는 아무것도 하지 않는다.
void Print(std::string_view text);
void PrintLine(std::string_view text = {});
void PrintError(std::string_view text);

/// 기계용 출력. json 모드에서만 나간다. 명령마다 정확히 한 번 부른다.
void PrintJson(std::string_view json);

/// 굵게/색깔. 터미널이 아니거나 --no-color 면 그대로 통과.
std::string Bold(std::string_view text);
std::string Dim(std::string_view text);
std::string Red(std::string_view text);
std::string Yellow(std::string_view text);
std::string Green(std::string_view text);
std::string Cyan(std::string_view text);

} // namespace alice::cli
