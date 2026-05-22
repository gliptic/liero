#pragma once

#include <toml++/toml.hpp>

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace gvl {
namespace toml {

struct parse_error : std::runtime_error {
  parse_error() : runtime_error("TOML parse error") {}
  parse_error(std::string const& msg) : runtime_error(msg) {}
};

// Forward declarations
template <typename T, int N>
inline void resize(T (&arr)[N], std::size_t s) {
  (void)arr;
  (void)s;
}

template <typename T>
inline void resize(std::vector<T>& v, std::size_t s) {
  v.resize(s);
}

// ============================================================
// Writer: builds a toml::table, serializes to a stream on flush
// ============================================================

template <typename Writer>
struct writer {
  static bool const in = false;
  static bool const out = true;

  writer(Writer& w) : w(w) { stack.push_back(&root); }

  ~writer() { flush(); }

  void flush() {
    if (flushed)
      return;
    flushed = true;
    std::ostringstream ss;
    ss << root;
    auto str = ss.str();
    for (char c : str)
      w.put(c);
  }

  Writer& w;
  ::toml::table root;
  std::vector<::toml::node*> stack;
  bool flushed = false;

  ::toml::table* cur_table() {
    auto* n = stack.back();
    return n->as_table();
  }

  ::toml::array* cur_array() {
    auto* n = stack.back();
    return n->as_array();
  }

  void set(char const* name, ::toml::node&& val) {
    if (name) {
      cur_table()->insert_or_assign(name, std::move(val));
    } else {
      // Unnamed: append to current array
      cur_array()->push_back(std::move(val));
    }
  }

  template <typename F>
  writer& obj(char const* name, F func) {
    ::toml::table t;
    if (name) {
      cur_table()->insert_or_assign(name, std::move(t));
      stack.push_back(cur_table()->at(name).as_table());
    } else if (cur_array()) {
      cur_array()->push_back(std::move(t));
      stack.push_back(cur_array()->back().as_table());
    } else {
      // No name and no array context: write fields directly into current table
      func();
      return *this;
    }
    func();
    stack.pop_back();
    return *this;
  }

  template <typename A, typename F>
  writer& arr(char const* name, A& arr, F func) {
    ::toml::array a;
    if (name) {
      cur_table()->insert_or_assign(name, std::move(a));
      stack.push_back(cur_table()->at(name).as_array());
    } else {
      cur_array()->push_back(std::move(a));
      stack.push_back(cur_array()->back().as_array());
    }
    for (auto& e : arr)
      func(e);
    stack.pop_back();
    return *this;
  }

  template <typename A, typename F>
  writer& array_obj(char const* name, A& arr, F func) {
    ::toml::array a;
    if (name) {
      cur_table()->insert_or_assign(name, std::move(a));
      stack.push_back(cur_table()->at(name).as_array());
    } else {
      cur_array()->push_back(std::move(a));
      stack.push_back(cur_array()->back().as_array());
    }
    for (auto& e : arr) {
      ::toml::table t;
      cur_array()->push_back(std::move(t));
      stack.push_back(cur_array()->back().as_table());
      func(e);
      stack.pop_back();
    }
    stack.pop_back();
    return *this;
  }

  writer& i32(int32_t v) {
    cur_array()->push_back(static_cast<int64_t>(v));
    return *this;
  }

  writer& i32(char const* name, int32_t v) {
    if (name)
      cur_table()->insert_or_assign(name, static_cast<int64_t>(v));
    else
      cur_array()->push_back(static_cast<int64_t>(v));
    return *this;
  }

  writer& u32(uint32_t v) {
    cur_array()->push_back(static_cast<int64_t>(v));
    return *this;
  }

  writer& u32(char const* name, uint32_t v) {
    if (name)
      cur_table()->insert_or_assign(name, static_cast<int64_t>(v));
    else
      cur_array()->push_back(static_cast<int64_t>(v));
    return *this;
  }

  writer& b(bool v) {
    cur_array()->push_back(v);
    return *this;
  }

  writer& b(char const* name, bool v) {
    if (name)
      cur_table()->insert_or_assign(name, v);
    else
      cur_array()->push_back(v);
    return *this;
  }

  writer& null(char const* /*name*/) {
    // TOML has no null; we simply omit the key.
    // When called from ref resolver, this is intentional — absent key = null.
    return *this;
  }

  template <typename T, typename Resolver>
  writer& ref(char const* name, T const& v, Resolver resolver) {
    // The resolver will call str(0, s) or null(0) on us.
    // We temporarily redirect unnamed writes to capture the value,
    // then assign it under `name`.
    refName = name;
    inRef = true;
    resolver.v2r(*this, v);
    inRef = false;
    refName = nullptr;
    return *this;
  }

  writer& str(std::string& s) {
    if (inRef && refName) {
      cur_table()->insert_or_assign(refName, s);
    } else {
      cur_array()->push_back(s);
    }
    return *this;
  }

  writer& str(char const* name, std::string& s) {
    if (!name && inRef && refName) {
      cur_table()->insert_or_assign(refName, s);
    } else if (name) {
      cur_table()->insert_or_assign(name, s);
    } else {
      cur_array()->push_back(s);
    }
    return *this;
  }

  char const* refName = nullptr;
  bool inRef = false;
};

// ============================================================
// Reader: parses from a stream into toml::table, then navigates
// ============================================================

template <typename Reader>
struct reader {
  static bool const in = true;
  static bool const out = false;

  reader(Reader& r) : r(r) {
    // Read entire stream into string
    std::string content;
    try {
      for (;;)
        content.push_back(static_cast<char>(r.get()));
    } catch (std::runtime_error&) {
      // EOF reached
    }

    auto result = ::toml::parse(content);
    root = std::move(result);
    stack.push_back(&root);
  }

  Reader& r;
  ::toml::table root;
  std::vector<::toml::node*> stack;

  ::toml::table* cur_table() {
    auto* n = stack.back();
    return n->as_table();
  }

  ::toml::array* cur_array() {
    auto* n = stack.back();
    return n->as_array();
  }

  // For array iteration
  ::toml::node* cur_node() { return stack.back(); }

  ::toml::node* find(char const* name) {
    if (name) {
      auto* t = cur_table();
      if (!t)
        return nullptr;
      auto it = t->find(name);
      if (it == t->end())
        return nullptr;
      return &it->second;
    } else {
      return cur_node();
    }
  }

  template <typename F>
  reader& obj(char const* name, F func) {
    auto* node = find(name);
    if (!node || !node->is_table())
      return *this;  // Missing or wrong type: keep defaults
    stack.push_back(node);
    func();
    stack.pop_back();
    return *this;
  }

  template <typename A, typename F>
  reader& arr(char const* name, A& arr, F func) {
    auto* node = find(name);
    if (!node || !node->is_array())
      return *this;  // Missing: keep defaults
    auto* a = node->as_array();
    resize(arr, a->size());

    std::size_t idx = 0;
    for (auto& e : arr) {
      if (idx >= a->size())
        break;
      stack.push_back(&(*a)[idx]);
      func(e);
      stack.pop_back();
      ++idx;
    }
    return *this;
  }

  template <typename A, typename F>
  reader& array_obj(char const* name, A& a, F func) {
    return arr(name, a, std::move(func));
  }

  reader& i32(char const* name, int32_t& v) {
    auto* node = find(name);
    if (!node || !node->is_integer())
      return *this;
    v = static_cast<int32_t>(node->as_integer()->get());
    return *this;
  }

  reader& u32(char const* name, uint32_t& v) {
    auto* node = find(name);
    if (!node || !node->is_integer())
      return *this;
    v = static_cast<uint32_t>(node->as_integer()->get());
    return *this;
  }

  reader& b(char const* name, bool& v) {
    auto* node = find(name);
    if (!node || !node->is_boolean())
      return *this;
    v = node->as_boolean()->get();
    return *this;
  }

  template <typename T, typename Resolver>
  reader& ref(char const* name, T& v, Resolver resolver) {
    auto* node = find(name);
    if (!node) {
      resolver.r2v(v);
    } else if (node->is_string()) {
      resolver.r2v(v, std::string(node->as_string()->get()));
    } else {
      resolver.r2v(v);
    }
    return *this;
  }

  reader& str(char const* name, std::string& s) {
    auto* node = find(name);
    if (!node || !node->is_string())
      return *this;
    s = node->as_string()->get();
    return *this;
  }
};

}  // namespace toml

// Simple writer that appends to a std::string (for TOML-to-string serialization)
struct string_writer {
  std::string& buf;
  string_writer(std::string& buf) : buf(buf) {}
  void put(uint8_t c) { buf.push_back(static_cast<char>(c)); }
};

// Simple reader that reads from a std::string
struct string_reader {
  std::string const& buf;
  std::size_t pos = 0;
  string_reader(std::string const& buf) : buf(buf) {}
  uint8_t get() {
    if (pos >= buf.size())
      throw std::runtime_error("string_reader: unexpected end of input");
    return static_cast<uint8_t>(buf[pos++]);
  }
};

}  // namespace gvl
