# Examples

Small programs that show what standard C++26 reflection can do. Each file is self-contained: the library part comes first, then the user code after `// ---- user code`, and the program's output is at the end of the file.

| Example | What it does | Like |
|---|---|---|
| [autodiff.cpp](autodiff.cpp) | A formula string becomes a function with named arguments. Its derivatives are computed at compile time by taking the formula's type apart and rebuilding it. | symbolic math in Mathematica, JAX `grad` |
| [spreadsheet.cpp](spreadsheet.cpp) | Struct members carry formulas, which are parsed, ordered by dependency and compiled to plain member reads. A cycle or a misspelled name is a compile error. | Excel |
| [csv_table.cpp](csv_table.cpp) | CSV text becomes a typed constant table. Member names come from the header row, member types from the data. | F# type providers |
| [go_interfaces.cpp](go_interfaces.cpp) | A type satisfies an interface by having the methods. `dyn<Shape>` erases the type and keeps `s->area()` calls. | Go interfaces, Rust `dyn Trait` |
| [multimethods.cpp](multimethods.cpp) | Picks the most specific overload by the run-time types of all arguments. Nothing is registered, and ambiguous pairs are reported. | Julia, CLOS |
| [rust_enum.cpp](rust_enum.cpp) | Enums with data, named constructors and a `match` that must handle every alternative. | Rust enums |
| [pytest_fixtures.cpp](pytest_fixtures.cpp) | Tests get their fixtures by parameter name, set up in dependency order and torn down after. | pytest |
| [decorators.cpp](decorators.cpp) | Decorators written as annotations on plain functions: log, cache, `retry{3}`. Anyone can write a new one. | Python decorators |
| [swizzle.cpp](swizzle.cpp) | `vec<float, "xyz">` has real members `x`, `y`, `z`, an index operator and swizzles like `v.sw<"zyx">()`. | GLSL |
| [small_features.cpp](small_features.cpp) | Enum case lists and parsing, flag sets, range checks, struct tags, `repr` and hashing, each in a few lines. | Swift, Java, Python, Ada, Go, Rust |

Build one with a compiler that supports C++26 reflection, for example:

```sh
g++ -std=c++26 -freflection autodiff.cpp -o autodiff && ./autodiff
```

`python3 tests/check_snippets.py` builds every example and checks that it prints the output shown in the file.
