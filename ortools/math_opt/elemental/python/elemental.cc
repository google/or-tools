// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ortools/math_opt/elemental/elemental.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/elemental/arrays.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/elements.h"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

namespace py = ::pybind11;

namespace {
using ::operations_research::math_opt::AttrKey;
using ::operations_research::math_opt::is_attr_key_v;

// The conversion performed by `PyToCppEnum` is rather expensive as it requires
// doing lookups (`PyObject_{Has,Get}AttrString()`), type
// checking, and conversions. `PyToCppEnum` is invoked several times by
// `pybind11` to select which overload to call. We've found this to eventually
// be a bottleneck in benchmarks, costing nearly as much time as actually
// solving the problem. However, `PyToCppEnum` is always called with enum
// constants, which always have the same underlying python object, so we can
// cache the result for efficiency: the conversions are only done once per enum
// value, and cache lookups are essentially free (see analysis in cl/692865844).
// The cache is shared between all instantiations of `PyToCppEnum`.
struct EnumCacheValue {
  // The name of the C++ enum.
  std::string enum_name;
  // The numeric value of the enum.
  int64_t int_value = -1;
};
absl::flat_hash_map<const PyObject*, EnumCacheValue>& GetEnumCache() {
  static absl::NoDestructor<
      absl::flat_hash_map<const PyObject*, EnumCacheValue>>
      cache;
  return *cache;
}

// Converts a python enum to a C++ enum.
template <typename EnumT>
bool PyToCppEnum(PyObject* const py_enum, EnumT& cpp_enum,
                 const absl::string_view enum_name, const int num_enum_values) {
  // Note: this is not thread-safe, but pybind11 grabs the global lock before
  // calling into c++, so we never race:
  // https://pybind11.readthedocs.io/en/stable/advanced/misc.html#global-interpreter-lock-gil
  const auto [it, inserted] = GetEnumCache().try_emplace(py_enum);
  EnumCacheValue& enum_value = it->second;
  if (inserted) {
    // Cache miss, so we need to do the conversion work.
    enum_value.enum_name = py_enum->ob_type->tp_name;
    if (!PyObject_HasAttrString(py_enum, "value")) {
      // This is not even an enum, so we can't convert it. Does not happen
      // unless the user is doing something shady.
      GetEnumCache().erase(it);
      return false;
    }
    // Get the numeric value of the `.value` member.
    const auto py_value = py::reinterpret_steal<py::object>(
        PyObject_GetAttrString(py_enum, "value"));
    if (!PyLong_Check(py_value.ptr())) {
      GetEnumCache().erase(it);
      return false;
    }
    enum_value.int_value = py::int_(py_value);
  }
  // Check enum name.
  if (enum_value.enum_name != enum_name) {
    return false;
  }
  // Convert to C++ enum after checking the range to avoid UB.
  if (enum_value.int_value < 0 || enum_value.int_value >= num_enum_values) {
    return false;
  }
  cpp_enum = static_cast<EnumT>(enum_value.int_value);
  return true;
}

// An adapter to convert a string view constant to a char array (pybind11 uses
// the latter).
template <const absl::string_view& name>
struct AsArray {
  constexpr AsArray() {
#if __cplusplus >= 202002L
    std::copy(name.begin(), name.end(), array);
#else
    // `std::copy` is not constexpr before C++20.
    char* p = array;
    for (const char c : name) {
      *p++ = c;
    }
    *p = '\0';
#endif
  }

  char array[name.size() + 1] = {};
};

// An RAII object that allows creating a UTF8 string from a numpy unicode
// string. We could use `UnicodeText` for this, but unicode conversion is
// already in python, so we don't need to take a dependency on it.
class Utf8String {
 public:
  Utf8String(const char* unicode, const Py_ssize_t unicode_size_bytes)
      : unicode_object_(
            py::reinterpret_borrow<py::object>(PyUnicode_DecodeUTF32(
                unicode, unicode_size_bytes, nullptr, nullptr))) {}

  absl::string_view view() const {
    // NOTE: `PyUnicode_AsUTF8AndSize` caches the result of the conversion.
    Py_ssize_t utf8_size;
    const char* utf8_data =
        PyUnicode_AsUTF8AndSize(unicode_object_.ptr(), &utf8_size);
    return absl::string_view(utf8_data, utf8_size);
  }

