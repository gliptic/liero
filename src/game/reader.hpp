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
      std::size_t got = r.TryGet(buf, sizeof(buf));
      if (got == 0) break;
      data_.insert(data_.end(), buf, buf + got);
    }
  }

  uint8_t* Data() { return data_.data(); }
  uint8_t const* Data() const { return data_.data(); }
  std::size_t Len() const { return data_.size(); }

  std::size_t pos = 0;

  void Seekg(std::size_t new_pos) {
    if (new_pos > data_.size()) throw io::EndOfStream("EOF in seekg()");
    pos = new_pos;
  }

  std::size_t Tellg() const { return pos; }

  void Skip(std::size_t step) { Seekg(pos + step); }

  uint8_t Get() {
    if (pos >= data_.size()) throw io::EndOfStream("EOF in get()");
    return data_[pos++];
  }

  void Get(uint8_t* p, std::size_t l) {
    if (pos + l > data_.size()) throw io::EndOfStream("EOF in get()");
    std::memcpy(p, data_.data() + pos, l);
    pos += l;
  }

 private:
  std::vector<uint8_t> data_;
};
