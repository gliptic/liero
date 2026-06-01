#pragma once

#include <memory>
#include <string>
#include "game/common.hpp"

void replayToVideo(std::shared_ptr<Common> const& common, bool spectator,
                   std::string const& fullPath, std::string const& replayVideoName);
