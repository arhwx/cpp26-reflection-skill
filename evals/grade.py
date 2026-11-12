#!/usr/bin/env python3
"""Compile, run and lint eval solutions on GCC and the clang-p2996 fork.

usage: grade.py <solutions_dir> [--json out.json] [--tasks a,b] [--no-fork]
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import tomllib
import uuid
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

TASKS_DIR = Path(__file__).resolve().parent / "tasks"
COMPILE_TIMEOUT = 120
RUN_TIMEOUT = 10
JOBS = 4

GCC = ["g++", "-std=c++26", "-freflection"]
GCC20 = ["g++", "-std=c++20"]
FORK_IMAGE = os.environ.get(
    "FORK_IMAGE",
    "yusufemresamur/clang-p2996@sha256:702ad6d6cf3e6d822b2ddba0cb97c529b9eccb9fe047adf0b8a455535a6a4ee7")
FORK = ["clang++", "-std=c++26", "-freflection-latest", "-stdlib=libc++"]

# pre-standard identifiers and qualified names
OLD_NAMES = [
    (re.compile(p), label)
    for p, label in [
        (r"\bname_of\b", "name_of"),
        (r"\bqualified_name_of\b", "qualified_name_of"),
        (r"\bdisplay_name_of\b", "display_name_of"),
        (r"\breflect_value\b", "reflect_value"),
        (r"\bvalue_of\s*\(", "value_of"),
        (r"\bdefine_class\b", "define_class"),
        (r"\bdata_member_options_t\b", "data_member_options_t"),
        (r"\bmeta::expand\b", "meta::expand"),
        (r"\bhas_ellipsis_parameter\b", "has_ellipsis_parameter"),
        (r"\bis_consteval_only_type\b", "is_consteval_only_type"),
        (r"\btype_is_\w*", "type_is_*"),
        (r"\btest_trait\b", "test_trait"),
        (r"\boperator_symbol_of\b", "operator_symbol_of"),
        (r"\breflect_invoke\b", "reflect_invoke"),
        (r"\bmeta::define_static_\w*", "std::meta::define_static_*"),
        (r"\breflexpr\b", "reflexpr"),
    ]
]

# member queries that need an access context as second argument
CTX_CALL = re.compile(
    r"\b(members_of|nonstatic_data_members_of|static_data_members_of|bases_of|subobjects_of)\s*\("
)

# keywords after which `^` starts an expression
EXPR_KEYWORDS = {"return", "case", "co_return", "co_yield", "throw"}
EXPR_PUNCT = set("=(,{[:;<?!&|+-*/%~")


def strip_code(src):
    """Blank out comments and string/char literals, keeping newlines."""
    out = []
    i, n = 0, len(src)

    def blank(text):
        out.append(re.sub(r"[^\n]", " ", text))

    while i < n:
        c = src[i]
        if src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            blank(src[i:j])
            i = j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            blank(src[i:j])
            i = j
        elif c == '"':
            prefix = re.search(r"\w*$", src[max(0, i - 8):i]).group()
            paren = src.find("(", i)
            if prefix in ("R", "u8R", "uR", "UR", "LR") and paren > 0:
                # raw string R"delim( ... )delim"
                close = ")" + src[i + 1:paren] + '"'
                end = src.find(close, paren)
                j = n if end < 0 else end + len(close)
            else:
                j = _quoted_end(src, i, '"')
            out.append(c)
            blank(src[i + 1:j])
            i = j
        elif c == "'":
            k = i
            while k > 0 and (src[k - 1].isalnum() or src[k - 1] in "_'"):
                k -= 1
            if k < i and src[k].isdigit():
                # digit separator
                out.append(c)
                i += 1
                continue
            j = _quoted_end(src, i, "'")
            out.append(c)
            blank(src[i + 1:j])
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def _quoted_end(src, i, quote):
    j = i + 1
    while j < len(src):
        if src[j] == "\\":
            j += 2
        elif src[j] == quote or src[j] == "\n":
            return j + 1
        else:
            j += 1
    return len(src)


def count_args(code, open_paren):
    """Number of top-level arguments of the call whose '(' is at open_paren."""
    depth, args, empty = 0, 1, True
    for j in range(open_paren, len(code)):
        ch = code[j]
        if ch in "([{":
            depth += 1
            if depth == 1:
                continue
        elif ch in ")]}":
            depth -= 1
            if depth == 0:
                return 0 if empty else args
        elif ch == "," and depth == 1:
            args += 1
        if not ch.isspace():
            empty = False
    return args


def declares(code, name):
    """True when the solution declares its own function or variable called name."""
    return re.search(
        rf"\b(?!(?:return|co_return|co_yield|throw|case|else|new|delete)\b)\w+[\s&*>]+{name}\s*[(=;{{]",
        code) is not None


def lint(src):
    code = strip_code(src)
    hits = []

    def hit(pos, text):
        hits.append((code.count("\n", 0, pos) + 1, text))

    for rx, label in OLD_NAMES:
        own = label.isidentifier() and declares(code, label)
        for m in rx.finditer(code):
            if own and not code[:m.start()].rstrip().endswith("meta::"):
                continue
            hit(m.start(), label)

    for m in CTX_CALL.finditer(code):
        if count_args(code, m.end() - 1) < 2:
            hit(m.start(), f"{m.group(1)} without access context")

    for m in re.finditer(r"\^", code):
        i = m.start()
        if code[i - 1:i] == "^" or code[i + 1:i + 2] in ("^", "="):
            continue
        after = code[i + 1:].lstrip(" \t")
        if not re.match(r"[A-Za-z_]|::", after):
            continue
        before = code[:i].rstrip()
        if before:
            word = re.search(r"\w+$", before)
            if word:
                if word.group() not in EXPR_KEYWORDS:
                    continue
            elif before[-1] not in EXPR_PUNCT:
                continue
        name = re.match(r"(?:::)?[\w:]*", after).group()
        hit(i, f"single-caret ^{name}")
    return [f"line {line}: {text}" for line, text in sorted(hits)]


def first_error(text):
    for line in text.splitlines():
        if "error:" in line:
            return line.strip()
    return text.strip().splitlines()[-1] if text.strip() else "compile failed"


def same_output(actual, expected):
    return actual.rstrip("\n") == expected.rstrip("\n")


def run_cmd(cmd, cwd, timeout, container=None):
    """Return (returncode, stdout, stderr) or None on timeout."""
    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, timeout=timeout, stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired:
        if container:
            subprocess.run(["docker", "rm", "-f", container], capture_output=True)
        return None
    return p.returncode, p.stdout.decode(errors="replace"), p.stderr.decode(errors="replace")


def docker(work, args):
    name = f"grade-{uuid.uuid4().hex[:12]}"
    cmd = ["docker", "run", "--rm", "--name", name, "--user", f"{os.getuid()}:{os.getgid()}",
           "-v", f"{work}:/src", "-w", "/src", FORK_IMAGE, *args]
    return cmd, name


def build_and_run(work, compiler, binary, expected, use_docker=False):
    """Return a dict with ok, reason and (for the fork) deprecated warnings."""
    cmd = [*compiler, "test.cpp", "-o", binary]
    name = None
    if use_docker:
        cmd, name = docker(work, cmd)
    res = run_cmd(cmd, work, COMPILE_TIMEOUT, name)
    if res is None:
        return {"ok": False, "reason": "compile timeout", "warnings": []}
    code, _, err = res
    warnings = sorted({l.strip() for l in err.splitlines() if "warning:" in l and "deprecated" in l})
    if code != 0:
        return {"ok": False, "reason": first_error(err), "warnings": warnings}

    cmd = [f"./{binary}"]
    name = None
    if use_docker:
        cmd, name = docker(work, cmd)
    res = run_cmd(cmd, work, RUN_TIMEOUT, name)
    if res is None:
        return {"ok": False, "reason": "timeout", "warnings": warnings}
    code, out, _ = res
    if not same_output(out, expected):
        return {"ok": False, "reason": "wrong output", "warnings": warnings}
    if code != 0:
        return {"ok": False, "reason": f"exit code {code}", "warnings": warnings}
    return {"ok": True, "reason": "", "warnings": warnings}


def find_solution(sol_dir, tid):
    for p in (sol_dir / tid / "solution.hpp", sol_dir / f"{tid}.hpp"):
        if p.is_file():
            return p
    return None


def load_tasks(only):
    tasks = []
    for d in sorted(p for p in TASKS_DIR.iterdir() if p.is_dir()):
        missing = [f for f in ("test.cpp", "expected.txt", "task.toml") if not (d / f).is_file()]
        if missing:
            print(f"skipping {d.name}: missing {', '.join(missing)}", file=sys.stderr)
            continue
        with open(d / "task.toml", "rb") as f:
            meta = tomllib.load(f)
        tasks.append({"id": d.name, "dir": d, **meta})
    if only:
        known = {t["id"] for t in tasks}
        unknown = [t for t in only if t not in known]
        if unknown:
            sys.exit(f"unknown or incomplete tasks: {', '.join(unknown)}")
        tasks = [t for t in tasks if t["id"] in only]
    return tasks


def grade(task, sol_dir, use_fork):
    tid = task["id"]
    fork_ok = task.get("fork_ok", True)
    fallback = task.get("cxx20_fallback", False)
    r = {
        "id": tid,
        "holdout": task.get("holdout", False),
        "fork_ok": fork_ok,
        "present": False,
        "gcc": "missing",
        "fork": "n/a" if not fork_ok else ("skip" if not use_fork else "missing"),
        "cxx20": "missing" if fallback else "",
        "lint": [],
        "reason": "missing",
    }
    sol = find_solution(sol_dir, tid)
    if sol is None:
        return r
    r["present"] = True
    src = sol.read_text(errors="replace")
    r["lint"] = lint(src)
    expected = (task["dir"] / "expected.txt").read_text()

    work = tempfile.mkdtemp(prefix=f"grade-{tid}-")
    try:
        shutil.copy(task["dir"] / "test.cpp", work)
        Path(work, "solution.hpp").write_text(src)
        reasons = []

        g = build_and_run(work, GCC, "t_gcc", expected)
        if g["ok"] and fallback and "^^" not in strip_code(src):
            g = {"ok": False, "reason": "no reflection path"}
        r["gcc"] = "pass" if g["ok"] else "fail"
        if not g["ok"]:
            reasons.append(f"gcc: {g['reason']}")

        if fallback:
            c = build_and_run(work, GCC20, "t_cxx20", expected)
            r["cxx20"] = "pass" if c["ok"] else "fail"
            if not c["ok"]:
                reasons.append(f"c++20: {c['reason']}")

        if use_fork:
            f = build_and_run(work, FORK, "t_fork", expected, use_docker=True)
            r["lint"] += [f"fork {w}" for w in f["warnings"]]
            if fork_ok:
                r["fork"] = "pass" if f["ok"] else "fail"
                if not f["ok"]:
                    reasons.append(f"fork: {f['reason']}")
        r["reason"] = "; ".join(reasons)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return r


def passed_gcc(r):
    return r["gcc"] == "pass" and r["cxx20"] in ("", "pass")


def cell(text):
    text = text.replace("|", "\\|")
    return text if len(text) <= 160 else text[:157] + "..."


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("solutions_dir", type=Path)
    ap.add_argument("--json", type=Path, help="write per-task results and totals")
    ap.add_argument("--tasks", help="comma separated task ids")
    ap.add_argument("--no-fork", action="store_true", help="skip the clang-p2996 fork")
    args = ap.parse_args()

    if not args.solutions_dir.is_dir():
        sys.exit(f"not a directory: {args.solutions_dir}")
    only = [t for t in args.tasks.split(",") if t] if args.tasks else None
    tasks = load_tasks(only)
    use_fork = not args.no_fork

    with ThreadPoolExecutor(JOBS) as pool:
        results = list(pool.map(lambda t: grade(t, args.solutions_dir, use_fork), tasks))

    print("| task | gcc | fork | c++20 | lint | reason |")
    print("|---|---|---|---|---|---|")
    for r in results:
        print(f"| {r['id']} | {r['gcc']} | {r['fork']} | {r['cxx20']} | {len(r['lint'])} | {cell(r['reason'])} |")

    linted = [r for r in results if r["lint"]]
    if linted:
        print("\nlint hits:")
        for r in linted:
            for hit in r["lint"]:
                print(f"- {r['id']}: {hit}")

    present = [r for r in results if r["present"]]
    fork_tasks = [r for r in results if r["fork_ok"]]
    holdout = [r for r in results if r["holdout"]]
    totals = {
        "gcc": [sum(map(passed_gcc, results)), len(results)],
        "clean": [sum(not r["lint"] for r in present), len(present)],
        "holdout_gcc": [sum(map(passed_gcc, holdout)), len(holdout)],
    }
    if use_fork:
        totals["fork"] = [sum(r["fork"] == "pass" for r in fork_tasks), len(fork_tasks)]

    print()
    print(f"gcc {totals['gcc'][0]}/{totals['gcc'][1]}")
    if use_fork:
        print(f"fork {totals['fork'][0]}/{totals['fork'][1]}")
    print(f"clean {totals['clean'][0]}/{totals['clean'][1]}")
    print(f"holdout gcc {totals['holdout_gcc'][0]}/{totals['holdout_gcc'][1]}")

    if args.json:
        for r in results:
            r["gcc_overall"] = passed_gcc(r)
        args.json.write_text(json.dumps({"tasks": results, "totals": totals}, indent=2) + "\n")


if __name__ == "__main__":
    main()
