#pragma once

// Thin wrapper around Tracy. When OPENLIERO_ENABLE_TRACY is defined (set by
// CMake when -DOPENLIERO_ENABLE_TRACY=ON is passed), real Tracy macros are
// used. Otherwise every macro expands to nothing so instrumented source
// compiles without any Tracy headers on the include path.

#ifdef OPENLIERO_ENABLE_TRACY
#include <tracy/Tracy.hpp>
#else
// These no-op macros match Tracy's mixed-case API names exactly so instrumented
// call sites compile identically with and without Tracy.
// NOLINTBEGIN(readability-identifier-naming)
#define ZoneScoped
#define ZoneScopedN(name)
#define FrameMark
#define FrameMarkNamed(name)
// NOLINTEND(readability-identifier-naming)
#endif
