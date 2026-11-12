# Examples

Small programs that show what standard C++26 reflection can do. Each file is self-contained: the library part comes first, then the user code after `// ---- user code`, and the program's output is at the end of the file.

| Example | What it does | Like |
|---|---|---|
| [typed_regex.cpp](typed_regex.cpp) | The pattern is a template argument and each named group becomes a struct member: `(?<year:int>\d+)` gives `int year`, and a group that can be left out gives a `std::optional`. A bad pattern does not compile. | CTRE, Python `re` named groups |
| [checked_sql.cpp](checked_sql.cpp) | Query text is checked against a struct at compile time. The result row is a generated struct with just the selected columns, each `?` is a parameter with its column's type, and a misspelled column gets a did-you-mean error. | Rust sqlx `query!` |
| [units.cpp](units.cpp) | Unit text such as `"kg*m/s^2"` becomes a type, and named and scaled units are annotations on an enum. Adding meters to seconds does not compile, and km/h or kWh convert on the way in and out. | F# units of measure |
| [autodiff.cpp](autodiff.cpp) | A formula string becomes a function with named arguments. Its derivatives are computed at compile time by taking the formula's type apart and rebuilding it. | symbolic math in Mathematica, JAX `grad` |
| [spreadsheet.cpp](spreadsheet.cpp) | Struct members carry formulas, which are parsed, ordered by dependency and compiled to plain member reads. A cycle or a misspelled name is a compile error. | Excel |
| [csv_table.cpp](csv_table.cpp) | CSV text becomes a typed constant table. Member names come from the header row, member types from the data. | F# type providers |
| [protobuf.cpp](protobuf.cpp) | Plain structs with field numbers in annotations encode to and decode from the protobuf wire format, byte for byte what protoc writes, and the `.proto` file is written from the same structs. A field number used twice does not compile. | Protocol Buffers, Rust prost |
| [typescript_types.cpp](typescript_types.cpp) | TypeScript declarations generated from plain C++ structs and enums. Each type is declared once, after the types it uses, and types that refer to themselves or to each other work. | Rust ts-rs, typeshare |
| [peg_grammar.cpp](peg_grammar.cpp) | Plain AST structs, variants and token annotations give a PEG parser, the grammar text and a printer, so the three cannot drift apart. The parser and printer also run at compile time. | pest, parser generators |
| [key_paths.cpp](key_paths.cpp) | A path such as `"favorites[1].name"` is checked against the struct at compile time and becomes plain member accesses, with get, set and a functional update. A bad path fails with a message like `Person.favorites[1] has no member 'price'`. | Swift key paths |
| [utility_types.cpp](utility_types.cpp) | Partial, Pick and Omit versions of a plain struct, as real structs with the original member names. Values move between them by member name, and a wrong name in `get<"name">` or `pick` does not compile. | TypeScript utility types |
| [kwargs.cpp](kwargs.cpp) | Ordinary functions called with named arguments, `{.name = "ann", .times = 3}`, through a struct generated from the function's own parameter names. A missing, misspelled or wrongly typed argument does not compile. | Python keyword arguments |
| [go_interfaces.cpp](go_interfaces.cpp) | A type satisfies an interface by having the methods. `dyn<Shape>` erases the type and keeps `s->area()` calls. | Go interfaces, Rust `dyn Trait` |
| [mocks.cpp](mocks.cpp) | Recording mocks and logging proxies made from a plain interface struct, with no `MOCK_METHOD` macros. Logged calls show the interface's parameter names, and calls with the wrong argument types do not compile. | gMock, Mockito |
| [dependency_injection.cpp](dependency_injection.cpp) | Builds any class from its constructor parameters, making each object once and after the objects it needs, with nothing registered. A dependency cycle does not compile. | Dagger, Spring |
| [effects.cpp](effects.cpp) | A program is a coroutine that asks for things by `co_await`-ing plain structs, and the caller decides what they mean by passing handler objects. A handler method is found by the effect type it takes, and a missing handler is a compile error. | Koka, OCaml 5 effects |
| [multimethods.cpp](multimethods.cpp) | Picks the most specific overload by the run-time types of all arguments. Nothing is registered, and ambiguous pairs are reported. | Julia, CLOS |
| [rust_enum.cpp](rust_enum.cpp) | Enums with data, named constructors and a `match` that must handle every alternative. | Rust enums |
| [quickcheck.cpp](quickcheck.cpp) | Property tests for plain structs. Random values come from the member types and their annotations, and a failing value is shrunk to a small counterexample. | Haskell QuickCheck, Python Hypothesis |
| [contracts.cpp](contracts.cpp) | Class invariants written as text in annotations are parsed at compile time. `checked<T>` checks every change and undoes the ones that break an invariant, and a typo in an invariant does not compile. | Eiffel and D class invariants |
| [guarded_by.cpp](guarded_by.cpp) | Members name the mutex that guards them, and `lock::locked(obj)` takes those locks and returns a view that reaches only the guarded members. Touching guarded data without its lock does not compile. | Rust `Mutex<T>`, Abseil `ABSL_GUARDED_BY` |
| [arch_lint.cpp](arch_lint.cpp) | One call walks a whole namespace tree at compile time and reports bad names, a half-written rule of five, too much padding, and lower layers that use classes from higher ones. A `static_assert` turns the rules into a build error. | Java ArchUnit |
| [registers.cpp](registers.cpp) | A layout string such as `"EN:1 PARITY:2 _:1 BAUD:12"` becomes a real bit-field struct, and annotated functions become the interrupt vector table. Overlapping fields, two handlers on one interrupt and a pin used twice do not compile, and neither does a platform whose bit-fields would not land where the string says. | Rust svd2rust, `#[interrupt]` |
| [type_counter.cpp](type_counter.cpp) | Counters and type lists that grow as the compiler reads the file, so plugins add themselves right after their definition and `main` loops over all of them. Plain standard C++, with no friend injection tricks. | C `__COUNTER__`, Rust `inventory` |
| [meta_pipe.cpp](meta_pipe.cpp) | Type changes written like shell pipes, `"remove_reference \| add_const \| add_pointer"`. The words are not written by hand: they are the functions of `std::meta`, found by reflecting on `std::meta` itself. | nothing: reflection used on its own API |
| [pytest_fixtures.cpp](pytest_fixtures.cpp) | Tests get their fixtures by parameter name, set up in dependency order and torn down after. | pytest |
| [decorators.cpp](decorators.cpp) | Decorators written as annotations on plain functions: log, cache, `retry{3}`. Anyone can write a new one. | Python decorators |
| [swizzle.cpp](swizzle.cpp) | `vec<float, "xyz">` has real members `x`, `y`, `z`, an index operator and swizzles like `v.sw<"zyx">()`. | GLSL |
| [small_features.cpp](small_features.cpp) | Enum case lists and parsing, flag sets, range checks, struct tags, `repr` and hashing, each in a few lines. | Swift, Java, Python, Ada, Go, Rust |

Build one with a compiler that supports C++26 reflection, for example:

```sh
g++ -std=c++26 -freflection autodiff.cpp -o autodiff && ./autodiff
```

`python3 tests/check_snippets.py` builds every example and checks that it prints the output shown in the file.
