#ifndef REPLAY_TO_VIDEO_HPP
#define REPLAY_TO_VIDEO_HPP

#include <string>
#include "game/common.hpp"

void replayToVideo(
	gvl::shared_ptr<Common> const& common,
    bool spectator,
	std::string const& fullPath,
	std::string const& replayVideoName);

#endif // REPLAY_TO_VIDEO_HPP
