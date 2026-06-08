#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Minimal stream layer: polymorphic Reader / Writer base classes with
// concrete implementations for files, memory buffers, and (in deflate.hpp)
// zlib-via-miniz streams.

namespace io {

struct EndOfStream : std::runtime_error {
  EndOfStream() : std::runtime_error("end of stream") {}
  explicit EndOfStream(char const* what) : std::runtime_error(what) {}
};

struct StreamError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct ArchiveCheckError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct Reader {
  virtual ~Reader() = default;

  // Read one byte; throw EndOfStream on EOF.
  virtual uint8_t Get() = 0;

  // Read up to `n` bytes; return number actually read.
  virtual std::size_t TryGet(uint8_t* dst, std::size_t n) = 0;

  // Read exactly `n` bytes or throw EndOfStream.
  void Get(uint8_t* dst, std::size_t n) {
    std::size_t const kGot = TryGet(dst, n);
    if (kGot != n) throw EndOfStream{};
  }

  // Discard up to `n` bytes; return number actually discarded.
  virtual std::size_t TrySkip(std::size_t n) {
    uint8_t buf[1024];
    std::size_t total = 0;
    while (total < n) {
      std::size_t const kTake = std::min(sizeof(buf), n - total);
      std::size_t const kGot = TryGet(buf, kTake);
      total += kGot;
      if (kGot < kTake) break;
    }
    return total;
  }
};

struct Writer {
  virtual ~Writer() = default;

  virtual void Put(uint8_t b) = 0;
  virtual void Put(uint8_t const* src, std::size_t n) = 0;
  virtual void Flush() {}
};

// ---- File-backed ----

struct FileReader : Reader {
  // Borrows the FILE* — caller retains ownership and is responsible
  // for closing it. Useful for stdin or pre-opened file descriptors.
  explicit FileReader(std::FILE* f) : f_(f), owned_(false) {}

  // Tag for the take-ownership flavour of the FILE* constructor.
  struct OwnFile {};
  FileReader(std::FILE* f, OwnFile /*unused*/) : f_(f), owned_(true) {}

  FileReader(char const* path, char const* mode) : f_(std::fopen(path, mode)) {
    if (!f_) throw StreamError(std::string("Couldn't open ") + path);
    owned_ = true;
  }
  ~FileReader() override {
    if (owned_ && f_)
      std::fclose(f_);  // NOLINT(cert-err33-c) — destructor cleanup; cannot signal a failure here.
  }
  FileReader(FileReader const&) = delete;
  FileReader& operator=(FileReader const&) = delete;

  uint8_t Get() override {
    int const kC = std::fgetc(f_);
    if (kC == EOF) throw EndOfStream{};
    return static_cast<uint8_t>(kC);
  }

  std::size_t TryGet(uint8_t* dst, std::size_t n) override { return std::fread(dst, 1, n, f_); }

 private:
  std::FILE* f_;
  bool owned_;
};

struct FileWriter : Writer {
  explicit FileWriter(std::FILE* f) : f_(f), owned_(false) {}

  struct OwnFile {};
  FileWriter(std::FILE* f, OwnFile /*unused*/) : f_(f), owned_(true) {}

  FileWriter(char const* path, char const* mode) : f_(std::fopen(path, mode)) {
    if (!f_) throw StreamError(std::string("Couldn't open ") + path);
    owned_ = true;
  }
  ~FileWriter() override {
    if (owned_ && f_) {
      std::fflush(f_);  // NOLINT(cert-err33-c) — destructor cleanup; cannot signal a failure here.
      std::fclose(f_);  // NOLINT(cert-err33-c) — destructor cleanup; cannot signal a failure here.
    }
  }
  FileWriter(FileWriter const&) = delete;
  FileWriter& operator=(FileWriter const&) = delete;

  void Put(uint8_t b) override {
    if (std::fputc(b, f_) == EOF) throw StreamError("write failed");
  }
  void Put(uint8_t const* src, std::size_t n) override {
    if (std::fwrite(src, 1, n, f_) != n) throw StreamError("write failed");
  }
  void Flush() override {
    std::fflush(f_);  // NOLINT(cert-err33-c) — best-effort; surfaces on next IO.
  }

 private:
  std::FILE* f_;
  bool owned_;
};

// ---- Memory-backed ----

struct MemReader : Reader {
  MemReader() = default;
  MemReader(uint8_t const* data, std::size_t size) : data_(data), size_(size) {}
  explicit MemReader(std::string const& s)
      : data_(reinterpret_cast<uint8_t const*>(s.data())), size_(s.size()) {}
  explicit MemReader(std::vector<uint8_t> const& v) : data_(v.data()), size_(v.size()) {}

  // Point at a different buffer (e.g. once it has been filled by an
  // owning container).
  void Reset(uint8_t const* data, std::size_t size) {
    data_ = data;
    size_ = size;
    pos_ = 0;
  }

  std::size_t Tellg() const { return pos_; }
  void Seekg(std::size_t pos) {
    if (pos > size_) throw EndOfStream("seekg past end");
    pos_ = pos;
  }

  uint8_t Get() override {
    if (pos_ >= size_) throw EndOfStream{};
    return data_[pos_++];
  }

  std::size_t TryGet(uint8_t* dst, std::size_t n) override {
    std::size_t const kAvail = size_ - pos_;
    std::size_t const kTake = std::min(n, kAvail);
    std::memcpy(dst, data_ + pos_, kTake);
    pos_ += kTake;
    return kTake;
  }

 private:
  uint8_t const* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t pos_ = 0;
};

struct VectorWriter : Writer {
  std::vector<uint8_t>& buf;
  explicit VectorWriter(std::vector<uint8_t>& b) : buf(b) {}

  void Put(uint8_t b) override { buf.push_back(b); }
  void Put(uint8_t const* src, std::size_t n) override { buf.insert(buf.end(), src, src + n); }
};

struct StringWriter : Writer {
  std::string& buf;
  explicit StringWriter(std::string& b) : buf(b) {}

  void Put(uint8_t b) override { buf.push_back(static_cast<char>(b)); }
  void Put(uint8_t const* src, std::size_t n) override {
    buf.append(reinterpret_cast<char const*>(src), n);
  }
};

}  // namespace io
