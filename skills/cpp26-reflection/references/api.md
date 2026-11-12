# `<meta>` API

The complete `<meta>` synopsis of C++26, followed by notes on the functions that are easy to misuse. Any name not listed here is not standard.

General rules:

- Every function is `consteval`. Call it only during constant evaluation: in a `consteval` function, a `constexpr` variable initializer, a template argument, a `template for` header, `if constexpr`, `static_assert`, or a `consteval { }` block.
- Functions report errors by throwing `std::meta::exception`. Uncaught, the program is ill-formed.
- Functions returning `string_view` return null-terminated text in static storage. The view can be kept in a `constexpr` variable and used at run time.
- Calls are found by argument-dependent lookup on `info`, but write `std::meta::` in headers anyway.
- `<meta>` does not provide `<print>`, `<string>` or `<ranges>`. Include what you use.

## Synopsis

```text
#include <compare>              // see [compare.syn]
#include <initializer_list>     // see [initializer.list.syn]

namespace std {
  // [meta.string.literal], checking string literals
  consteval bool is_string_literal(const char* p);
  consteval bool is_string_literal(const wchar_t* p);
  consteval bool is_string_literal(const char8_t* p);
  consteval bool is_string_literal(const char16_t* p);
  consteval bool is_string_literal(const char32_t* p);

  // [meta.define.static], promoting to static storage
  namespace meta {
    template<ranges::input_range R>
      consteval info reflect_constant_string(R&& r);
    template<ranges::input_range R>
      consteval info reflect_constant_array(R&& r);
  }
  template<ranges::input_range R>
    consteval const ranges::range_value_t<R>* define_static_string(R&& r);
  template<ranges::input_range R>
    consteval span<const ranges::range_value_t<R>, see below> define_static_array(R&& r);
  template<class T>
    consteval const remove_cvref_t<T>* define_static_object(T&& r);
}

namespace std::meta {
  using info = decltype(^^::);

  // [meta.reflection.exception], class exception
  class exception;

  // [meta.reflection.operators], operator representations
  enum class operators {
    see below;
  };
  using enum operators;
  consteval operators operator_of(info r);
  consteval string_view symbol_of(operators op);
  consteval u8string_view u8symbol_of(operators op);

  // [meta.reflection.names], reflection names and locations
  consteval bool has_identifier(info r);

  consteval string_view identifier_of(info r);
  consteval u8string_view u8identifier_of(info r);

  consteval string_view display_string_of(info r);
  consteval u8string_view u8display_string_of(info r);

  consteval source_location source_location_of(info r);

  // [meta.reflection.queries], reflection queries
  consteval info type_of(info r);
  consteval info object_of(info r);
  consteval info constant_of(info r);

  consteval bool is_public(info r);
  consteval bool is_protected(info r);
  consteval bool is_private(info r);

  consteval bool is_virtual(info r);
  consteval bool is_pure_virtual(info r);
  consteval bool is_override(info r);
  consteval bool is_final(info r);

  consteval bool is_deleted(info r);
  consteval bool is_defaulted(info r);
  consteval bool is_user_provided(info r);
  consteval bool is_user_declared(info r);
  consteval bool is_explicit(info r);
  consteval bool is_noexcept(info r);

  consteval bool is_bit_field(info r);
  consteval bool is_enumerator(info r);
  consteval bool is_annotation(info r);

  consteval bool is_const(info r);
  consteval bool is_volatile(info r);
  consteval bool is_mutable_member(info r);
  consteval bool is_lvalue_reference_qualified(info r);
  consteval bool is_rvalue_reference_qualified(info r);

  consteval bool has_static_storage_duration(info r);
  consteval bool has_thread_storage_duration(info r);
  consteval bool has_automatic_storage_duration(info r);

  consteval bool has_internal_linkage(info r);
  consteval bool has_module_linkage(info r);
  consteval bool has_external_linkage(info r);
  consteval bool has_c_language_linkage(info r);
  consteval bool has_linkage(info r);

  consteval bool is_complete_type(info r);
  consteval bool is_enumerable_type(info r);

  consteval bool is_variable(info r);
  consteval bool is_type(info r);
  consteval bool is_namespace(info r);
  consteval bool is_type_alias(info r);
  consteval bool is_namespace_alias(info r);

  consteval bool is_function(info r);
  consteval bool is_conversion_function(info r);
  consteval bool is_operator_function(info r);
  consteval bool is_literal_operator(info r);
  consteval bool is_special_member_function(info r);
  consteval bool is_constructor(info r);
  consteval bool is_default_constructor(info r);
  consteval bool is_copy_constructor(info r);
  consteval bool is_move_constructor(info r);
  consteval bool is_assignment(info r);
  consteval bool is_copy_assignment(info r);
  consteval bool is_move_assignment(info r);
  consteval bool is_destructor(info r);

  consteval bool is_function_parameter(info r);
  consteval bool is_explicit_object_parameter(info r);
  consteval bool has_default_argument(info r);
  consteval bool is_vararg_function(info r);

  consteval bool is_template(info r);
  consteval bool is_function_template(info r);
  consteval bool is_variable_template(info r);
  consteval bool is_class_template(info r);
  consteval bool is_alias_template(info r);
  consteval bool is_conversion_function_template(info r);
  consteval bool is_operator_function_template(info r);
  consteval bool is_literal_operator_template(info r);
  consteval bool is_constructor_template(info r);
  consteval bool is_concept(info r);

  consteval bool is_value(info r);
  consteval bool is_object(info r);

  consteval bool is_structured_binding(info r);

  consteval bool is_class_member(info r);
  consteval bool is_namespace_member(info r);
  consteval bool is_nonstatic_data_member(info r);
  consteval bool is_static_member(info r);
  consteval bool is_base(info r);

  consteval bool has_default_member_initializer(info r);

  consteval bool has_parent(info r);
  consteval info parent_of(info r);

  consteval info dealias(info r);

  consteval bool has_template_arguments(info r);
  consteval info template_of(info r);
  consteval vector<info> template_arguments_of(info r);
  consteval vector<info> parameters_of(info r);
  consteval info variable_of(info r);
  consteval info return_type_of(info r);

  // [meta.reflection.access.context], access control context
  struct access_context;

  // [meta.reflection.access.queries], member accessibility queries
  consteval bool is_accessible(info r, access_context ctx);
  consteval bool has_inaccessible_nonstatic_data_members(info r, access_context ctx);
  consteval bool has_inaccessible_bases(info r, access_context ctx);
  consteval bool has_inaccessible_subobjects(info r, access_context ctx);

  // [meta.reflection.scope], scope identification
  consteval info current_function();
  consteval info current_class();
  consteval info current_namespace();

  // [meta.reflection.member.queries], reflection member queries
  consteval vector<info> members_of(info r, access_context ctx);
  consteval vector<info> bases_of(info type, access_context ctx);
  consteval vector<info> static_data_members_of(info type, access_context ctx);
  consteval vector<info> nonstatic_data_members_of(info type, access_context ctx);
  consteval vector<info> subobjects_of(info type, access_context ctx);
  consteval vector<info> enumerators_of(info type_enum);

  // [meta.reflection.layout], reflection layout queries
  struct member_offset;
  consteval member_offset offset_of(info r);
  consteval size_t size_of(info r);
  consteval size_t alignment_of(info r);
  consteval size_t bit_size_of(info r);

  // [meta.reflection.annotation], annotation reflection
  consteval vector<info> annotations_of(info item);
  consteval vector<info> annotations_of_with_type(info item, info type);

  // [meta.reflection.extract], value extraction
  template<class T>
    consteval T extract(info r);

  // [meta.reflection.substitute], reflection substitution
  template<class R>
    concept reflection_range = see below;

  template<reflection_range R = initializer_list<info>>
    consteval bool can_substitute(info templ, R&& arguments);
  template<reflection_range R = initializer_list<info>>
    consteval info substitute(info templ, R&& arguments);

  // [meta.reflection.result], expression result reflection
  template<class T>
    consteval info reflect_constant(T expr);
  template<class T>
    consteval info reflect_object(T& expr);
  template<class T>
    consteval info reflect_function(T& fn);

  // [meta.reflection.define.aggregate], class definition generation
  struct data_member_options;
  consteval info data_member_spec(info type, data_member_options options);
  consteval bool is_data_member_spec(info r);
  template<reflection_range R = initializer_list<info>>
    consteval info define_aggregate(info type_class, R&& mdescrs);

  // associated with [meta.unary.cat], primary type categories
  consteval bool is_void_type(info type);
  consteval bool is_null_pointer_type(info type);
  consteval bool is_integral_type(info type);
  consteval bool is_floating_point_type(info type);
  consteval bool is_array_type(info type);
  consteval bool is_pointer_type(info type);
  consteval bool is_lvalue_reference_type(info type);
  consteval bool is_rvalue_reference_type(info type);
  consteval bool is_member_object_pointer_type(info type);
  consteval bool is_member_function_pointer_type(info type);
  consteval bool is_enum_type(info type);
  consteval bool is_union_type(info type);
  consteval bool is_class_type(info type);
  consteval bool is_function_type(info type);
  consteval bool is_reflection_type(info type);

  // associated with [meta.unary.comp], composite type categories
  consteval bool is_reference_type(info type);
  consteval bool is_arithmetic_type(info type);
  consteval bool is_fundamental_type(info type);
  consteval bool is_object_type(info type);
  consteval bool is_scalar_type(info type);
  consteval bool is_compound_type(info type);
  consteval bool is_member_pointer_type(info type);

  // associated with [meta.unary.prop], type properties
  consteval bool is_const_type(info type);
  consteval bool is_volatile_type(info type);
  consteval bool is_trivially_copyable_type(info type);
  consteval bool is_standard_layout_type(info type);
  consteval bool is_empty_type(info type);
  consteval bool is_polymorphic_type(info type);
  consteval bool is_abstract_type(info type);
  consteval bool is_final_type(info type);
  consteval bool is_aggregate_type(info type);
  consteval bool is_structural_type(info type);
  consteval bool is_signed_type(info type);
  consteval bool is_unsigned_type(info type);
  consteval bool is_bounded_array_type(info type);
  consteval bool is_unbounded_array_type(info type);
  consteval bool is_scoped_enum_type(info type);

  template<reflection_range R = initializer_list<info>>
    consteval bool is_constructible_type(info type, R&& type_args);
  consteval bool is_default_constructible_type(info type);
  consteval bool is_copy_constructible_type(info type);
  consteval bool is_move_constructible_type(info type);

  consteval bool is_assignable_type(info type_dst, info type_src);
  consteval bool is_copy_assignable_type(info type);
  consteval bool is_move_assignable_type(info type);

  consteval bool is_swappable_with_type(info type1, info type2);
  consteval bool is_swappable_type(info type);

  consteval bool is_destructible_type(info type);

  template<reflection_range R = initializer_list<info>>
    consteval bool is_trivially_constructible_type(info type, R&& type_args);
  consteval bool is_trivially_default_constructible_type(info type);
  consteval bool is_trivially_copy_constructible_type(info type);
  consteval bool is_trivially_move_constructible_type(info type);

  consteval bool is_trivially_assignable_type(info type_dst, info type_src);
  consteval bool is_trivially_copy_assignable_type(info type);
  consteval bool is_trivially_move_assignable_type(info type);
  consteval bool is_trivially_destructible_type(info type);

  template<reflection_range R = initializer_list<info>>
    consteval bool is_nothrow_constructible_type(info type, R&& type_args);
  consteval bool is_nothrow_default_constructible_type(info type);
  consteval bool is_nothrow_copy_constructible_type(info type);
  consteval bool is_nothrow_move_constructible_type(info type);

  consteval bool is_nothrow_assignable_type(info type_dst, info type_src);
  consteval bool is_nothrow_copy_assignable_type(info type);
  consteval bool is_nothrow_move_assignable_type(info type);

  consteval bool is_nothrow_swappable_with_type(info type1, info type2);
  consteval bool is_nothrow_swappable_type(info type);

  consteval bool is_nothrow_destructible_type(info type);

  consteval bool is_implicit_lifetime_type(info type);

  consteval bool has_virtual_destructor(info type);

  consteval bool has_unique_object_representations(info type);

  consteval bool reference_constructs_from_temporary(info type_dst, info type_src);
  consteval bool reference_converts_from_temporary(info type_dst, info type_src);

  // associated with [meta.unary.prop.query], type property queries
  consteval size_t rank(info type);
  consteval size_t extent(info type, unsigned i = 0);

  // associated with [meta.rel], type relations
  consteval bool is_same_type(info type1, info type2);
  consteval bool is_base_of_type(info type_base, info type_derived);
  consteval bool is_virtual_base_of_type(info type_base, info type_derived);
  consteval bool is_convertible_type(info type_src, info type_dst);
  consteval bool is_nothrow_convertible_type(info type_src, info type_dst);
  consteval bool is_layout_compatible_type(info type1, info type2);
  consteval bool is_pointer_interconvertible_base_of_type(info type_base, info type_derived);

  template<reflection_range R = initializer_list<info>>
    consteval bool is_invocable_type(info type, R&& type_args);
  template<reflection_range R = initializer_list<info>>
    consteval bool is_invocable_r_type(info type_result, info type, R&& type_args);

  template<reflection_range R = initializer_list<info>>
    consteval bool is_nothrow_invocable_type(info type, R&& type_args);
  template<reflection_range R = initializer_list<info>>
    consteval bool is_nothrow_invocable_r_type(info type_result, info type, R&& type_args);

  // associated with [meta.trans.cv], const-volatile modifications
  consteval info remove_const(info type);
  consteval info remove_volatile(info type);
  consteval info remove_cv(info type);
  consteval info add_const(info type);
  consteval info add_volatile(info type);
  consteval info add_cv(info type);

  // associated with [meta.trans.ref], reference modifications
  consteval info remove_reference(info type);
  consteval info add_lvalue_reference(info type);
  consteval info add_rvalue_reference(info type);

  // associated with [meta.trans.sign], sign modifications
  consteval info make_signed(info type);
  consteval info make_unsigned(info type);

  // associated with [meta.trans.arr], array modifications
  consteval info remove_extent(info type);
  consteval info remove_all_extents(info type);

  // associated with [meta.trans.ptr], pointer modifications
  consteval info remove_pointer(info type);
  consteval info add_pointer(info type);

  // associated with [meta.trans.other], other transformations
  consteval info remove_cvref(info type);
  consteval info decay(info type);
  template<reflection_range R = initializer_list<info>>
    consteval info common_type(R&& type_args);
  template<reflection_range R = initializer_list<info>>
    consteval info common_reference(R&& type_args);
  consteval info underlying_type(info type);
  template<reflection_range R = initializer_list<info>>
    consteval info invoke_result(info type, R&& type_args);
  consteval info unwrap_reference(info type);
  consteval info unwrap_ref_decay(info type);

  consteval size_t tuple_size(info type);
  consteval info tuple_element(size_t index, info type);
  consteval bool is_applicable_type(info fn, info tuple);
  consteval bool is_nothrow_applicable_type(info fn, info tuple);
  consteval info apply_result(info fn, info tuple);

  consteval size_t variant_size(info type);
  consteval info variant_alternative(size_t index, info type);

  consteval strong_ordering type_order(info type_a, info type_b);
}

```

