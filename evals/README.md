# Evals

These evals check whether an agent writes correct standard C++26 static reflection code.
Each task asks for one header. The agent writes it in a single pass, without compiling or running anything.
We then compile it against a fixed test driver, run it on two toolchains, compare the output, and check the code for outdated spellings.

## Tasks

| task | what it tests | holdout |
|---|---|---|
| `actor` | Message struct per void handler, variant mailbox, queued in-order delivery | yes |
| `alter_table` | `ALTER TABLE` statements from two struct versions | yes |
| `api_hash` | Namespace API snapshot with its own type printer, hashed for a guard | yes |
| `arbitrary` | Seeded random struct values with exact splitmix64 and shrink order | yes |
| `ast_grammar` | PEG parser, EBNF text and printer built from AST structs, variants and literal annotations | yes |
| `autodiff` | Formula string to a typed expression, generated argument struct, symbolic derivatives | yes |
| `await_struct` | Await a struct of awaitables, get a generated same-named struct of results | yes |
| `bit_packed` | Generated bit-field struct from width annotations, with masking pack and unpack | yes |
| `c_vtable` | Generated C function-pointer table, thunks and C header text from a C++ class | yes |
| `call_by_name` | Call a member function chosen by name | yes |
| `cli_parse` | Command line parser from an options struct | no |
| `compare_hash` | Memberwise equality and hash | no |
| `console` | Namespace variables become settings and functions become commands parsed from text | yes |
| `convert_version` | Convert one struct version to another by member name, with renames, defaults and nesting | yes |
| `csv_table` | CSV text becomes a typed constant row struct and array, with column types inferred | yes |
| `decorators` | Chains of decorator annotations on functions: order, arguments, rejecting bad calls | yes |
| `derive` | Rust-style derive annotation on a class, checked through members, with explanations | yes |
| `di_container` | Dependency injector that builds objects from their constructor parameters | yes |
| `effects` | Algebraic effects; handler methods found by parameter type; coverage messages | yes |
| `enum_names` | Enum to string and back | no |
| `enum_values` | Enumerator count, values and index with duplicate values | no |
| `env_config` | Environment variable names built from member paths, with parsing per type and annotations | yes |
| `exhaustive_check` | Every value of nested structs in a fixed order, counterexample search, constexpr count | yes |
| `field_names` | Constexpr array of field names | no |
| `fixtures` | pytest-style fixtures passed in by parameter name, built and torn down once per test | yes |
| `flag_names` | Bit-flag enum to string, replacing X-macros | no |
| `fmt_args` | Format string generates the typed argument struct, with checks on invalid formats | yes |
| `format_fields` | Format string whose field placeholders are checked at compile time in the function's constraints | yes |
| `fsm_check` | State machine from enumerator annotations: checks, DOT text, run by event name | yes |
| `guarded_by` | Annotations holding a mutex reflection; locked view exposes only guarded members | yes |
| `hkd` | One `map<T, F>` across optional, vector, reference_wrapper, unique_ptr and alias templates | yes |
| `ini_config` | Nested config struct generated from sample INI text; checked reader, writer | yes |
| `inspector` | Struct drawn as a text form; range, label, read-only annotations; edit by path | yes |
| `interfaces` | Go-style interfaces: a concept, a `dyn<I>` handle with `s->method()` calls, missing-method report | yes |
| `invariants` | Class invariant annotations parsed at compile time, enforced with rollback | yes |
| `json_annotations` | JSON writer with rename and skip annotations | no |
| `json_serde` | JSON writer with skip, rename, camel naming, versions and friend access | yes |
| `key_path` | Dotted and indexed paths resolved at compile time, with get, set, over and errors | yes |
| `lang_extras` | Enum cases, name and parse, enum sets, annotation checks, tags, repr, hash | yes |
| `layout_report` | Pahole-style layout table with holes, bit-fields and bases; FNV-1a layout hash | yes |
| `lint_rules` | Namespace-wide lints: naming, rule of five, padding budget, layering | yes |
| `member_pointers` | Constexpr member pointer and name tables for members of a given type | yes |
| `meta_pipe` | Type pipelines looked up among `std::meta` functions, plus shortest-pipeline search | yes |
| `mock_proxy` | Recording mocks and logging proxies generated from an interface struct of declarations | yes |
| `multi_dispatch` | Picks an overload by every argument's dynamic type, caches the choice, reports ambiguous pairs | yes |
| `named_args` | Argument struct generated from parameter names; required, defaulted and reference parameters | yes |
| `named_call` | Call a free function with string arguments given by parameter name, using default arguments | yes |
| `ns_dispatch` | Call a namespace function by name | yes |
| `offsets` | Constexpr member offset table | yes |
| `on_handlers` | `on_<enumerator>` handlers found by name; missing and unused handlers reported | yes |
| `optional_patch` | Generated patch struct of optional members | no |
| `param_names` | Function signature with parameter names | no |
| `partial_types` | Partial, Pick and Omit structs, spread by member name, checked `get` | yes |
| `perfect_hash` | Compile-time perfect hash from names to enumerators and data members | yes |
| `portable_enum` | Enum names with a C++20 fallback | no |
| `private_fields` | Field count with private members, public field names | yes |
| `proto_wire` | Protobuf wire encode and decode, plus `.proto` text, from field-number annotations | yes |
| `qualified_name` | Fully qualified name of a namespace-scope entity | yes |
| `range_check` | Range validation from annotations | yes |
| `redact` | `[[=secret]]` members print as `***`; serializing a type with secrets needs a policy | yes |
| `regex_captures` | Regex named groups become typed struct members, compile-time pattern errors | yes |
| `registers` | Bit-field structs from layout text, IRQ vector table, pin conflict check | yes |
| `replace_type` | Replace a type inside template arguments, pointers and const | yes |
| `replicate` | Generated snapshot struct of annotated members, plus delta encode and apply | yes |
| `route_table` | Call member functions by route annotation | yes |
| `schema_dsl` | Schema string becomes nested structs, vectors and optionals; value printer; parse errors | yes |
| `soa_columns` | Generated struct of arrays | no |
| `sorted_layout` | Generated struct with members sorted by alignment, ties in declaration order | no |
| `spreadsheet` | Formula annotations parsed at compile time, run in dependency order, errors reported as text | yes |
| `sql_ddl` | Compile-time CREATE TABLE text, foreign keys holding member reflections, dependency order | yes |
| `sql_query` | Query text gives row type, typed `?` parameters and did-you-mean column errors | yes |
| `stream_decode` | Resumable binary decoder and encoder walking struct members in coroutines | yes |
| `struct_diff` | Recursive diff of nested structs and C arrays with member paths | no |
| `struct_print` | Recursive struct printer | no |
| `sum_type` | Rust-style enum with named constructors and a match that needs every arm | yes |
| `swizzle` | GLSL vector members generated from a string; read and write swizzles | yes |
| `to_tuple` | Struct to tuple | yes |
| `tracked` | Generated dirty-flag struct, setters by member name, observers, commit and revert | yes |
| `ts_emitter` | TypeScript declarations from a type-graph walk, dependencies first, cycles handled | yes |
| `type_counter` | Compile-time counters and type lists, held by completing classes | yes |
| `type_dump` | Zig-style type tree with portable names, cycle-safe | yes |
| `type_filter` | Filter tuple element types | no |
| `type_names` | Compiler-independent type names, including template specializations | yes |
| `undo_patch` | Generated nested change patches; diff, apply, invert, merge for undo and redo | yes |
| `units` | Unit text such as `kg*m/s^2` turned into dimension types; scaled units from annotations | yes |
| `variant_of_members` | Variant of distinct member types and member lookup by name | yes |
| `vertex_layout` | Vertex attribute table: offset, location, component count and type | yes |
| `visitor` | Sealed hierarchy found by namespace scan; visitor struct that must cover every class | yes |
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
