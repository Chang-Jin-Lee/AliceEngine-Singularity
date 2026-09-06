// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Tools/AliceCLI/Commands.h
//
// `alice` 는 이 엔진의 **AI 접점**이다.
//
// 언리얼에서 "이 애셋이 올바른가"를 물으려면 에디터를 켜야 한다. 유니티도 마찬가지다.
// 그래서 AI 는 콘텐츠를 만들어 놓고도 그것이 맞는지 확인할 방법이 없다. 확인 없이
// 반복하면 오류가 누적된다.
//
// 여기서는 명령 한 줄이면 된다:
//
//   alice check Content/            # 전부 검증. 틀린 곳과 고치는 법이 나온다
//   alice check Content/ --json     # 같은 내용을 기계가 읽는 형태로
//   alice verbs --json              # 쓸 수 있는 동작 전부
//   alice schema show alice/actor/1 # 이 문서에 뭘 적을 수 있는지
//   alice new material Steel        # 올바른 뼈대를 만들어 준다
//   alice explain doc.parse.tab_indent
//
// 종료 코드: 0 = 성공, 1 = 검증 실패, 2 = 사용법 오류.
#pragma once

#include "CommandLine.h"

namespace alice::cli {

int RunCheck(const Args& args);      ///< 문서 검증 (스키마 + 동사 + 조건식)
int RunFormat(const Args& args);     ///< 문서 정규화. diff 를 안정시킨다
int RunConvert(const Args& args);    ///< YAML ↔ JSON
int RunNew(const Args& args);        ///< 스키마에서 문서 뼈대 생성

int RunSchema(const Args& args);     ///< list / show / emit
int RunVerbs(const Args& args);      ///< 동사 목록과 상세, 식 심볼
int RunExplain(const Args& args);    ///< 진단 코드 설명
int RunDoctor(const Args& args);     ///< 환경 점검
int RunVersion(const Args& args);
int RunHelp(const Args& args);

/// 진단 코드 하나의 설명. `alice explain` 과 위키가 같은 표를 쓴다.
struct CodeExplanation {
    const char* code;
    const char* summary;
    const char* why;       ///< 왜 이 규칙이 있는가
    const char* wrong;     ///< 잘못된 예
    const char* right;     ///< 고친 예
};

const CodeExplanation* FindExplanation(std::string_view code);
const CodeExplanation* AllExplanations(usize& outCount);

} // namespace alice::cli
