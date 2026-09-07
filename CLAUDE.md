# CLAUDE.md

**이 저장소의 에이전트 지침은 [`AGENTS.md`](AGENTS.md) 에 있습니다. 먼저 그것을 읽으십시오.**

지침을 두 벌로 유지하면 반드시 어긋납니다. `AGENTS.md` 가 정본이고,
이 파일에는 Claude Code 에만 해당하는 것만 적습니다.

---

## Claude Code 에서 시작하기

```
넌 이제부터 Seeho다.
AGENTS.md 와 Agents/Seeho.md 를 읽고,
Agents/Backlog/seeho/ 에서 작업 하나를 골라 진행하라.
```

또는 작업을 지정해서:

```
넌 이제부터 Monday다. Agents/Backlog/monday/MON-02.md 를 진행하라.
```

## Claude Code 특이사항

**Bash 도구의 heredoc.** 이 저장소 작업 중 여러 줄 heredoc 안의 `\\` 가 `\` 로 접히는
일이 있었습니다. C++ 소스에 `'\\n'` 같은 이스케이프를 쓸 때는 Write/Edit 도구를 쓰거나,
파이썬 스크립트를 파일로 만들어 실행하십시오.

**한글 출력.** MSVC 의 진단은 CP949 로 나옵니다. 빌드 로그를 grep 할 때
`export LC_ALL=C` 를 붙이면 `sort`/`grep` 이 멀티바이트에서 죽지 않습니다.

**빌드는 VS 개발자 환경이 필요합니다.** `Scripts/build.ps1` 이 `vswhere` 로 찾아
`Enter-VsDevShell` 까지 해줍니다. Bash 에서 직접 `cmake` 를 부르면
`No CMAKE_CXX_COMPILER could be found` 가 납니다.

## 그 외

나머지는 전부 [`AGENTS.md`](AGENTS.md) 에 있습니다.

- 역할과 백로그
- 절대 규칙 다섯
- 커밋과 PR 규칙
- 커밋 전 검증
- 자주 하는 실수
