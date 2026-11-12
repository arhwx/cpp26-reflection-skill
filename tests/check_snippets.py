#!/usr/bin/env python3
"""Compile every ```cpp block in the skill.

A block containing `int main` is built and run. If a ```text block follows it
directly, the program output must match it. Other blocks are compiled only.

usage: python3 tests/check_snippets.py [--fork] [files...]
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKILL = ROOT / "skills" / "cpp26-reflection"

GCC = ["g++", "-std=c++26", "-freflection", "-Wall", "-Wextra"]
FORK_IMAGE = os.environ.get(
    "FORK_IMAGE",
    "yusufemresamur/clang-p2996@sha256:702ad6d6cf3e6d822b2ddba0cb97c529b9eccb9fe047adf0b8a455535a6a4ee7")
FORK = ["clang++", "-std=c++26", "-freflection-latest", "-stdlib=libc++",
        "-Werror=deprecated-declarations"]

BLOCK = re.compile(r"^```(\w+)\n(.*?)^```", re.M | re.S)


def blocks(path):
    text = path.read_text()
    found = list(BLOCK.finditer(text))
    for i, m in enumerate(found):
        if m.group(1) != "cpp":
            continue
        line = text.count("\n", 0, m.start()) + 1
        expected = None
        if i + 1 < len(found):
            nxt = found[i + 1]
            between = text[m.end():nxt.start()]
            if nxt.group(1) == "text" and between.strip() == "":
                expected = nxt.group(2)
        yield line, m.group(2), expected


def run(cmd, cwd, timeout=120):
    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return 1, "", "timeout"


def first_line(err, word):
    for line in err.splitlines():
        if word in line:
            return line.strip()[:160]
    return err.strip().splitlines()[0][:160] if err.strip() else ""


def check(code, expected, fork):
    with tempfile.TemporaryDirectory() as d:
        Path(d, "s.cpp").write_text(code)
        has_main = "int main" in code
        if fork:
            uid = f"{os.getuid()}:{os.getgid()}"
            docker = ["docker", "run", "--rm", "--user", uid, "-v", f"{d}:/src", "-w", "/src", FORK_IMAGE]
            build = docker + FORK + (["s.cpp", "-o", "s"] if has_main else ["-c", "s.cpp", "-o", "s.o"])
            exe = docker + ["./s"]
        else:
            build = GCC + (["s.cpp", "-o", "s"] if has_main else ["-c", "s.cpp", "-o", "s.o"])
            exe = ["./s"]
        rc, _, err = run(build, d)
        if rc:
            return "compile: " + first_line(err, "error")
        if "warning:" in err:
            return "warning: " + first_line(err, "warning:")
        if not has_main:
            return None
        rc, out, err = run(exe, d, 30)
        if rc:
            return f"exit {rc}"
        if expected is not None and out != expected:
            return f"output differs:\n{out}"
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fork", action="store_true", help="also build with the clang fork")
    ap.add_argument("files", nargs="*")
    args = ap.parse_args()
    files = [Path(f).resolve() for f in args.files] or sorted(SKILL.rglob("*.md"))
    jobs = [(f, line, code, exp) for f in files for line, code, exp in blocks(f)]
    targets = [False, True] if args.fork else [False]

    def one(job):
        f, line, code, exp = job
        return job, [check(code, exp, t) for t in targets]

    failed = 0
    with ThreadPoolExecutor(4) as pool:
        for (f, line, _, _), results in pool.map(one, jobs):
            name = f"{f.relative_to(ROOT)}:{line}"
            gcc = results[0]
            if gcc:
                failed += 1
            status = "ok" if not gcc else "FAIL"
            extra = ""
            if args.fork:
                extra = " fork ok" if not results[1] else f" fork: {results[1]}"
            print(f"{status:4} {name}{extra}")
            if gcc:
                print("     " + gcc.replace("\n", "\n     "))
    print(f"{len(jobs) - failed}/{len(jobs)} blocks pass")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
