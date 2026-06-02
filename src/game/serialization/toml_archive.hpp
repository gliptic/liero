#pragma once

// Custom cereal archive that reads/writes TOML using toml++.
//
// Mirrors the structure of cereal's bundled json.hpp: a single
// `serialize()` per type works for both PortableBinaryArchive and these
// TOML archives. Names come from CEREAL_NVP / make_nvp. Variable-sized
// containers (std::vector, etc.) use sizeTag, which triggers an array
// in TOML; everything else lands in a table.
//
// Limitations vs. JSON:
//  - TOML has no null; missing keys are the way to represent absence.
//  - TOML integers are 64-bit signed; small types round-trip through
//    int64_t with a checked cast on load.
//  - Arrays of primitives serialize inline ([1,2,3]); arrays of tables
//    use TOML's `[[section]]` syntax when emitted at the top level of
//    a table (toml++ chooses inline vs. block automatically).

#include <cereal/cereal.hpp>
#include <toml++/toml.hpp>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cereal {

// ============================================================
// Output
// ============================================================

class TomlOutputArchive : public cereal::OutputArchive<TomlOutputArchive>,
                          public cereal::traits::TextArchive {
 public:
  TomlOutputArchive(std::ostream& s) : cereal::OutputArchive<TomlOutputArchive>(this), out_(s) {
    Frame root_frame;
    root_frame.container = &root_;
    root_frame.is_array = false;
    root_frame.materialized = true;
    frames_.push_back(std::move(root_frame));
  }

  ~TomlOutputArchive() { out_ << root_ << "\n"; }

  // Cereal extension API
  void setNextName(char const* name) { next_name_ = name; }

  void startNode() {
    Frame f;
    f.container = nullptr;
    f.is_array = false;
    f.materialized = false;
    f.name_in_parent = TakeName(frames_.back());
    frames_.push_back(std::move(f));
  }

  void finishNode() {
    if (!frames_.back().materialized) MaterializeAll();
    frames_.pop_back();
  }

  // sizeTag prologue calls this to mark the just-started node as an array.
  void MakeArray() { frames_.back().is_array = true; }

  void saveValue(bool v) { PutValue(v); }
  void saveValue(std::int32_t v) { PutValue(static_cast<std::int64_t>(v)); }
  void saveValue(std::uint32_t v) { PutValue(static_cast<std::int64_t>(v)); }
  void saveValue(std::int64_t v) { PutValue(v); }
  void saveValue(std::uint64_t v) {
    // toml++ stores ints as int64_t; values above INT64_MAX would corrupt.
    PutValue(static_cast<std::int64_t>(v));
  }
  void saveValue(double v) { PutValue(v); }
  void saveValue(float v) { PutValue(static_cast<double>(v)); }
  void saveValue(std::string const& s) { PutValue(s); }

 private:
  struct Frame {
    toml::node* container;  // valid only after materialization
    bool is_array;
    bool materialized;
    std::string name_in_parent;
    std::uint32_t name_counter = 0;
  };

  std::ostream& out_;
  toml::table root_;
  std::vector<Frame> frames_;
  char const* next_name_ = nullptr;

  std::string TakeName(Frame& parent) {
    if (next_name_) {
      std::string n = next_name_;
      next_name_ = nullptr;
      return n;
    }
    return "value" + std::to_string(parent.name_counter++);
  }

  void MaterializeAll() {
    // Root (frame 0) is always materialized; walk down from there.
    for (std::size_t i = 1; i < frames_.size(); ++i) {
      if (!frames_[i].materialized) MaterializeAt(i);
    }
  }

  void MaterializeAt(std::size_t i) {
    Frame& f = frames_[i];
    Frame& parent = frames_[i - 1];
    if (parent.container->is_table()) {
      auto* t = parent.container->as_table();
      if (f.is_array)
        t->insert_or_assign(f.name_in_parent, toml::array{});
      else
        t->insert_or_assign(f.name_in_parent, toml::table{});
      f.container = t->get(f.name_in_parent);
    } else {
      auto* a = parent.container->as_array();
      if (f.is_array)
        a->push_back(toml::array{});
      else
        a->push_back(toml::table{});
      f.container = &a->back();
    }
    f.materialized = true;
  }

  template <typename T>
  void PutValue(T const& v) {
    MaterializeAll();
    Frame& f = frames_.back();
    if (f.container->is_table()) {
      f.container->as_table()->insert_or_assign(TakeName(f), v);
    } else {
      f.container->as_array()->push_back(v);
    }
  }
};

// ============================================================
// Input
// ============================================================

struct TomlParseError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

