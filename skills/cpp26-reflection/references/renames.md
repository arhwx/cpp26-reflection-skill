# Old spellings and their C++26 equivalents

Reflection went through many revisions before C++26. Blog posts, talks, papers and experimental compilers still show older spellings. Translate them with this table. Names in the left column are not standard C++26.

## Syntax

| Old | C++26 |
|---|---|
| `^T`, `^expr` (single caret) | `^^T`, `^^expr` |
| `reflexpr(T)` (Reflection TS, type-based) | `^^T` (value-based `std::meta::info`) |
| `[: r :]` used as a type without `typename` | `typename [: r :]` outside type-only contexts |
| `[: r :]<args>` | `template [: r :]<args>` |
| `std::meta::expand(range) >> [&]<auto m> { ... };` | `template for (constexpr auto m : std::define_static_array(range)) { ... }` |

## Names

| Old | C++26 |
|---|---|
| `name_of`, `qualified_name_of` | `identifier_of` (build qualified names by walking `parent_of`) |
| `display_name_of` | `display_string_of` |
| `members_of(r)`, `nonstatic_data_members_of(r)`, `bases_of(r)` (one argument) | add a context: `members_of(r, std::meta::access_context::current())` |
| `accessible_members_of`, `get_public_members`, `members_of(r, is_public)` | `members_of(r, std::meta::access_context::unprivileged())`, or a context of your choice |
| `test_trait(^^std::is_class_v, t)`, `test_type`, `test_types` | `is_class_type(t)` and the other `is_*_type` functions |
| `type_is_const`, `type_remove_cvref`, `type_*` | `is_const_type`, `remove_cvref`, ... |
| `is_alias` | `is_type_alias`, `is_namespace_alias` |
| `is_incomplete_type` | `!is_complete_type(r)` |
| `has_complete_definition` | `is_enumerable_type` |
| `is_special_member` | `is_special_member_function` |
| `has_ellipsis_parameter` | `is_vararg_function` |
| `reflect_value`, `reflect_result` | `reflect_constant` (or `reflect_object`, `reflect_function`) |
| `value_of` | `constant_of` for a reflection, `extract<T>` for a C++ value |
| `reflect_invoke` | removed. Splice the function and call it: `[: f :](args...)` |
| `define_class` | `define_aggregate` (inside a `consteval { }` block) |
| `data_member_options_t` | `data_member_options` |
| `.width` in member options | `.bit_width` |
| `member_offsets` | `member_offset` |
| `operator_symbol_of` | `symbol_of` |
| `std::meta::define_static_array`, `std::meta::define_static_string` | `std::define_static_array`, `std::define_static_string` (namespace `std`) |
| `annotation_of_type`, `annotate` | `annotations_of_with_type`, or filter `annotations_of` |
| `is_trivial_type`, `is_consteval_only_type` | removed |
| `define_enum`, `enumerator_spec` | not in C++26 |
| `attributes_of`, `has_attribute` | not in C++26 |
| `is_entity_proxy`, `proxied_entity_of`, `underlying_entity_of` | not in C++26 |
