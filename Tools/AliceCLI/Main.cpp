// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — alice CLI 진입점
#include "Commands.h"

#include "Foundation/Diagnostic.h"
#include "Foundation/Log.h"
#include "Foundation/StringUtil.h"

#include <map>
#include <vector>

namespace {

using alice::cli::Args;

struct Command {
    const char* name;
    int (*run)(const Args&);
    const char* summary;
};

const Command kCommands[] = {
    {"check",   &alice::cli::RunCheck,   "문서를 검증한다"},
    {"fmt",     &alice::cli::RunFormat,  "문서를 정규화한다"},
    {"convert", &alice::cli::RunConvert, "YAML 과 JSON 을 오간다"},
    {"new",     &alice::cli::RunNew,     "스키마에서 문서 뼈대를 만든다"},
    {"schema",  &alice::cli::RunSchema,  "문서 타입과 필드를 보여준다"},
    {"verbs",   &alice::cli::RunVerbs,   "콘텐츠가 쓸 수 있는 동작을 보여준다"},
    {"explain", &alice::cli::RunExplain, "진단 코드를 설명한다"},
    {"doctor",  &alice::cli::RunDoctor,  "환경을 점검한다"},
    {"version", &alice::cli::RunVersion, "버전을 출력한다"},
    {"help",    &alice::cli::RunHelp,    "도움말"},
};

} // namespace

int main(int argc, char** argv) {
    Args args(argc, argv);

    // 출력 모드를 먼저 정한다. 이후의 모든 출력이 이걸 따른다.
    alice::cli::OutputMode& output = alice::cli::Output();
    output.json  = args.Has("json");
    output.quiet = args.Has("quiet") || args.Has("q");
    if (args.Has("no-color")) output.color = false;

    // CLI 는 조용해야 한다. 엔진 로그는 경고 이상만, --json 이면 아예 끈다.
    // 도구의 출력에 엔진 로그가 섞이면 파이프로 못 넘긴다.
    alice::Log::RemoveAllSinks();
    if (!output.json) {
        alice::Log::AddSink(alice::MakeConsoleSink(output.color));
        alice::LogLevel level = alice::LogLevel::Warn;
        alice::ParseLogLevel(args.Get("log-level", "warn"), level);
        alice::Log::SetLevel(level);
    } else {
        alice::Log::SetLevel(alice::LogLevel::Off);
    }

    if (args.Command().empty() || args.Has("help") || args.Has("h")) {
        return alice::cli::RunHelp(args);
    }
    if (args.Has("version") || args.Has("V")) {
        return alice::cli::RunVersion(args);
    }

    for (const Command& command : kCommands) {
        if (args.Command() == command.name) {
            const int code = command.run(args);
            alice::Log::Flush();
            return code;
        }
    }

    // 모르는 명령. 조용히 도움말만 뱉지 않고 가장 가까운 것을 제안한다.
    std::vector<std::string> names;
    for (const Command& command : kCommands) names.emplace_back(command.name);

    alice::cli::PrintError(alice::Fmt("알 수 없는 명령: {}", args.Command()));
    const std::string suggestion = alice::ClosestMatch(args.Command(), names);
    if (!suggestion.empty()) {
        alice::cli::PrintError(alice::Fmt("  '{}' 을(를) 뜻했는가?", suggestion));
    }
    alice::cli::PrintError("  쓸 수 있는 명령: " + alice::Join(names, ", "));

    if (alice::cli::Output().json) {
        std::string out = "{\"error\":\"unknown_command\",\"command\":";
        out += alice::JsonQuote(args.Command());
        out += ",\"suggestion\":";
        out += alice::JsonQuote(suggestion);
        out += "}";
        // json 모드에서는 PrintError 가 침묵하므로 여기서 직접 낸다.
        alice::cli::Output().json = true;
        alice::cli::PrintJson(out);
    }
    return 2;
}