## class exception

```text
class exception : public std::exception {
public:
  consteval exception(u8string_view what, info from,
                      source_location where = source_location::current()) noexcept;
  consteval exception(string_view what, info from,
                      source_location where = source_location::current()) noexcept;
  constexpr const char* what() const noexcept override;
  consteval u8string_view u8what() const noexcept;
  consteval info from() const noexcept;
  consteval source_location where() const noexcept;
};
```

`from()` reflects the function that threw. `where()` is the call site.

## Notes

### Static storage

- `std::define_static_array(r)` returns `std::span<const T>` over a static copy of the range. It is the usual way to iterate query results in `template for`, or to keep them in a `constexpr` variable. `T` must be a structural type, so `std::string_view` and `std::string` elements do not work; use `const char*` from `define_static_string`.
- `std::define_static_string(r)` returns a null-terminated `const char*` in static storage. The pointer is usable in structural values, annotations and template arguments.
- `std::define_static_object(v)` returns a pointer to a static copy of a structural object.
- `reflect_constant_string` and `reflect_constant_array` return a reflection of such an array instead of a pointer.
- `is_string_literal(p)` tells whether `p` points into a string literal. Such pointers cannot be template arguments or `reflect_constant` inputs.

### Names

- `identifier_of(r)` requires `has_identifier(r)`. Types such as `int`, `const S`, `std::vector<int>`, constructors, destructors, operators, conversion functions and unnamed entities have no identifier.
- A function parameter has an identifier only if every declaration of the function that names it uses the same name.
- `display_string_of(r)` is implementation-defined. Do not depend on its exact text.
- There is no qualified-name function. Walk `parent_of` to build one.

