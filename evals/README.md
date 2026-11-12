# Evals

These evals check whether an agent writes correct standard C++26 static reflection code.
Each task asks for one header. The agent writes it in a single pass, without compiling or running anything.
We then compile it against a fixed test driver, run it on two toolchains, compare the output, and check the code for outdated spellings.

## Tasks

| task | what it tests | holdout |
|---|---|---|
| `bit_packed` | Generated bit-field struct from width annotations, with masking pack and unpack | yes |
| `call_by_name` | Call a member function chosen by name | yes |
| `cli_parse` | Command line parser from an options struct | no |
| `compare_hash` | Memberwise equality and hash | no |
| `di_container` | Dependency injector that builds objects from their constructor parameters | yes |
| `enum_names` | Enum to string and back | no |
| `enum_values` | Enumerator count, values and index with duplicate values | no |
| `field_names` | Constexpr array of field names | no |
| `flag_names` | Bit-flag enum to string, replacing X-macros | no |
| `format_fields` | Format string whose field placeholders are checked at compile time in the function's constraints | yes |
| `json_annotations` | JSON writer with rename and skip annotations | no |
| `member_pointers` | Constexpr member pointer and name tables for members of a given type | yes |
| `named_call` | Call a free function with string arguments given by parameter name, using default arguments | yes |
| `ns_dispatch` | Call a namespace function by name | yes |
| `offsets` | Constexpr member offset table | yes |
| `optional_patch` | Generated patch struct of optional members | no |
| `param_names` | Function signature with parameter names | no |
| `portable_enum` | Enum names with a C++20 fallback | no |
| `private_fields` | Field count with private members, public field names | yes |
| `qualified_name` | Fully qualified name of a namespace-scope entity | yes |
| `range_check` | Range validation from annotations | yes |
| `replace_type` | Replace a type inside template arguments, pointers and const | yes |
| `route_table` | Call member functions by route annotation | yes |
| `soa_columns` | Generated struct of arrays | no |
| `sorted_layout` | Generated struct with members sorted by alignment, ties in declaration order | no |
| `struct_diff` | Recursive diff of nested structs and C arrays with member paths | no |
| `struct_print` | Recursive struct printer | no |
| `to_tuple` | Struct to tuple | yes |
| `type_filter` | Filter tuple element types | no |
| `type_names` | Compiler-independent type names, including template specializations | yes |
| `variant_of_members` | Variant of distinct member types and member lookup by name | yes |
| `with_bases` | Struct printer including base classes | no |

Each task directory has:

- `prompt.md`: the request given to the agent, including the full driver and expected output
- `test.cpp`: the driver, which includes `solution.hpp`
- `expected.txt`: the exact expected stdout
- `task.toml`: title, needed features, fork support, holdout flag

`reference/<id>.hpp` is a reference solution for each task.

Holdout tasks have no matching example in the skill. They show whether the skill generalizes beyond the cases it covers.

## Toolchains

- GCC 16: `g++ -std=c++26 -freflection`. This is the primary grader.
  GCC 16's libstdc++ rejects `std::array<std::meta::info, 0>`: the empty helper's `operator[]` lacks `constexpr`, which is ill-formed for a consteval-only type.
  The grader compiles against a copy of `<array>` with that one `constexpr` added, so the library bug does not fail correct solutions.
- The clang-p2996 fork, run in Docker: `clang++ -std=c++26 -freflection-latest -stdlib=libc++`.
  The image is pinned in `grade.py`; set `FORK_IMAGE` to use another one.
  It lacks some standard features, so tasks that need them set `fork_ok = false` and give the reason in `fork_note`.

## Running

Generate solutions with Codex, with or without the skill:

```sh
python3 evals/run_codex.py --out /tmp/runs/skill --skill skills/cpp26-reflection
python3 evals/run_codex.py --out /tmp/runs/base
```

Options: `--model M`, `--tasks a,b`, `--jobs N` (default 4).
Each task gets its own workdir with only `prompt.md`; with `--skill`, the skill is copied to `.agents/skills/` there.
The runs directory must be outside any git repo, so the agent cannot see this repo or the reference solutions.
Codex output goes to `<workdir>/codex.log`, and wall time and exit code per task go to `runs.json`.

Grade a directory holding `<id>/solution.hpp` or `<id>.hpp`:

```sh
python3 evals/grade.py /tmp/runs/skill --json skill.json
python3 evals/grade.py evals/reference
```

Options: `--tasks a,b`, `--no-fork` (GCC only), `--json out.json` (per-task results and totals).

## Scoring

- A task passes on a toolchain when the driver compiles (120 s limit), runs (10 s limit), and prints exactly `expected.txt`.
  Only trailing newlines are ignored.
- Tasks with `cxx20_fallback = true` (`portable_enum`) must also pass with `g++ -std=c++20`, without reflection, to count as a GCC pass.
  The solution must also contain `^^` outside comments and strings, so a pure C++20 solution fails with `no reflection path`.
- Fork results count only for tasks with `fork_ok = true`; the rest show `n/a`.
- Lint flags pre-standard spellings in the solution, ignoring comments and string literals:
  single-caret reflection (`^int`), old names such as `name_of`, `reflect_value`, `define_class` or `meta::expand`,
  and `members_of`, `nonstatic_data_members_of`, `static_data_members_of`, `bases_of` or `subobjects_of` called without an access context.
  An old name that the solution declares itself, such as its own `name_of` helper, counts only when written as `meta::name_of`.
  Every `deprecated` warning from the fork build also counts as a hit.
- Totals: `gcc N/M` over all tasks, `fork N/M` over fork tasks, `clean N/M` over solutions present with no lint hits, and `holdout gcc N/M`.
