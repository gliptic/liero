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
}

struct InflateReader : Reader {
	explicit InflateReader(std::unique_ptr<Reader> source)
		: source_(std::move(source))
		, inbuf_(detail::kDeflateBufSize)
		, outbuf_(detail::kDeflateBufSize)
	{
		stream_.zalloc = nullptr;
		stream_.zfree = nullptr;
		stream_.opaque = nullptr;
		stream_.next_in = nullptr;
		stream_.avail_in = 0;
		stream_.next_out = nullptr;
		stream_.avail_out = 0;
		if (mz_inflateInit(&stream_) != MZ_OK)
			throw StreamError("mz_inflateInit failed");
	}

	~InflateReader() override {
		mz_inflateEnd(&stream_);
	}

	InflateReader(InflateReader const&) = delete;
	InflateReader& operator=(InflateReader const&) = delete;

	uint8_t get() override {
		if (out_pos_ >= out_len_) {
			refill();
			if (out_len_ == 0)
				throw EndOfStream{};
		}
		return outbuf_[out_pos_++];
	}

	std::size_t try_get(uint8_t* dst, std::size_t n) override {
		std::size_t total = 0;
		while (total < n) {
			if (out_pos_ >= out_len_) {
				refill();
				if (out_len_ == 0)
					break;
			}
			std::size_t take = std::min(n - total, out_len_ - out_pos_);
			std::memcpy(dst + total, outbuf_.data() + out_pos_, take);
			out_pos_ += take;
			total += take;
		}
		return total;
	}

private:
	void refill() {
		out_pos_ = 0;
		out_len_ = 0;
		if (eos_)
			return;

		stream_.next_out = outbuf_.data();
		stream_.avail_out = static_cast<unsigned int>(outbuf_.size());

		while (stream_.avail_out > 0) {
			if (stream_.avail_in == 0 && !input_done_) {
				std::size_t got = source_->try_get(inbuf_.data(), inbuf_.size());
				if (got == 0) {
					input_done_ = true;
				} else {
					stream_.next_in = inbuf_.data();
					stream_.avail_in = static_cast<unsigned int>(got);
				}
			}

			int flush = input_done_ ? MZ_FINISH : MZ_NO_FLUSH;
			int rc = mz_inflate(&stream_, flush);

			if (rc == MZ_STREAM_END) {
				eos_ = true;
				break;
			}
			if (rc == MZ_BUF_ERROR) {
				// No progress; need more input or output buffer is full.
				if (stream_.avail_out == 0)
					break;
				if (input_done_) {
					eos_ = true;
					break;
				}
				// Otherwise loop and try to read more input.
				continue;
			}
			if (rc != MZ_OK)
				throw StreamError("mz_inflate failed");
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
		: sink_(std::move(sink))
		, outbuf_(detail::kDeflateBufSize)
	{
		stream_.zalloc = nullptr;
		stream_.zfree = nullptr;
		stream_.opaque = nullptr;
		stream_.next_in = nullptr;
		stream_.avail_in = 0;
		stream_.next_out = outbuf_.data();
		stream_.avail_out = static_cast<unsigned int>(outbuf_.size());
		if (mz_deflateInit(&stream_, MZ_DEFAULT_COMPRESSION) != MZ_OK)
			throw StreamError("mz_deflateInit failed");
	}

	~DeflateWriter() override {
		try {
			finish();
		} catch (...) {
			// Best-effort during stack unwind.
		}
		mz_deflateEnd(&stream_);
	}

	DeflateWriter(DeflateWriter const&) = delete;
	DeflateWriter& operator=(DeflateWriter const&) = delete;

	void put(uint8_t b) override {
		put(&b, 1);
	}

	void put(uint8_t const* src, std::size_t n) override {
		if (finished_)
			throw StreamError("write after finish");

		stream_.next_in = const_cast<unsigned char*>(src);
		stream_.avail_in = static_cast<unsigned int>(n);

		while (stream_.avail_in > 0) {
			if (stream_.avail_out == 0)
				drain_out();
			int rc = mz_deflate(&stream_, MZ_NO_FLUSH);
			if (rc != MZ_OK)
				throw StreamError("mz_deflate failed");
		}
	}

	void flush() override {
		// Pre-emptive drain — full flush happens on destruction via finish().
		drain_out();
		sink_->flush();
	}

private:
	void drain_out() {
		std::size_t produced = outbuf_.size() - stream_.avail_out;
		if (produced > 0) {
			sink_->put(outbuf_.data(), produced);
		}
		stream_.next_out = outbuf_.data();
		stream_.avail_out = static_cast<unsigned int>(outbuf_.size());
	}

	void finish() {
		if (finished_)
			return;
		finished_ = true;

		stream_.next_in = nullptr;
		stream_.avail_in = 0;

		for (;;) {
			int rc = mz_deflate(&stream_, MZ_FINISH);
			drain_out();
			if (rc == MZ_STREAM_END)
				break;
			if (rc != MZ_OK)
				throw StreamError("mz_deflate(MZ_FINISH) failed");
		}
		sink_->flush();
	}

	std::unique_ptr<Writer> sink_;
	mz_stream stream_{};
	std::vector<uint8_t> outbuf_;
	bool finished_ = false;
};

}  // namespace io
