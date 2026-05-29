#pragma once

#include "game/common.hpp"

struct ReaderFile;

void loadFromExe(Common& common, ReaderFile& exe, ReaderFile& gfx, ReaderFile& snd);
void loadSfx(std::vector<SfxSample>& sounds, ReaderFile& snd);