### Queries

- `type_of(r)` takes an entity with a type: variable, data member, function, parameter, enumerator, base relationship, annotation, value or object. Not a type.
- For an annotation of class type `A`, `type_of(a)` is `const A`.
- `dealias(r)` follows type and namespace aliases. `^^Alias != ^^Target` until dealiased.
- `parameters_of(f)` on a function returns parameter reflections. On a function type it returns types.
- `variable_of(p)` maps a parameter reflection to the parameter variable. It works only inside that function's body.
- `return_type_of(f)` takes a function or function type.
- `template_of(r)` and `template_arguments_of(r)` require `has_template_arguments(r)`. Dealias first.

### Members

- `members_of(r, ctx)` lists every member declaration, including implicitly declared special member functions, nested types, static members and member templates.
- `nonstatic_data_members_of(r, ctx)` lists data members in declaration order. `static_data_members_of(r, ctx)` lists static ones.
- `bases_of(r, ctx)` lists base relationships in declaration order. `type_of(b)` is the base class type. `is_virtual(b)` and the access predicates work on them.
- `subobjects_of(r, ctx)` is bases followed by non-static data members.
- `enumerators_of(r)` lists enumerators in declaration order and takes no access context. Duplicated values appear once per enumerator.
- The `has_inaccessible_*` functions tell whether a context would miss members or bases.

