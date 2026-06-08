#pragma once

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

#include "io/stream.hpp"

// Streaming deflate / inflate filters over an underlying io::Reader / Writer.
// Bytes are buffered in chunks; the caller pulls or pushes uncompressed data
// and the wrapper takes care of feeding / draining the miniz state.

namespace io {

namespace detail {
constexpr std::size_t kDeflateBufSize = 65536;
}  // namespace detail

struct InflateReader : Reader {
  explicit InflateReader(std::unique_ptr<Reader> source)
      : source_(std::move(source)),
        inbuf_(detail::kDeflateBufSize),
        outbuf_(detail::kDeflateBufSize) {
    stream_.zalloc = nullptr;
    stream_.zfree = nullptr;
    stream_.opaque = nullptr;
    stream_.next_in = nullptr;
    stream_.avail_in = 0;
    stream_.next_out = nullptr;
    stream_.avail_out = 0;
    if (mz_inflateInit(&stream_) != MZ_OK) {
      throw StreamError("mz_inflateInit failed");
    }
  }

  ~InflateReader() override { mz_inflateEnd(&stream_); }

  InflateReader(InflateReader const&) = delete;
  InflateReader& operator=(InflateReader const&) = delete;

  uint8_t Get() override {
    if (out_pos_ >= out_len_) {
      Refill();
      if (out_len_ == 0) {
        throw EndOfStream{};
      }
    }
    return outbuf_[out_pos_++];
  }

  std::size_t TryGet(uint8_t* dst, std::size_t n) override {
    std::size_t total = 0;
    while (total < n) {
      if (out_pos_ >= out_len_) {
        Refill();
        if (out_len_ == 0) {
          break;
        }
      }
      std::size_t const kTake = std::min(n - total, out_len_ - out_pos_);
      std::memcpy(dst + total, outbuf_.data() + out_pos_, kTake);
      out_pos_ += kTake;
      total += kTake;
    }
    return total;
  }

 private:
  void Refill() {
    out_pos_ = 0;
    out_len_ = 0;
    if (eos_) {
      return;
    }

    stream_.next_out = outbuf_.data();
    stream_.avail_out = static_cast<unsigned int>(outbuf_.size());

    while (stream_.avail_out > 0) {
      if (stream_.avail_in == 0 && !input_done_) {
        std::size_t const kGot = source_->TryGet(inbuf_.data(), inbuf_.size());
        if (kGot == 0) {
          input_done_ = true;
        } else {
          stream_.next_in = inbuf_.data();
          stream_.avail_in = static_cast<unsigned int>(kGot);
        }
      }

      int const kFlush = input_done_ ? MZ_FINISH : MZ_NO_FLUSH;
      int const kRc = mz_inflate(&stream_, kFlush);

      if (kRc == MZ_STREAM_END) {
        eos_ = true;
        break;
      }
      if (kRc == MZ_BUF_ERROR) {
        // No progress; need more input or output buffer is full.
        if (stream_.avail_out == 0) {
          break;
        }
        if (input_done_) {
          eos_ = true;
          break;
        }
        // Otherwise loop and try to read more input.
        continue;
      }
      if (kRc != MZ_OK) {
        throw StreamError("mz_inflate failed");
      }
    }

    out_len_ = outbuf_.size() - stream_.avail_out;
  }

  std::unique_ptr<Reader> source_;
  mz_stream stream_{};
  std::vector<uint8_t> inbuf_;
  std::vector<uint8_t> outbuf_;
  std::size_t out_pos_ = 0;
  std::size_t out_len_ = 0;
  bool input_done_ = false;
  bool eos_ = false;
};

struct DeflateWriter : Writer {
  explicit DeflateWriter(std::unique_ptr<Writer> sink)
      : sink_(std::move(sink)), outbuf_(detail::kDeflateBufSize) {
    stream_.zalloc = nullptr;
    stream_.zfree = nullptr;
    stream_.opaque = nullptr;
    stream_.next_in = nullptr;
    stream_.avail_in = 0;
    stream_.next_out = outbuf_.data();
    stream_.avail_out = static_cast<unsigned int>(outbuf_.size());
    if (mz_deflateInit(&stream_, MZ_DEFAULT_COMPRESSION) != MZ_OK) {
      throw StreamError("mz_deflateInit failed");
    }
  }

  ~DeflateWriter() override {
    try {
      Finish();
    } catch (...) {  // NOLINT(bugprone-empty-catch) — destructor must not throw; the best-effort
                     // flush above is the entire intent.
      // Best-effort during stack unwind.
    }
    mz_deflateEnd(&stream_);
  }

  DeflateWriter(DeflateWriter const&) = delete;
  DeflateWriter& operator=(DeflateWriter const&) = delete;

  void Put(uint8_t b) override { Put(&b, 1); }

  void Put(uint8_t const* src, std::size_t n) override {
    if (finished_) {
      throw StreamError("write after finish");
    }

    stream_.next_in = const_cast<unsigned char*>(src);
    stream_.avail_in = static_cast<unsigned int>(n);

    while (stream_.avail_in > 0) {
      if (stream_.avail_out == 0) {
        DrainOut();
      }
      int const kRc = mz_deflate(&stream_, MZ_NO_FLUSH);
      if (kRc != MZ_OK) {
        throw StreamError("mz_deflate failed");
      }
    }
  }

  void Flush() override {
    // Pre-emptive drain — full flush happens on destruction via finish().
    DrainOut();
    sink_->Flush();
  }

 private:
  void DrainOut() {
    std::size_t const kProduced = outbuf_.size() - stream_.avail_out;
    if (kProduced > 0) {
      sink_->Put(outbuf_.data(), kProduced);
    }
    stream_.next_out = outbuf_.data();
    stream_.avail_out = static_cast<unsigned int>(outbuf_.size());
  }

  void Finish() {
    if (finished_) {
      return;
    }
    finished_ = true;

    stream_.next_in = nullptr;
    stream_.avail_in = 0;

    for (;;) {
      int const kRc = mz_deflate(&stream_, MZ_FINISH);
      DrainOut();
      if (kRc == MZ_STREAM_END) {
        break;
      }
      if (kRc != MZ_OK) {
        throw StreamError("mz_deflate(MZ_FINISH) failed");
      }
    }
    sink_->Flush();
  }

  std::unique_ptr<Writer> sink_;
  mz_stream stream_{};
  std::vector<uint8_t> outbuf_;
  bool finished_ = false;
};

}  // namespace io
