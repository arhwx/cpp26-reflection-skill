#!/usr/bin/env python3
"""Run Codex single-shot on each eval task; grade the result with grade.py.

usage: run_codex.py --out <runs_dir> [--skill <skill dir>] [--model M] [--tasks a,b] [--jobs 4]
"""

import argparse
import json
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

TASKS_DIR = Path(__file__).resolve().parent / "tasks"
INSTRUCTION = "Read prompt.md and do what it asks. Do not compile or run any code. Do not search the web."
TIMEOUT = 1800


def git_root(path):
    for p in (path, *path.parents):
        if (p / ".git").exists():
            return p
    return None


def run_task(tid, out, skill, model):
    work = out / tid
    work.mkdir(parents=True)
    shutil.copy(TASKS_DIR / tid / "prompt.md", work)
    if skill:
        shutil.copytree(skill, work / ".agents" / "skills" / skill.name)

    cmd = ["codex", "exec", "--skip-git-repo-check", "--ephemeral", "-s", "workspace-write", "-C", str(work)]
    if model:
        cmd += ["-m", model]
    cmd.append(INSTRUCTION)

    start = time.monotonic()
    with open(work / "codex.log", "wb") as log:
        try:
            code = subprocess.run(cmd, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                                  timeout=TIMEOUT).returncode
        except subprocess.TimeoutExpired:
            code = None
    seconds = round(time.monotonic() - start, 1)
    print(f"{tid}: exit {code}, {seconds} s", file=sys.stderr)
    return {"seconds": seconds, "exit_code": code, "solution": (work / "solution.hpp").is_file()}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", type=Path, required=True, help="runs directory, must be outside any git repo")
    ap.add_argument("--skill", type=Path, help="skill directory to install in each workdir")
    ap.add_argument("--model", help="model passed to codex -m")
    ap.add_argument("--tasks", help="comma separated task ids")
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()

    out = args.out.resolve()
    # inside a repo codex would pick up its AGENTS.md, skills and the reference solutions
    if root := git_root(out):
        sys.exit(f"--out must be outside a git repo (found {root})")
    skill = args.skill.resolve() if args.skill else None
    if skill and not (skill / "SKILL.md").is_file():
        sys.exit(f"no SKILL.md in {skill}")

    tasks = sorted(p.parent.name for p in TASKS_DIR.glob("*/prompt.md"))
    if args.tasks:
        only = [t for t in args.tasks.split(",") if t]
        unknown = [t for t in only if t not in tasks]
        if unknown:
            sys.exit(f"unknown tasks: {', '.join(unknown)}")
        tasks = [t for t in tasks if t in only]
    if out.exists() and any(out.iterdir()):
        sys.exit(f"{out} is not empty")

    out.mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(args.jobs) as pool:
        results = list(pool.map(lambda t: run_task(t, out, skill, args.model), tasks))

    runs = {
        "model": args.model,
        "skill": str(skill) if skill else None,
        "tasks": dict(zip(tasks, results)),
    }
    (out / "runs.json").write_text(json.dumps(runs, indent=2) + "\n")
    print(f"grade with: python3 {Path(__file__).parent / 'grade.py'} {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
