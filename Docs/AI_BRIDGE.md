# 엔진 내장 AI 대화창

> 상태: 설계. 구현은 `CHR-01`, `CHR-09`.

## 목표

엔진을 켠 채로, 에디터 안에서 AI 에게 직접 명령합니다.

```
> 플레이어가 두 번 점프할 수 있게 해줘

AI  behaviors/player_move.behavior.yaml 을 고치겠습니다.

    variables:
   -  maxJumps: 1
   +  maxJumps: 2

   + - when: physics.grounded and jumpCount > 0
   +   do:
   +     - var.set: { name: jumpCount, value: 0 }

    검증: alice check → 오류 0

    [적용]  [문서 열기]  [취소]
```

## 왜 이게 가능한가

이 기능은 앞의 모든 설계 위에 서 있습니다.

| 필요한 것 | 어디서 오는가 |
|---|---|
| AI 가 콘텐츠를 읽고 쓴다 | 콘텐츠가 텍스트 문서다 |
| AI 가 뭘 할 수 있는지 안다 | `alice verbs --json`, `alice schema list --json` |
| AI 가 자기 결과를 확인한다 | `alice check --json` |
| 사람이 적용 전에 검토한다 | 문서라서 diff 가 읽힌다 |
| 실패하면 되돌린다 | 텍스트라서 롤백이 파일 복원이다 |

**언리얼/유니티에서 이 기능을 만들 수 없는 이유**도 같은 표에서 나옵니다.
바이너리 애셋에는 diff 가 없고, 확인할 방법이 없고, 되돌릴 안전한 지점이 없습니다.

## 구조

```
┌─────────────────────────────────────────────┐
│  엔진 (메인 스레드)                          │
│    에디터 대화창 UI                          │
│         │                                   │
│    BridgeQueue  ← 스레드 경계                │
└─────────┼───────────────────────────────────┘
          │
┌─────────┼───────────────────────────────────┐
│  Bridge 워커 스레드                          │
│    프로세스 스폰: claude -p --output-format  │
│                          stream-json         │
│    ← stdout 스트림 파싱                      │
│    → 도구 호출을 큐에 넣음                    │
└─────────────────────────────────────────────┘
```

스레드 마샬링 구조는 SpartanEngine 의 `McpQueue` 에서 가져왔습니다.
AI 응답은 임의 시점에 오지만 엔진 상태는 메인 스레드에서만 만질 수 있습니다.

## AI 에게 주는 도구

**별도 API 를 만들지 않습니다. `alice` CLI 그 자체를 줍니다.**

```
Bash(alice check *)
Bash(alice schema *)
Bash(alice verbs *)
Bash(alice new *)
Bash(alice explain *)
Read / Edit / Write  (콘텐츠 디렉터리 안으로 제한)
```

왜 이렇게 하는가:

1. **이미 있는 것을 두 번 만들지 않는다.** CLI 가 이미 `--json` 을 낸다
2. **어긋날 수 없다.** CLI 가 바뀌면 AI 도구도 같이 바뀐다
3. **사람이 검증할 수 있다.** AI 가 뭘 했는지 터미널에서 그대로 재현된다
4. **AI 가 이미 안다.** Claude Code 와 Codex 는 CLI 도구를 쓰는 데 익숙하다

여기에 엔진 상태 질의를 더합니다.

```
alice engine logs --since=<프레임> --level=warn --json
alice engine profile --json
alice engine dump --scene --json
```

이 세 개는 엔진이 떠 있을 때만 동작합니다. Bridge 가 로컬 소켓으로 엔진에 묻습니다.

## 안전 장치

| 장치 | 왜 |
|---|---|
| 콘텐츠 디렉터리 밖 쓰기 금지 | AI 가 엔진 소스를 고치면 안 된다 |
| 적용 전 diff 미리보기 | 사람이 마지막 판단을 한다 |
| 적용 후 자동 `alice check` | 검증 실패면 자동 롤백 |
| 적용 전 스냅샷 | git 이 없어도 되돌릴 수 있다 |
| 프로세스 타임아웃과 취소 | 멈춘 AI 가 엔진을 잡아두면 안 된다 |
| 네트워크 없이도 엔진 동작 | 이 기능은 부가 기능이지 필수가 아니다 |

## 대화 기록

```
.alice/conversations/
  2026-09-07-141530.jsonl
```

NDJSON 으로 남깁니다. 다음 세션이 이어받을 수 있고, PR 에 붙일 요약을 뽑을 수 있습니다.
(개발 규칙 3번 — "AI 와 나눈 대화를 요약해 붙여주면 고맙겠습니다")

## claude / codex 감지

```bash
alice doctor --json
# → { "aiCli": { "claude": true, "codex": true } }
```

`alice doctor` 가 PATH 를 확인합니다. 둘 다 없으면 대화창은 비활성화되고,
엔진의 나머지 기능은 그대로 동작합니다.

| CLI | 호출 방식 |
|---|---|
| `claude` | `claude -p "<prompt>" --output-format stream-json --allowedTools ...` |
| `codex` | `codex exec "<prompt>" --json` |

둘의 스트림 형식이 다르므로 어댑터를 하나씩 둡니다.
새 AI CLI 가 나오면 어댑터를 하나 더 붙이면 됩니다.

## 반대 방향 — MCP 서버 (`CHR-09`)

엔진 안에서 AI 를 부르는 것과 반대로, 밖의 AI 가 엔진을 조종하는 길도 엽니다.

```json
// Claude Desktop 설정
{
  "mcpServers": {
    "alice": { "command": "alice", "args": ["mcp"] }
  }
}
```

Claude Desktop, Cursor 등에서 이 엔진을 도구로 씁니다.
`alice --json` 이 이미 기계용 인터페이스이므로, MCP 는 그 위의 얇은 껍데기입니다.

## 열려 있는 문제

- **컨텍스트 크기.** 큰 프로젝트의 문서를 전부 보낼 수 없습니다.
  `alice schema list` 와 `alice verbs` 를 먼저 주고, 필요한 문서만 읽게 합니다
- **비결정성.** 같은 요청에 다른 결과가 나올 수 있습니다.
  그래서 항상 diff 를 보여주고 사람이 승인합니다
- **비용.** 매 요청이 토큰을 씁니다. 로컬에서 되는 것(검증, 스키마 조회)은 AI 를 안 부릅니다
