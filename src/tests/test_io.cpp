#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "io/coding.hpp"
#include "io/deflate.hpp"
#include "io/stream.hpp"

TEST_CASE("io::MemReader reads exact bytes", "[io]") {
  std::vector<uint8_t> data{0x01, 0x02, 0x03};
  io::MemReader r(data);
  REQUIRE(r.Get() == 0x01);
  REQUIRE(r.Get() == 0x02);
  REQUIRE(r.Get() == 0x03);
  REQUIRE_THROWS_AS(r.Get(), io::EndOfStream);
}

TEST_CASE("io::MemReader::try_get returns short count at EOF", "[io]") {
  std::vector<uint8_t> data{0x10, 0x20};
  io::MemReader r(data);
  uint8_t buf[4]{};
  REQUIRE(r.TryGet(buf, 4) == 2);
  REQUIRE(buf[0] == 0x10);
  REQUIRE(buf[1] == 0x20);
}

TEST_CASE("io::VectorWriter and StringWriter append correctly", "[io]") {
  std::vector<uint8_t> v;
  io::VectorWriter vw(v);
  vw.Put(0xAA);
  uint8_t more[] = {0xBB, 0xCC};
  vw.Put(more, 2);
  REQUIRE(v.size() == 3);
  REQUIRE(v[0] == 0xAA);
  REQUIRE(v[2] == 0xCC);

  std::string s;
  io::StringWriter sw(s);
  sw.Put('H');
  sw.Put(reinterpret_cast<uint8_t const*>("ello"), 4);
  REQUIRE(s == "Hello");
}

TEST_CASE("io::coding LE round-trip", "[io]") {
  std::vector<uint8_t> buf;
  {
    io::VectorWriter w(buf);
    io::WriteUint16Le(w, 0xBEEF);
    io::WriteUint32Le(w, 0xDEADBEEF);
  }
  io::MemReader r(buf);
  REQUIRE(io::ReadUint16Le(r) == 0xBEEF);
  REQUIRE(io::ReadUint32Le(r) == 0xDEADBEEF);
}

TEST_CASE("io::coding BE round-trip", "[io]") {
  std::vector<uint8_t> buf;
  {
    io::VectorWriter w(buf);
    io::WriteUint16(w, 0x1234);
    io::WriteUint32(w, 0x12345678);
  }
  REQUIRE(buf[0] == 0x12);
  REQUIRE(buf[1] == 0x34);
  REQUIRE(buf[2] == 0x12);
  io::MemReader r(buf);
  REQUIRE(io::ReadUint16(r) == 0x1234);
  REQUIRE(io::ReadUint32(r) == 0x12345678);
}

TEST_CASE("io::DeflateWriter / InflateReader round-trip random payload", "[io]") {
  std::vector<uint8_t> payload(64 * 1024);
  std::mt19937 rng(0xC0FFEE);
  for (auto& b : payload) b = static_cast<uint8_t>(rng());

  std::vector<uint8_t> compressed;
  {
    auto sink = std::make_unique<io::VectorWriter>(compressed);
    io::DeflateWriter dw(std::move(sink));
    dw.Put(payload.data(), payload.size());
  }
  REQUIRE(!compressed.empty());

  auto src = std::make_unique<io::MemReader>(compressed);
  io::InflateReader ir(std::move(src));
  std::vector<uint8_t> roundtrip(payload.size());
  REQUIRE(ir.TryGet(roundtrip.data(), roundtrip.size()) == roundtrip.size());
  REQUIRE(roundtrip == payload);
}

TEST_CASE("io::DeflateWriter / InflateReader round-trip empty payload", "[io]") {
  std::vector<uint8_t> compressed;
  {
    auto sink = std::make_unique<io::VectorWriter>(compressed);
    io::DeflateWriter dw(std::move(sink));
  }
  // Empty input still yields a valid zlib stream (header + final block).
  REQUIRE(!compressed.empty());

  auto src = std::make_unique<io::MemReader>(compressed);
  io::InflateReader ir(std::move(src));
  uint8_t buf[16];
  REQUIRE(ir.TryGet(buf, sizeof(buf)) == 0);
}

TEST_CASE("io::DeflateWriter compresses repetitive data well", "[io]") {
  std::vector<uint8_t> payload(8192, 0xAA);
  std::vector<uint8_t> compressed;
  {
    auto sink = std::make_unique<io::VectorWriter>(compressed);
    io::DeflateWriter dw(std::move(sink));
    dw.Put(payload.data(), payload.size());
  }
  // Highly repetitive data should compress at least 10x.
  REQUIRE(compressed.size() < payload.size() / 10);

  auto src = std::make_unique<io::MemReader>(compressed);
  io::InflateReader ir(std::move(src));
  std::vector<uint8_t> roundtrip(payload.size());
  REQUIRE(ir.TryGet(roundtrip.data(), roundtrip.size()) == roundtrip.size());
  REQUIRE(roundtrip == payload);
}