 private:
  const py::object unicode_object_;
};

// A view of a 2d numpy array as ranges of `AttrKey`s.
template <typename AttrKeyT>
class AttrKeyArrayView {
 public:
  static_assert(is_attr_key_v<AttrKeyT>);
  using value_type = AttrKeyT;

  explicit AttrKeyArrayView(
      const py::array_t<int64_t>& array ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : array_(array) {
    if (array.ndim() != 2) {
      throw std::invalid_argument(absl::StrCat(
          "array has incorrect number of dimensions: ", array.ndim(),
          "; expected 2"));
    }
    if (array.shape(1) != AttrKeyT::size()) {
      throw std::invalid_argument(
          absl::StrCat("expected array shape (..., ", AttrKeyT::size(),
                       "), got (", array.shape(0), ", ", array.shape(1), ")"));
    }
  }

  ssize_t size() const { return array_.shape(0); }

  AttrKeyT operator[](const int64_t i) const {
    std::array<int64_t, AttrKeyT::size()> key_ids;
    const auto ids = array_.unchecked<2>();
    for (int j = 0; j < AttrKeyT::size(); ++j) {
      key_ids[j] = ids(i, j);
    }
    return {key_ids};
  }

 private:
  const py::array_t<int64_t>& array_;
};

// Throws an exception if the status is not OK.
// Invalid arguments are converted to `std::invalid_argument`, all other errors
// are converted to `std::runtime_error`.
void ThrowIfError(const absl::Status& status) {
  if (!status.ok()) {
    if (status.code() == absl::StatusCode::kInvalidArgument) {
      throw std::invalid_argument(std::string(status.message()));
    } else {
      throw std::runtime_error(std::string(status.message()));
    }
  }
}

// Throws a `ValueError` if the status is not OK, otherwise forwards the value.
template <typename T>
T&& ThrowIfError(absl::StatusOr<T>&& status_or ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ThrowIfError(status_or.status());
  return std::move(status_or).value();
}

// Checks that `strings` is a 1d array of dtype U.
absl::Status CheckStringArray(const py::array& strings) {
  const char dtype = strings.dtype().char_();
  if (strings.ndim() == 1 && dtype == 'U') {
    return absl::OkStatus();
  }
  return util::InvalidArgumentErrorBuilder()
         << "expected a 1d array of dtype U:, got " << strings.ndim()
         << "d array of dtype " << dtype;
}

// Checks that 1d range `values` has no duplicates.
template <typename InRange>
absl::Status CheckForDuplicates(const InRange& values) {
  // `py::array_t<T>` has no `value_type`.
  using T = std::remove_cv_t<
      std::remove_reference_t<decltype(std::declval<InRange>()[0])>>;
  absl::flat_hash_set<T> seen;
  seen.reserve(values.size());
  for (int i = 0; i < values.size(); ++i) {
    if (!seen.insert(values[i]).second) {
      return util::InvalidArgumentErrorBuilder()
             << "array has duplicates: " << values[i];
    }
  }
  return absl::OkStatus();
}

// Maps `fn` over random-accessible range `in` and returns an array of the
// results.
template <typename OutT, typename InRange, typename FnT>
py::array_t<OutT> MapToArray(const InRange& in, const FnT& fn) {
  const int64_t n = static_cast<int64_t>(in.size());
  py::array_t<OutT> out(n);
  auto out_ref = out.template mutable_unchecked<1>();
  for (int i = 0; i < n; ++i) {
    out_ref[i] = fn(in[i]);
  }
  return out;
}

}  // namespace

using ::operations_research::math_opt::AttrKey;
using ::operations_research::math_opt::AttrTypeDescriptorT;
using ::operations_research::math_opt::ElementId;
using ::operations_research::math_opt::ElementType;
using ::operations_research::math_opt::GetIndexIfAttr;
using ::py::arg;

namespace PYBIND11_NAMESPACE {
namespace detail {

// Type caster for `ElementType`.
template <>
struct type_caster<ElementType> {
  PYBIND11_TYPE_CASTER(ElementType, const_name("ElementType"));

  // Python -> C++ conversion.
  bool load(handle src, bool) {
    return PyToCppEnum(src.ptr(), value, "ElementType",
                       operations_research::math_opt::kNumElements);
  }

