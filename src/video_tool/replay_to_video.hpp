#pragma once

#include <memory>
#include <string>
#include "game/common.hpp"

void ReplayToVideo(std::shared_ptr<Common> const& common, bool spectator,
                   std::string const& full_path, std::string const& replay_video_name);