### Access contexts

- `access_context::current()` is the access at the point where it is written. As a default argument it is evaluated at the caller.
- `access_context::unprivileged()` sees public members.
- `access_context::unchecked()` sees all members.
- `ctx.via(cls)` gives the access of a context that names members through class `cls` (protected access through derived classes).
- `is_accessible(r, ctx)` checks one member.

### Values

- `reflect_constant(v)` reflects a value of structural type. Class values are reflected as template parameter objects.
- `reflect_object(o)` reflects an object with static storage, `reflect_function(f)` a function.
- `constant_of(r)` turns a reflection of a variable or object usable in constant expressions into a reflection of its value.
- `extract<T>(r)` returns the C++ value. `T` must be the reflected type (cv-qualifiers aside). Use `T&` to get a reference to a reflected object, and a member pointer type for a non-static data member.
- `object_of(r)` gives the object a reference variable refers to.

### Templates

- `substitute(tmpl, args)` forms a specialization; `can_substitute` checks it without failing. Arguments are reflections: types are `^^T`, values are `reflect_constant(v)`.
- A `reflection_range` is any input range of `info`, including `std::vector<std::meta::info>` and a braced list.

### Class generation

- `data_member_spec(type, options)` describes one member. `data_member_options` has `name`, `alignment`, `bit_width`, `no_unique_address` and `annotations`. `name` accepts `std::string_view` or `std::u8string_view`.
- `define_aggregate(cls, specs)` completes an incomplete class and returns `cls`. Call it from a `consteval { }` block.

### Layout

- `offset_of(m)` returns `member_offset{bytes, bits}`. `size_of`, `alignment_of` and `bit_size_of` take types, data members or objects.
- `type_order(a, b)` gives a stable total order of types, usable for sorting type lists.

### Scope

- `current_function()`, `current_class()` and `current_namespace()` reflect the enclosing entities.
