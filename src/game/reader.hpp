#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#include "io/stream.hpp"

// Random-access in-memory file: slurp an io::Reader once, then use
// seekg/tellg/skip/get/get(buf, n). Used by the TC asset loaders for
// sprite/sound/font files.
struct ReaderFile {
  ReaderFile() = default;

  ReaderFile(ReaderFile const&) = delete;
  ReaderFile& operator=(ReaderFile const&) = delete;

  ReaderFile(ReaderFile&& other) noexcept : data_(std::move(other.data_)), pos(other.pos) {
    other.pos = 0;
  }

  explicit ReaderFile(io::Reader& r) {
    uint8_t buf[4096];
    for (;;) {
      std::size_t got = r.try_get(buf, sizeof(buf));
      if (got == 0) break;
      data_.insert(data_.end(), buf, buf + got);
    }
  }

  uint8_t* data() { return data_.data(); }
  uint8_t const* data() const { return data_.data(); }
  std::size_t len() const { return data_.size(); }

  std::size_t pos = 0;

  void seekg(std::size_t newPos) {
    if (newPos > data_.size()) throw io::EndOfStream("EOF in seekg()");
    pos = newPos;
  }

  std::size_t tellg() const { return pos; }

  void skip(std::size_t step) { seekg(pos + step); }

  uint8_t get() {
    if (pos >= data_.size()) throw io::EndOfStream("EOF in get()");
    return data_[pos++];
  }

  void get(uint8_t* p, std::size_t l) {
    if (pos + l > data_.size()) throw io::EndOfStream("EOF in get()");
    std::memcpy(p, data_.data() + pos, l);
    pos += l;
  }

 private:
  std::vector<uint8_t> data_;
};
