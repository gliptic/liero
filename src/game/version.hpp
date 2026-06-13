#pragma once

// Version 7: palette channels stored as 8-bit (older replays carry 6-bit
// VGA values that are expanded on load).
// Version 8: level display layer (display_data/display_valid) added.
// Version 9: level anim layer (argb_ramps/display_anim) added.
static int const kMyReplayVersion = 9;
