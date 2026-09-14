# SPDX-License-Identifier: MIT
# alice — 콘텐츠 도구이자 이 엔진의 AI 접점.
#
# 실행 파일 이름을 짧게 두는 이유: AI 가 부를 명령이라 타이핑/토큰 비용이 그대로 든다.

alice_executable(alice
    FOLDER "Tools"
    SOURCES
        Main.cpp
        CommandLine.h
        CommandLine.cpp
        Commands.h
        Commands_Doc.cpp
        Commands_Info.cpp
    DEPS
        Alice.Foundation
        Alice.Doc
        Alice.Schema
        Alice.Verbs
        Alice.RHI
)
