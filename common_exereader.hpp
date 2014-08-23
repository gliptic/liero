#ifndef LIERO_COMMON_EXEREADER_HPP
#define LIERO_COMMON_EXEREADER_HPP

#include "common.hpp"

void loadFromExe(Common& common, std::string const& lieroExe);
void loadSfx(std::vector<sfx_sound*>& sounds, ReaderFile& snd);

#endif // LIERO_COMMON_EXEREADER_HPP