class TomlInputArchive : public cereal::InputArchive<TomlInputArchive>,
                         public cereal::traits::TextArchive {
 public:
  TomlInputArchive(std::istream& s) : cereal::InputArchive<TomlInputArchive>(this) {
    try {
      auto parsed = toml::parse(s);
      root_ = std::move(parsed);
    } catch (toml::parse_error const& e) {
      throw TomlParseError(std::string("TOML parse error: ") + e.what());
    }
    Frame f;
    f.node = &root_;
    f.is_array = false;
    f.index = 0;
    frames_.push_back(std::move(f));
  }

  void setNextName(char const* name) { next_name_ = name; }

  void startNode() {
    Frame f;
    f.is_array = false;
    f.index = 0;

    Frame& parent = frames_.back();
    toml::node* child = Lookup(parent);
    f.node = child;  // may be null if missing
    frames_.push_back(std::move(f));
  }

  void finishNode() {
    bool was_array_element = false;
    // If parent is array, advance its index.
    if (frames_.size() >= 2) {
      Frame& parent = frames_[frames_.size() - 2];
      if (parent.node && parent.node->is_array() && next_name_ == nullptr) {
        was_array_element = true;
      }
    }
    frames_.pop_back();
    if (was_array_element) frames_.back().index += 1;
    next_name_ = nullptr;
  }

  // sizeTag prologue marks the upcoming/current node as array context;
  // loadSize reports the size of that array.
  void MakeArray() { frames_.back().is_array = true; }

  void LoadSize(cereal::size_type& size) {
    Frame& f = frames_.back();
    if (f.node && f.node->is_array())
      size = f.node->as_array()->size();
    else
      size = 0;
  }

  void loadValue(bool& v) {
    if (auto* n = Lookup(frames_.back()); n && n->is_boolean()) v = n->as_boolean()->get();
    Advance();
  }

  void loadValue(std::int32_t& v) {
    if (auto* n = Lookup(frames_.back()); n && n->is_integer())
      v = static_cast<std::int32_t>(n->as_integer()->get());
    Advance();
  }
  void loadValue(std::uint32_t& v) {
    if (auto* n = Lookup(frames_.back()); n && n->is_integer())
      v = static_cast<std::uint32_t>(n->as_integer()->get());
    Advance();
  }
  void loadValue(std::int64_t& v) {
    if (auto* n = Lookup(frames_.back()); n && n->is_integer()) v = n->as_integer()->get();
    Advance();
  }
  void loadValue(std::uint64_t& v) {
    if (auto* n = Lookup(frames_.back()); n && n->is_integer())
      v = static_cast<std::uint64_t>(n->as_integer()->get());
    Advance();
  }
  void loadValue(double& v) {
    if (auto* n = Lookup(frames_.back())) {
      if (n->is_floating_point())
        v = n->as_floating_point()->get();
      else if (n->is_integer())
        v = static_cast<double>(n->as_integer()->get());
    }
    Advance();
  }
  void loadValue(float& v) {
    double d = v;
    loadValue(d);
    v = static_cast<float>(d);
  }
  void loadValue(std::string& v) {
    if (auto* n = Lookup(frames_.back()); n && n->is_string()) v = n->as_string()->get();
    Advance();
  }

 private:
  struct Frame {
    toml::node* node;  // current container (table or array), may be null
    bool is_array;
    std::size_t index;  // next array index to consume
  };

  toml::table root_;
  std::vector<Frame> frames_;
  char const* next_name_ = nullptr;

  toml::node* Lookup(Frame& f) {
    if (!f.node) {
      next_name_ = nullptr;
      return nullptr;
    }
    if (f.node->is_table()) {
      if (next_name_) {
        auto* t = f.node->as_table();
        auto it = t->find(next_name_);
        next_name_ = nullptr;
        if (it == t->end()) return nullptr;
        return &it->second;
      }
      next_name_ = nullptr;
      return nullptr;
    }
    if (f.node->is_array()) {
      auto* a = f.node->as_array();
      next_name_ = nullptr;
      if (f.index >= a->size()) return nullptr;
      return a->get(f.index);
    }
    next_name_ = nullptr;
    return nullptr;
  }

  void Advance() {
    Frame& f = frames_.back();
    if (f.node && f.node->is_array()) f.index += 1;
  }
};

// ============================================================
// cereal prologue/epilogue/save/load for the TOML archives
// ============================================================

// ---- NameValuePair ----
template <class T>
inline void prologue(TomlOutputArchive&, NameValuePair<T> const&) {}
template <class T>
inline void prologue(TomlInputArchive&, NameValuePair<T> const&) {}
template <class T>
inline void epilogue(TomlOutputArchive&, NameValuePair<T> const&) {}
template <class T>
inline void epilogue(TomlInputArchive&, NameValuePair<T> const&) {}

template <class T>
inline void CEREAL_SAVE_FUNCTION_NAME(TomlOutputArchive& ar, NameValuePair<T> const& t) {
  ar.setNextName(t.name);
  ar(t.value);
}
template <class T>
inline void CEREAL_LOAD_FUNCTION_NAME(TomlInputArchive& ar, NameValuePair<T>& t) {
  ar.setNextName(t.name);
  ar(t.value);
}

