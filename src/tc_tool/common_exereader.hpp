#pragma once

#include "game/common.hpp"

struct ReaderFile;

void LoadFromExe(Common& common, ReaderFile& exe, ReaderFile& gfx, ReaderFile& snd);
void LoadSfx(std::vector<SfxSample>& sounds, ReaderFile& snd);
