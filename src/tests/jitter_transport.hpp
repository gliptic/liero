#pragma once

// In-process batched-input transport between two RollbackControllers.
// Models per-packet random delivery delay, packet loss, and optional
// duplication. Out-of-order arrival happens naturally because per-packet
// delays are picked from a uniform distribution independently.

#include <array>
#include <cstdint>
#include <functional>
#include <random>
#include <vector>

#include "rollback/buffer.hpp"

namespace rollback_test {

struct JitterTransport {
  struct Params {
    uint32_t seed = 0xC0FFEE;
    int min_delay_frames = 0;
    int max_delay_frames = 0;
    double loss_probability = 0.0;
    double duplicate_probability = 0.0;
  };

  static constexpr std::size_t kMaxBatch = rollback::kMaxRollback + 1;

  struct InFlight {
    int deliver_at_frame;
    uint8_t generation;
    uint32_t base_frame;
    uint8_t count;
    std::array<uint8_t, kMaxBatch> inputs;
    uint32_t local_frame;
  };

  // The on-wire generation byte travels through the transport so the
  // receive side can exercise the controller's stale-generation drop.
  // Tests that don't care about phase transitions just pass 0.
  using Deliver = std::function<void(uint8_t generation, uint32_t base_frame, uint8_t count,
                                     uint8_t const* inputs, uint32_t local_frame)>;

  Params params;
  std::mt19937 rng;
  std::vector<InFlight> a_to_b;
  std::vector<InFlight> b_to_a;
  int current_frame = 0;

  uint64_t packets_sent = 0;
  uint64_t packets_dropped = 0;
  uint64_t packets_duplicated = 0;

  explicit JitterTransport(Params p) : params(p), rng(p.seed) {}

  int RandomDelay() {
    int const kLo = params.min_delay_frames;
    int const kHi = params.max_delay_frames;
    if (kHi <= kLo) {
      return kLo;
    }
    std::uniform_int_distribution<int> d(kLo, kHi);
    return d(rng);
  }

  bool Roll(double p) {
    if (p <= 0.0) {
      return false;
    }
    std::uniform_real_distribution<double> d(0.0, 1.0);
    return d(rng) < p;
  }

  void SendAToB(uint8_t generation, uint32_t base_frame, uint8_t count, uint8_t const* inputs,
                uint32_t local_frame) {
    Enqueue(a_to_b, generation, base_frame, count, inputs, local_frame);
  }
  void SendBToA(uint8_t generation, uint32_t base_frame, uint8_t count, uint8_t const* inputs,
                uint32_t local_frame) {
    Enqueue(b_to_a, generation, base_frame, count, inputs, local_frame);
  }

  // Drain anything whose deliverAtFrame has elapsed, then advance the
  // clock. Within a queue, packets scan in send order — an earlier-sent
  // packet may carry a later deliverAtFrame than a packet queued after
  // it, exactly modelling jittery out-of-order delivery.
  void Tick(Deliver const& deliver_a, Deliver const& deliver_b) {
    DrainDue(a_to_b, deliver_b);
    DrainDue(b_to_a, deliver_a);
    ++current_frame;
  }

  // Force-deliver every in-flight packet. Used at the end of a test to
  // converge both peers regardless of how late tail packets were.
  void Flush(Deliver const& deliver_a, Deliver const& deliver_b) {
    for (auto const& p : a_to_b) {
      deliver_b(p.generation, p.base_frame, p.count, p.inputs.data(), p.local_frame);
    }
    for (auto const& p : b_to_a) {
      deliver_a(p.generation, p.base_frame, p.count, p.inputs.data(), p.local_frame);
    }
    a_to_b.clear();
    b_to_a.clear();
  }

  bool Empty() const { return a_to_b.empty() && b_to_a.empty(); }

 private:
  void Enqueue(std::vector<InFlight>& q, uint8_t generation, uint32_t base_frame, uint8_t count,
               uint8_t const* inputs, uint32_t local_frame) {
    ++packets_sent;
    if (Roll(params.loss_probability)) {
      ++packets_dropped;
      return;
    }
    InFlight p{};
    p.deliver_at_frame = current_frame + RandomDelay();
    p.generation = generation;
    p.base_frame = base_frame;
    p.count = count;
    for (uint8_t i = 0; i < count; ++i) {
      p.inputs[i] = inputs[i];
    }
    p.local_frame = local_frame;
    q.push_back(p);
    if (Roll(params.duplicate_probability)) {
      ++packets_duplicated;
      InFlight d = p;
      d.deliver_at_frame = current_frame + RandomDelay();
      q.push_back(d);
    }
  }

  void DrainDue(std::vector<InFlight>& q, Deliver const& deliver) const {
    auto it = q.begin();
    while (it != q.end()) {
      if (it->deliver_at_frame <= current_frame) {
        deliver(it->generation, it->base_frame, it->count, it->inputs.data(), it->local_frame);
        it = q.erase(it);
      } else {
        ++it;
      }
    }
  }
};

}  // namespace rollback_test
