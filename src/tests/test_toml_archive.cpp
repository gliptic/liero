// Phase 2 tests: cereal::TomlOutputArchive / TomlInputArchive in isolation.
// Cover primitives, nested objects, arrays of primitives, arrays of objects,
// missing-key tolerance, and version-tagged types.

#include <catch2/catch_test_macros.hpp>

#include "game/serialization/toml_archive.hpp"

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Primitives {
  std::int32_t i = 0;
  std::uint32_t u = 0;
  bool b = false;
  std::string s;
  double d = 0.0;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(i), CEREAL_NVP(u), CEREAL_NVP(b), CEREAL_NVP(s),
       CEREAL_NVP(d));
  }

  bool operator==(Primitives const& o) const {
    return i == o.i && u == o.u && b == o.b && s == o.s && d == o.d;
  }
};

struct Inner {
  std::int32_t x = 0;
  std::int32_t y = 0;
  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(x), CEREAL_NVP(y));
  }
  bool operator==(Inner const& o) const { return x == o.x && y == o.y; }
};

struct Outer {
  Inner inner;
  std::int32_t flag = 0;
  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(inner), CEREAL_NVP(flag));
  }
  bool operator==(Outer const& o) const {
    return inner == o.inner && flag == o.flag;
  }
};

struct ArrayHolder {
  std::vector<std::int32_t> ints;
  std::vector<std::string> strs;
  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(ints), CEREAL_NVP(strs));
  }
  bool operator==(ArrayHolder const& o) const {
    return ints == o.ints && strs == o.strs;
  }
};

struct ObjectArrayHolder {
  std::vector<Inner> items;
  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(items));
  }
  bool operator==(ObjectArrayHolder const& o) const {
    return items == o.items;
  }
};

struct Versioned {
  std::int32_t a = 0;
  std::int32_t b = 0;  // added in v2

  template <class Archive>
  void serialize(Archive& ar, std::uint32_t const version) {
    ar(CEREAL_NVP(a));
    if (version >= 2)
      ar(CEREAL_NVP(b));
  }
};

}  // namespace

CEREAL_CLASS_VERSION(Versioned, 2);

TEST_CASE("TomlArchive: primitive round-trip", "[toml_archive]") {
  Primitives src{-42, 7u, true, "hello world", 3.5};
  std::stringstream ss;
  {
    cereal::TomlOutputArchive out(ss);
    out(cereal::make_nvp("p", src));
  }
  Primitives dst;
  {
    cereal::TomlInputArchive in(ss);
    in(cereal::make_nvp("p", dst));
  }
  CHECK(dst == src);
}

TEST_CASE("TomlArchive: nested object round-trip", "[toml_archive]") {
  Outer src;
  src.inner = {11, 22};
  src.flag = 99;
  std::stringstream ss;
  {
    cereal::TomlOutputArchive out(ss);
    out(cereal::make_nvp("outer", src));
  }
  Outer dst;
  {
    cereal::TomlInputArchive in(ss);
    in(cereal::make_nvp("outer", dst));
  }
  CHECK(dst == src);
}

TEST_CASE("TomlArchive: arrays of primitives", "[toml_archive]") {
  ArrayHolder src;
  src.ints = {1, 2, 3, -4, 5};
  src.strs = {"a", "bb", "ccc"};
  std::stringstream ss;
  {
    cereal::TomlOutputArchive out(ss);
    out(cereal::make_nvp("h", src));
  }
  ArrayHolder dst;
  {
    cereal::TomlInputArchive in(ss);
    in(cereal::make_nvp("h", dst));
  }
  CHECK(dst == src);
}

TEST_CASE("TomlArchive: arrays of objects", "[toml_archive]") {
  ObjectArrayHolder src;
  src.items = {{1, 2}, {3, 4}, {5, 6}};
  std::stringstream ss;
  {
    cereal::TomlOutputArchive out(ss);
    out(cereal::make_nvp("h", src));
  }
  ObjectArrayHolder dst;
  {
    cereal::TomlInputArchive in(ss);
    in(cereal::make_nvp("h", dst));
  }
  CHECK(dst == src);
}

TEST_CASE("TomlArchive: missing key keeps default", "[toml_archive]") {
  // Manually-authored TOML with only one of the Primitives fields.
  std::string toml_text = "[p]\ni = 17\n";
  std::stringstream ss(toml_text);
  Primitives dst{0, 99u, true, "preserved", 1.25};
  cereal::TomlInputArchive in(ss);
  in(cereal::make_nvp("p", dst));
  CHECK(dst.i == 17);
  CHECK(dst.u == 99u);
  CHECK(dst.b == true);
  CHECK(dst.s == "preserved");
  CHECK(dst.d == 1.25);
}

TEST_CASE("TomlArchive: versioned type emits and reads version",
          "[toml_archive]") {
  Versioned src{10, 20};
  std::stringstream ss;
  {
    cereal::TomlOutputArchive out(ss);
    out(cereal::make_nvp("v", src));
  }
  // The serialized text should contain the cereal_class_version field.
  CHECK(ss.str().find("cereal_class_version") != std::string::npos);

  Versioned dst;
  {
    cereal::TomlInputArchive in(ss);
    in(cereal::make_nvp("v", dst));
  }
  CHECK(dst.a == 10);
  CHECK(dst.b == 20);
}
