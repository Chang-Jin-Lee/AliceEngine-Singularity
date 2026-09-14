#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: MIT
"""백로그 상태를 집계한다.

여러 컴퓨터에서 동시에 작업하는 구조에서, 어느 작업이 잡혀 있는지를 아는 것이
충돌을 막는 유일한 장치다. 손으로 갱신하는 표는 반드시 틀린다.

    python Scripts/backlog.py            # 표 출력
    python Scripts/backlog.py --json     # 기계용
    python Scripts/backlog.py --check    # README 의 표가 최신인지 (CI 용)
"""
import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BACKLOG = ROOT / "Agents" / "Backlog"
AGENTS = ["alice", "sidney", "monday", "chrono", "seeho"]
STATES = ["대기", "진행중", "리뷰", "완료"]


def parse_task(path):
    text = path.read_text(encoding="utf-8")
    def field(name, default=""):
        m = re.search(r"^{}:\s*(.+)$".format(name), text, re.MULTILINE)
        return m.group(1).strip() if m else default
    title = ""
    m = re.search(r"^#\s+(\S+)\s+·\s+(.+)$", text, re.MULTILINE)
    if m:
        title = m.group(2).strip()
    return {
        "id": path.stem,
        "agent": path.parent.name,
        "title": title,
        "status": field("상태", "대기"),
        "size": field("크기", "?"),
        "deps": field("선행", "없음"),
        "owner": field("담당", "-"),
        "path": str(path.relative_to(ROOT)).replace("\\", "/"),
    }


def collect():
    tasks = []
    for agent in AGENTS:
        directory = BACKLOG / agent
        if not directory.is_dir():
            continue
        for path in sorted(directory.glob("*.md")):
            tasks.append(parse_task(path))
    return tasks


def summarize(tasks):
    table = {a: {s: 0 for s in STATES} for a in AGENTS}
    for t in tasks:
        if t["agent"] in table and t["status"] in table[t["agent"]]:
            table[t["agent"]][t["status"]] += 1
    return table


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--check", action="store_true",
                        help="Agents/README.md 의 표가 최신인지 검사한다")
    args = parser.parse_args()

    tasks = collect()
    table = summarize(tasks)

    if args.json:
        print(json.dumps({"tasks": tasks, "summary": table},
                         ensure_ascii=False, indent=2))
        return 0

    if args.check:
        readme = (ROOT / "Agents" / "README.md").read_text(encoding="utf-8")
        stale = []
        for agent in AGENTS:
            waiting = table[agent]["대기"]
            active = table[agent]["진행중"]
            done = table[agent]["완료"]
            pattern = r"\[{}\]\(Backlog/{}\)\s*\|\s*{}\s*\|\s*{}\s*\|\s*{}\s*\|".format(
                agent.capitalize(), agent, waiting, active, done)
            if not re.search(pattern, readme):
                stale.append("  {}: 대기 {} · 진행중 {} · 완료 {}".format(
                    agent.capitalize(), waiting, active, done))
        if stale:
            print("Agents/README.md 의 백로그 표가 실제와 다르다:", file=sys.stderr)
            print("\n".join(stale), file=sys.stderr)
            return 1
        print("백로그 표가 최신이다.")
        return 0

    width = max(len(t["id"]) for t in tasks) if tasks else 8
    for agent in AGENTS:
        rows = [t for t in tasks if t["agent"] == agent]
        if not rows:
            continue
        print("\n{}".format(agent.capitalize()))
        for t in rows:
            print("  {:<{}}  {:<4}  {:<3}  {}".format(
                t["id"], width, t["status"], t["size"], t["title"]))

    print("\n요약")
    for agent in AGENTS:
        counts = table[agent]
        print("  {:<8} 대기 {}  진행중 {}  리뷰 {}  완료 {}".format(
            agent.capitalize(), counts["대기"], counts["진행중"],
            counts["리뷰"], counts["완료"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
