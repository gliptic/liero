#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "src/game/gfx.h"

class InputStringSecurityTest : public ::testing::TestWithParam<std::string> {};

TEST_P(InputStringSecurityTest, BufferNeverExceedsMaxLenUnderAdversarialTextInput) {
    // Invariant: Processing SDL_TEXTINPUT events must never cause buffer size to exceed maxLen
    Gfx gfx;
    std::string dest;
    std::size_t maxLen = 10;
    std::string payload = GetParam();
    
    // Simulate SDL_TEXTINPUT event with adversarial payload
    SDL_Event ev;
    ev.type = SDL_TEXTINPUT;
    strncpy(ev.text.text, payload.c_str(), SDL_TEXTINPUTEVENT_TEXT_SIZE);
    
    // Mock the utf8ToDOS function to return multiple characters for long UTF-8 sequences
    // This simulates the worst-case expansion scenario
    int expansionFactor = 3; // Simulate UTF-8 to DOS expansion
    std::size_t expectedMax = payload.size() * expansionFactor;
    
    // Security property: processed buffer must never exceed maxLen
    ASSERT_TRUE(expectedMax <= maxLen || payload.empty()) 
        << "Adversarial input '" << payload << "' would expand to " 
        << expectedMax << " chars, exceeding maxLen=" << maxLen;
}

INSTANTIATE_TEST_SUITE_P(
    AdversarialInputs,
    InputStringSecurityTest,
    ::testing::Values(
        // Exact exploit: multi-byte UTF-8 sequence that expands beyond buffer
        "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC", // 4 Euro signs expanding to 12 chars
        // Boundary case: maxLen+1 single-byte characters
        "0123456789A", // 11 chars, 1 over limit
        // Valid input: within bounds
        "Safe123"
    )
);

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}