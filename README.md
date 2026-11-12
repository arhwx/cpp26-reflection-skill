# cpp26-reflection-skill

An agent skill that teaches coding agents to write C++26 static reflection code that compiles.

C++26 adds reflection: `^^`, `[: :]` splices, `template for`, annotations and the `<meta>` library. Coding agents learned it from years of proposal papers, blog posts and experimental compilers, so they often write spellings that never made it into the standard (`^T`, `name_of`, `std::meta::expand`, member queries without an access context) or code that breaks the compile-time rules. This skill gives them the final C++26 rules, the full `<meta>` API, tested recipes and a translation table for old spellings.

```cpp
// what agents tend to write
for (auto m : std::meta::nonstatic_data_members_of(^T))
    std::println("{}={}", std::meta::name_of(m), obj.[:m:]);

// what compiles
template for (constexpr auto m : std::define_static_array(
                  std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())))
    std::println("{}={}", std::meta::identifier_of(m), obj.[:m:]);
```

## Results

There are 23 tasks, each tried once per model. Models wrote each header in one pass and were not allowed to compile anything. A task passes when the header builds with GCC 16 and the test program prints the expected output.

| Model | Without skill | With skill |
|---|---|---|
| Claude Opus 5.5 | 22/23 | 23/23 |
| Claude Fable 5.2 | 23/23 | 23/23 |
| GPT-6.1 Sol | 17/23 | 23/23 |
| GPT-6 Astra | 18/23 | 23/23 |
| **Total** | **80/92 (87%)** | **92/92 (100%)** |

On the 9 holdout tasks, which have no matching example in the skill, the totals are 29/36 without and 36/36 with.

Common failures without the skill:

- annotation values that are not structural types, such as `std::string_view` or a pointer to a string literal
- a splice used as a template argument without `typename`
- reflection values reaching code that runs at run time
- `define_aggregate` called outside a `consteval` block
- comparing an annotation's type without removing `const`, which compiles and prints the wrong result

Each model made one attempt per task, so a difference of one task is within noise.

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
tests/check_snippets.py   compiles every code block in the skill
```

## Evals

See [evals/README.md](evals/README.md). Each task asks an agent for a header that a fixed test program includes. The grader compiles and runs it and checks the output and the spellings used.

## License

MIT