  // C++ -> Python conversion.
  static handle cast(ElementType src, return_value_policy, handle) {
    return PyLong_FromLong(static_cast<int>(src));
  }
};

// Type caster for `ElementId`.
template <ElementType element_type>
struct type_caster<ElementId<element_type>> {
  PYBIND11_TYPE_CASTER(ElementId<element_type>, const_name("ElementId"));

  // Python -> C++ conversion.
  bool load(handle src, bool) {
    value = ElementId<element_type>(
        static_cast<int64_t>(reinterpret_borrow<py::int_>(src.ptr())));
    return true;
  }

  // C++ -> Python conversion.
  static handle cast(ElementId<element_type> src, return_value_policy, handle) {
    return PyLong_FromLong(src.value());
  }
};

// Type caster for casting enum values from python enums to C++ `Elemental`
// enums.
template <typename AttrType>
struct type_caster<AttrType,
                   std::enable_if_t<(GetIndexIfAttr<AttrType>() >= 0)>> {
  PYBIND11_TYPE_CASTER(
      AttrType,
      const_name(AsArray<AttrTypeDescriptorT<AttrType>::kName>().array));

  // Python -> C++ conversion.
  bool load(handle src, bool) {
    return PyToCppEnum(src.ptr(), value, AttrTypeDescriptorT<AttrType>::kName,
                       AttrTypeDescriptorT<AttrType>::NumAttrs());
  }

  // C++ -> Python conversion.
  static handle cast(AttrType src, return_value_policy, handle) {
    return PyLong_FromLong(static_cast<int>(src));
  }
};

// Type caster for `AttrKey<n>`. This also canonicalizes the keys.
template <int n, typename Symmetry>
struct type_caster<AttrKey<n, Symmetry>>
    : array_caster<AttrKey<n, Symmetry>, int64_t, false, n> {};

}  // namespace detail
}  // namespace PYBIND11_NAMESPACE

namespace operations_research::math_opt {

namespace {

template <typename AttrType>
absl::Status ValidateSliceKeyIndex(const AttrType attr, const int key_index) {
  const int key_size = GetAttrKeySize<AttrType>();
  if (key_index < 0 || key_index >= key_size) {
    return util::InvalidArgumentErrorBuilder()
           << "key_index must be in [0, " << key_size
           << ") for attribute: " << attr
           << " but key_index was: " << key_index;
  }
  return absl::OkStatus();
}

// A wrapper that turns a compile-time (template) int argument of a template
// function into a normal argument so that:
//    ApplyOnIndex<n>(Fn, idx) == Fn<idx>() for 0 <= idx < n.
// Other values of idx cause a CHECK-failure.
//
// The template argument n contains the number of supported values of idx.
// ApplyOnIndex instantiates Fn for values of the template argument in [0, n)
// and then dispatches between them dynamically.
//
// Fn<idx> must have the same return type for all values of idx, and the return
// type must be movable.
//
// This function take time linear in n.
template <int n, typename Fn,
          typename R =
              std::invoke_result_t<decltype(&Fn::template operator()<0>), Fn>>
R ApplyOnIndex(Fn fn, const int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, n);
  std::optional<R> result;
  // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
  ForEachIndex<n>([index, &fn, &result]<int k>() {
    if (k == index) {
      result = fn.template operator()<k>();
    }
  });
  CHECK(result.has_value());
  return *std::move(result);
}

// Like Elemental::Slice, but where the slicing index is given at runtime
// instead of compile time. Bad values of either key_index or element_id give
// Status errors.
template <typename AttrType>
absl::StatusOr<std::vector<AttrKeyFor<AttrType>>> DynamicSlice(
    const Elemental& e, const AttrType attr, const int key_index,
    const int element_id) {
  RETURN_IF_ERROR(ValidateSliceKeyIndex(attr, key_index));
  return ApplyOnIndex<GetAttrKeySize<AttrType>()>(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      [&e, attr, element_id]<int k>() {
        return e.Slice<k, Elemental::StatusPolicy>(attr, element_id);
      },
      key_index);
}

// Like Elemental::GetSliceSize, but where the slicing index is given at runtime
// instead of compile time. Bad values of either key_index or element_id give
// Status errors.
template <typename AttrType>
absl::StatusOr<int64_t> DynamicGetSliceSize(const Elemental& e,
                                            const AttrType attr,
                                            const int key_index,
                                            const int element_id) {
  RETURN_IF_ERROR(ValidateSliceKeyIndex(attr, key_index));
  return ApplyOnIndex<GetAttrKeySize<AttrType>()>(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      [&e, attr, element_id]<int k>() {
        return e.GetSliceSize<k, Elemental::StatusPolicy>(attr, element_id);
      },
      key_index);
}

// Converts a vector attribute keys (with each with size key size), with
// num_keys elements to a numpy array with shape (num_keys, key_size).
template <int key_size, typename Symmetry>
py::array_t<int64_t> ConvertAttrKeysToNpArray(
    const absl::Span<const AttrKey<key_size, Symmetry>> keys) {
  const int64_t num_keys = static_cast<int64_t>(keys.size());
  py::array_t<int64_t> result(
      py::array_t<int64_t>::ShapeContainer({num_keys, key_size}), nullptr);
  auto fast_result = result.mutable_unchecked<2>();
  for (int64_t i = 0; i < num_keys; ++i) {
    for (int j = 0; j < key_size; ++j) {
      fast_result(i, j) = keys[i][j];
    }
  }
  return result;
}

// Checks that the elements in `keys` exist in `e`.
template <typename AttrType>
absl::Status CheckForElementExistence(
    const Elemental& e, const AttrType attr,
    const AttrKeyArrayView<AttrKeyFor<AttrType>>& keys) {
  const int64_t num_keys = keys.size();
  for (int i = 0; i < num_keys; ++i) {
    const auto key = keys[i];
    for (int j = 0; j < GetAttrKeySize<AttrType>(); ++j) {
      const auto element_type = GetElementTypes<AttrType>(attr)[j];
      if (!e.ElementExistsUntyped(element_type, key[j])) {
        return util::InvalidArgumentErrorBuilder()
               << element_type << " id " << key[j] << " does not exist";
      }
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<Elemental::DiffHandle> GetDiffHandle(const Elemental& elemental,
                                                    const int64_t diff_id) {
  std::optional<Elemental::DiffHandle> handle =
      elemental.GetDiffHandle(diff_id);
  if (handle == std::nullopt) {
    return util::InvalidArgumentErrorBuilder()
           << "no diff with id: " << diff_id;
  }
  return *handle;
}

template <typename AttrType>
py::array_t<int64_t> slice_attr(const Elemental& e, const AttrType attr,
                                const int key_index, const int element_id) {
  const std::vector<AttrKeyFor<AttrType>> slice =
      ThrowIfError(DynamicSlice(e, attr, key_index, element_id));
  return ConvertAttrKeysToNpArray(absl::MakeConstSpan(slice));
}

template <typename AttrType>
int64_t get_attr_slice_size(const Elemental& e, const AttrType attr,
                            const int key_index, const int element_id) {
  return ThrowIfError(DynamicGetSliceSize(e, attr, key_index, element_id));
}

}  // namespace

PYBIND11_MODULE(cpp_elemental, py_module) {
  pybind11_protobuf::ImportNativeProtoCasters();
  // Wrap `Elemental`.
  py::class_<Elemental, std::unique_ptr<Elemental>> elemental(py_module,
                                                              "CppElemental");
  elemental.def(py::init<std::string, std::string>(), py::kw_only(),
                arg("model_name") = "", arg("primary_objective_name") = "");
  elemental.def_property_readonly("model_name", &Elemental::model_name);
  elemental.def_property_readonly("primary_objective_name",
                                  &Elemental::primary_objective_name);
  elemental.def("__repr__", [](const Elemental& e) { return e.DebugString(); });
  elemental.def(
      "clone",
      [](Elemental& e, std::optional<std::string> new_model_name) {
        return std::make_unique<Elemental>(e.Clone(new_model_name));
      },
      py::kw_only(), arg("new_model_name") = std::nullopt);

  elemental.def(
      "export_model",
      [](const Elemental& e, bool remove_names) {
        return ThrowIfError(e.ExportModel(remove_names));
      },
      py::kw_only(), arg("remove_names") = false);

  elemental.def("add_diff", [](Elemental& e) { return e.AddDiff().id(); });

  elemental.def("delete_diff", [](Elemental& e, const int64_t diff_handle) {
    const Elemental::DiffHandle h = ThrowIfError(GetDiffHandle(e, diff_handle));
    CHECK(e.DeleteDiff(h));
  });

  elemental.def("advance_diff", [](Elemental& e, const int64_t diff_handle) {
    const Elemental::DiffHandle h = ThrowIfError(GetDiffHandle(e, diff_handle));
    CHECK(e.Advance(h));
  });

  elemental.def(
      "export_model_update",
      [](Elemental& e, const int64_t diff_handle, const bool remove_names) {
        const Elemental::DiffHandle h =
            ThrowIfError(GetDiffHandle(e, diff_handle));
        return ThrowIfError(e.ExportModelUpdate(h, remove_names));
      },
      arg("diff_handle"), py::kw_only(), arg("remove_names") = false);

  // Element counting operations.
  elemental.def("get_num_elements", &Elemental::NumElements,
                arg("element_type"));
  elemental.def("get_next_element_id", &Elemental::NextElementId,
                arg("element_type"));

  elemental.def("ensure_next_element_id_at_least",
                &Elemental::EnsureNextElementIdAtLeastUntyped,
                arg("element_type"), arg("element_id"));

  // Non-batch element operations.
  elemental.def("add_element", &Elemental::AddElementUntyped,
                arg("element_type"), arg("name"));
  elemental.def("delete_element", &Elemental::DeleteElementUntyped,
                arg("element_type"), arg("element_id"));
  elemental.def("element_exists", &Elemental::ElementExistsUntyped,
                arg("element_type"), arg("element_id"));
  elemental.def(
      "get_element_name",
      [](Elemental& e, const ElementType elem_type, const int64_t id) {
        return ThrowIfError(e.GetElementNameUntyped(elem_type, id));
      },
      arg("element_type"), arg("element_id"));

  // Batch Element operations.
  elemental.def(
      "add_elements",
      [](Elemental& e, const ElementType elem_type, const int num_elements) {
        py::array_t<int64_t> result(num_elements);
        auto ids = result.mutable_unchecked<1>();
        for (int i = 0; i < num_elements; ++i) {
          ids(i) = e.AddElementUntyped(elem_type, "");
        }
        return result;
      },
      arg("element_type"), arg("num_elements"));

  elemental.def(
      "add_named_elements",
      // Note: `array_t` only supports POD types, so `np.array(str)` is passed
      // as type-erased `py::array` (see
      // https://github.com/pybind/pybind11/issues/1775). Each element is a
      // `char[array.itemsize]` containing unicode data.
      [](Elemental& e, const ElementType elem_type, const py::array& names) {
        ThrowIfError(CheckStringArray(names));
        const int64_t num_elements = static_cast<int64_t>(names.shape(0));
        const char* unicode_data =
            static_cast<const char*>(names.request().ptr);
        const ssize_t itemsize_bytes = names.request().itemsize;
        py::array_t<int64_t> result(names.size());
        auto ids = result.mutable_unchecked<1>();
        for (int i = 0; i < num_elements; ++i) {
          ids(i) = e.AddElementUntyped(
              elem_type,
              Utf8String(unicode_data + i * itemsize_bytes, itemsize_bytes)
                  .view());
        }
        return result;
      },
      arg("element_type"), arg("names"));

  elemental.def(
      "delete_elements",
      [](Elemental& e, const ElementType elem_type,
         const py::array_t<int64_t>& elements) {
        ThrowIfError(CheckForDuplicates(elements.unchecked<1>()));
        return MapToArray<bool>(elements.unchecked<1>(), [&](int64_t id) {
          return e.DeleteElementUntyped(elem_type, id);
        });
      },
      arg("element_type"), arg("num_elements"));

  elemental.def(
      "elements_exist",
      [](Elemental& e, const ElementType elem_type,
         const py::array_t<int64_t>& elements) {
        return MapToArray<bool>(elements.unchecked<1>(), [&](int64_t id) {
          return e.ElementExistsUntyped(elem_type, id);
        });
      },
      arg("element_type"), arg("elements"));

  elemental.def(
      "get_element_names",
      [](Elemental& e, const ElementType elem_type,
         py::array_t<int64_t>& elements) {
        const int64_t num_elements = static_cast<int64_t>(elements.shape(0));
        auto ids = elements.unchecked<1>();
        std::vector<absl::string_view> names;
        names.reserve(num_elements);
        for (int i = 0; i < num_elements; ++i) {
          names.emplace_back() =
              ThrowIfError(e.GetElementNameUntyped(elem_type, ids(i)));
        }
        return py::array(py::cast(names));
      },
      arg("element_type"), arg("elements"));

  elemental.def(
      "get_elements",
      [](Elemental& e, ElementType elem_type) {
        const auto all_elements = e.AllElementsUntyped(elem_type);
        return py::array_t<int64_t>(all_elements.size(), all_elements.data());
      },
      arg("element_type"));

  // Export attribute operations.
  ForEach(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      [&elemental]<typename Descriptor>(const Descriptor&) {
        using AttrType = typename Descriptor::AttrType;
        using ValueType = typename Descriptor::ValueType;
        using Key = AttrKeyFor<AttrType>;

        elemental.def("clear_attr", &Elemental::AttrClear<AttrType>,
                      arg("attr"));

        // Get:
        elemental.def(
            "get_attr",
            [](const Elemental& e, AttrType attr, const Key& key) {
              return ThrowIfError(
                  e.GetAttr<Elemental::StatusPolicy>(attr, key));
            },
            arg("attr"), arg("key"));
        elemental.def(
            "get_attrs",
            [](const Elemental& e, const AttrType a,
               py::array_t<int64_t> keys) {
              return MapToArray<ValueType>(
                  AttrKeyArrayView<Key>(keys), [&](Key key) {
                    return ThrowIfError(
                        e.GetAttr<Elemental::StatusPolicy>(a, key));
                  });
            },
            arg("attr"), arg("keys"));

        // Set:
        elemental.def(
            "set_attr",
            [](Elemental& e, AttrType attr, const Key& key,
               const ValueTypeFor<AttrType> value) {
              return ThrowIfError(
                  e.SetAttr<Elemental::StatusPolicy>(attr, key, value));
            },
            arg("attr"), arg("key"), arg("value"));
        elemental.def(
            "set_attrs",
            [](Elemental& e, AttrType attr, const py::array_t<int64_t>& keys,
               const py::array_t<ValueType>& values) {
              const AttrKeyArrayView<Key> keys_view(keys);
              // We need to check for duplicates and existence first, as we
              // don't want to end up with a partially mutated state on error.
              ThrowIfError(CheckForDuplicates(keys_view));
              ThrowIfError(CheckForElementExistence(e, attr, keys_view));
              const auto values_view = values.template unchecked<1>();
              const int64_t num_elements = keys_view.size();
              for (int i = 0; i < num_elements; ++i) {
                e.SetAttr<Elemental::UBPolicy>(attr, keys_view[i],
                                               values_view[i]);
              }
            },
            arg("attr"), arg("key"), arg("value"));

        // IsNonDefault:
        elemental.def(
            "is_attr_non_default",
            [](const Elemental& e, AttrType attr, const Key& key) {
              return ThrowIfError(
                  e.AttrIsNonDefault<Elemental::StatusPolicy>(attr, key));
            },
            arg("attr"), arg("key"));
        elemental.def(
            "bulk_is_attr_non_default",
            [](const Elemental& e, AttrType attr,
               const py::array_t<int64_t>& keys) {
              return MapToArray<bool>(
                  AttrKeyArrayView<Key>(keys), [&](Key key) {
                    return ThrowIfError(
                        e.AttrIsNonDefault<Elemental::StatusPolicy>(attr, key));
                  });
            },
            arg("attr"), arg("key"));
        if constexpr (GetAttrKeySize<AttrType>() >= 1) {
          elemental.def("slice_attr", &slice_attr<AttrType>);
          elemental.def("get_attr_slice_size", &get_attr_slice_size<AttrType>);
        }

        // NumNonDefaults:
        elemental.def("get_attr_num_non_defaults",
                      &Elemental::AttrNumNonDefaults<AttrType>, arg("attr"));

        // GetNonDefaults:
        elemental.def(
            "get_attr_non_defaults",
            [](const Elemental& e, AttrType attr) {
              const std::vector<AttrKeyFor<AttrType>> non_defaults =
                  e.AttrNonDefaults(attr);
              return ConvertAttrKeysToNpArray(
                  absl::MakeConstSpan(non_defaults));
            },
            arg("attr"));
      },
      AllAttrTypeDescriptors{});
}

}  // namespace operations_research::math_opt