// ---- sizeTag ----
template <class T>
inline void prologue(TomlOutputArchive& ar, SizeTag<T> const&) {
  ar.MakeArray();
}
template <class T>
inline void prologue(TomlInputArchive& ar, SizeTag<T> const&) {
  ar.MakeArray();
}
template <class T>
inline void epilogue(TomlOutputArchive&, SizeTag<T> const&) {}
template <class T>
inline void epilogue(TomlInputArchive&, SizeTag<T> const&) {}

template <class T>
inline void CEREAL_SAVE_FUNCTION_NAME(TomlOutputArchive&, SizeTag<T> const&) {
  // size is implicit in the TOML array length
}
template <class T>
inline void CEREAL_LOAD_FUNCTION_NAME(TomlInputArchive& ar, SizeTag<T>& st) {
  ar.LoadSize(st.size);
}

// ---- Generic object types (non-arithmetic, non-string) ----
template <class T, traits::EnableIf<
                       !std::is_arithmetic<T>::value,
                       !traits::has_minimal_base_class_serialization<
                           T, traits::has_minimal_output_serialization, TomlOutputArchive>::value,
                       !traits::has_minimal_output_serialization<T, TomlOutputArchive>::value> =
                       traits::sfinae>
inline void prologue(TomlOutputArchive& ar, T const&) {
  ar.startNode();
}
template <class T,
          traits::EnableIf<!std::is_arithmetic<T>::value,
                           !traits::has_minimal_base_class_serialization<
                               T, traits::has_minimal_input_serialization, TomlInputArchive>::value,
                           !traits::has_minimal_input_serialization<T, TomlInputArchive>::value> =
              traits::sfinae>
inline void prologue(TomlInputArchive& ar, T const&) {
  ar.startNode();
}
template <class T, traits::EnableIf<
                       !std::is_arithmetic<T>::value,
                       !traits::has_minimal_base_class_serialization<
                           T, traits::has_minimal_output_serialization, TomlOutputArchive>::value,
                       !traits::has_minimal_output_serialization<T, TomlOutputArchive>::value> =
                       traits::sfinae>
inline void epilogue(TomlOutputArchive& ar, T const&) {
  ar.finishNode();
}
template <class T,
          traits::EnableIf<!std::is_arithmetic<T>::value,
                           !traits::has_minimal_base_class_serialization<
                               T, traits::has_minimal_input_serialization, TomlInputArchive>::value,
                           !traits::has_minimal_input_serialization<T, TomlInputArchive>::value> =
              traits::sfinae>
inline void epilogue(TomlInputArchive& ar, T const&) {
  ar.finishNode();
}

// ---- Arithmetic ----
template <class T, traits::EnableIf<std::is_arithmetic<T>::value> = traits::sfinae>
inline void prologue(TomlOutputArchive&, T const&) {}
template <class T, traits::EnableIf<std::is_arithmetic<T>::value> = traits::sfinae>
inline void prologue(TomlInputArchive&, T const&) {}
template <class T, traits::EnableIf<std::is_arithmetic<T>::value> = traits::sfinae>
inline void epilogue(TomlOutputArchive&, T const&) {}
template <class T, traits::EnableIf<std::is_arithmetic<T>::value> = traits::sfinae>
inline void epilogue(TomlInputArchive&, T const&) {}

template <class T, traits::EnableIf<std::is_arithmetic<T>::value> = traits::sfinae>
inline void CEREAL_SAVE_FUNCTION_NAME(TomlOutputArchive& ar, T const& t) {
  ar.saveValue(t);
}
template <class T, traits::EnableIf<std::is_arithmetic<T>::value> = traits::sfinae>
inline void CEREAL_LOAD_FUNCTION_NAME(TomlInputArchive& ar, T& t) {
  ar.loadValue(t);
}

// ---- String ----
template <class CharT, class Traits, class Alloc>
inline void prologue(TomlOutputArchive&, std::basic_string<CharT, Traits, Alloc> const&) {}
template <class CharT, class Traits, class Alloc>
inline void prologue(TomlInputArchive&, std::basic_string<CharT, Traits, Alloc> const&) {}
template <class CharT, class Traits, class Alloc>
inline void epilogue(TomlOutputArchive&, std::basic_string<CharT, Traits, Alloc> const&) {}
template <class CharT, class Traits, class Alloc>
inline void epilogue(TomlInputArchive&, std::basic_string<CharT, Traits, Alloc> const&) {}

template <class CharT, class Traits, class Alloc>
inline void CEREAL_SAVE_FUNCTION_NAME(TomlOutputArchive& ar,
                                      std::basic_string<CharT, Traits, Alloc> const& s) {
  ar.saveValue(s);
}
template <class CharT, class Traits, class Alloc>
inline void CEREAL_LOAD_FUNCTION_NAME(TomlInputArchive& ar,
                                      std::basic_string<CharT, Traits, Alloc>& s) {
  ar.loadValue(s);
}

}  // namespace cereal

CEREAL_REGISTER_ARCHIVE(cereal::TomlOutputArchive)
CEREAL_REGISTER_ARCHIVE(cereal::TomlInputArchive)
CEREAL_SETUP_ARCHIVE_TRAITS(cereal::TomlInputArchive, cereal::TomlOutputArchive)
