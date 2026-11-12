# cpp26-reflection-skill

An agent skill that teaches coding agents to write C++26 static reflection code that compiles and does what was asked.

C++26 adds reflection: `^^`, `[: :]` splices, `template for`, annotations and the `<meta>` library. Agents know the syntax, but the rules are new and strict, and their mistakes are subtle: a splice used as a template argument without `typename`, a reflection reaching code that runs at run time, `define_aggregate` called outside a `consteval` block, or an annotation check that compiles and never matches. This skill gives them the final C++26 rules, the full `<meta>` API, tested recipes, and the traps that compile but misbehave.

```cpp
// compiles, but never finds the annotation: the type of an annotation is const
for (auto a : std::meta::annotations_of(m))
    if (std::meta::type_of(a) == ^^width) ...

// what works
for (auto a : std::meta::annotations_of(m))
    if (std::meta::remove_const(std::meta::type_of(a)) == ^^width) ...
```

## Results

There are 32 tasks, listed in [evals/README.md](evals/README.md), each tried once per model. The last 9 are harder: generated types, constructor and parameter reflection, type substitution and format strings checked at compile time. Models wrote each header in one pass and were not allowed to compile anything. A task passes when the header builds with GCC 16 and the test program prints the expected output.

| Model | Without skill | With skill |
|---|---|---|
| Claude Opus 5.5 | 31/32 | 32/32 |
| Claude Fable 5.2 | 32/32 | 31/32 |
| GPT-6.1 Sol | 20/32 | 32/32 |
| GPT-6 Astra | 27/32 | 32/32 |
| **Total** | **110/128 (86%)** | **127/128 (99%)** |

On the 16 holdout tasks, which have no matching example in the skill, the totals are 52/64 without and 63/64 with.

Claude Opus 5.5 and Fable 5.2 already handle these tasks well; the skill helps most with models that do not. On the 9 harder tasks, GPT-6.1 Sol went from 3/9 to 9/9.

Common failures without the skill:

- annotation values that are not structural types, such as `std::string_view` or a pointer to a string literal
- a splice used as a template argument without `typename`
- reflection values reaching code that runs at run time
- `define_aggregate` called outside a `consteval` block
- comparing an annotation's type without removing `const`, which compiles and prints the wrong result

Each model made one attempt per task, so a difference of one task is within noise.

## Examples

[examples/](examples/) has ten programs that show how far standard reflection goes. A formula string is differentiated at compile time, CSV text becomes a typed table, and struct members carry spreadsheet formulas. Go interfaces, Rust enums, Julia multimethods, pytest fixtures and Python decorators each fit in one file of under 200 lines, user code included.

## What it covers

- the core member and enumerator loop, and why each part is needed
- the compile-time boundary: where `std::meta::info` may live, splicing, `consteval`, `if constexpr` guards
- splice grammar: `typename`, `template`, parentheses
- access contexts, including the helper-function trap
- queries and their traps: special members in `members_of`, `has_identifier`, aliases, `display_string_of`
- values, annotations and `define_aggregate`
- feature-test macros and fallbacks for compilers without reflection
- migration from X-macros, magic_enum, Boost.PFR, Boost.Describe and registration patterns
- the full `<meta>` synopsis and a table of old spellings

The skill teaches standard C++26 only. It does not depend on a particular compiler. Every code block in it is compiled and run by `tests/check_snippets.py`.

## Install

Claude Code:

```
/plugin marketplace add arhwx/cpp26-reflection-skill
/plugin install cpp26-reflection@cpp26-reflection-skill
```

Codex:

```
codex plugin marketplace add arhwx/cpp26-reflection-skill
codex plugin add cpp26-reflection@cpp26-reflection-skill
```

Cursor and the other agents supported by the [skills](https://skills.sh) CLI:

```
npx skills add arhwx/cpp26-reflection-skill
```

Manual: copy `skills/cpp26-reflection` into your agent's skills directory, for example `~/.claude/skills/` or `.agents/skills/`.

The agent loads the skill when you work on reflection code. You can also ask for it by name.

## Layout

```
skills/cpp26-reflection/
  SKILL.md                rules and traps, loaded when the skill triggers
  references/recipes.md   complete programs for common tasks
  references/api.md       the <meta> synopsis with notes
  references/renames.md   old spellings and their C++26 names
  references/migration.md replacing older techniques, fallbacks
evals/                    tasks, reference solutions and the grader
examples/                 programs that show what reflection can do
tests/check_snippets.py   compiles every code block in the skill and every example
```

## Evals

See [evals/README.md](evals/README.md). Each task asks an agent for a header that a fixed test program includes. The grader compiles and runs it and checks the output and the spellings used.

## License

MIT
