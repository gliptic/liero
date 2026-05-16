#include "inputState.hpp"

#include "gfx.hpp"
#include "sfx.hpp"
#include "text.hpp"
#include "keys.hpp"
#include "common.hpp"

#include <SDL3/SDL.h>

// --- InputStringState ---

InputStringState::InputStringState(std::string initial, std::size_t maxLen, int x, int y,
	int (*filter)(int), std::string prefix, bool centered, Callback callback)
: buffer_(std::move(initial))
, maxLen_(maxLen)
, x_(x)
, y_(y)
, filter_(filter)
, prefix_(std::move(prefix))
, centered_(centered)
, callback_(std::move(callback))
{
}

void InputStringState::enter()
{
	SDL_StartTextInput(gfx->sdlWindow);
}

void InputStringState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev);

	switch (ev.type)
	{
		case SDL_EVENT_KEY_DOWN:
			switch (ev.key.scancode)
			{
				case SDL_SCANCODE_BACKSPACE:
					if (!buffer_.empty())
						buffer_.erase(buffer_.size() - 1);
					break;

				case SDL_SCANCODE_RETURN:
				case SDL_SCANCODE_KP_ENTER:
					accepted_ = true;
					done_ = true;
					break;

				case SDL_SCANCODE_ESCAPE:
					accepted_ = false;
					done_ = true;
					break;

				default:
					break;
			}
			break;

		case SDL_EVENT_TEXT_INPUT:
		{
			int k = utf8ToDOS(ev.text.text);
			if (k && buffer_.size() < maxLen_ &&
				(!filter_ || (k = filter_(k))))
			{
				buffer_ += char(k);
			}
			break;
		}

		case SDL_EVENT_TEXT_EDITING:
			// No complex IME support
			break;

		default:
			break;
	}
}

bool InputStringState::update()
{
	if (done_)
	{
		SDL_StopTextInput(gfx->sdlWindow);
		sfx.play(*gfx->common, 27);
		gfx->clearKeys();
		callback_(accepted_, buffer_);
		return false;
	}
	return true;
}

void InputStringState::draw()
{
	std::string str = prefix_ + buffer_ + '_';

	Font& font = gfx->common->font;
	int width = font.getDims(str);
	int adjust = centered_ ? width / 2 : 0;
	int clrX = x_ - 10 - adjust;

	blitImageNoKeyColour(gfx->playRenderer.bmp,
		&gfx->frozenScreen.getPixel(clrX, y_),
		clrX, y_, clrX + 10 + width, 8, gfx->frozenScreen.pitch);

	drawRoundedBox(gfx->playRenderer.bmp, x_ - 2 - adjust, y_, 0, 7, width);
	font.drawText(gfx->playRenderer.bmp, str, x_ - adjust, y_ + 1, 50);
}

// --- WaitForKeyState ---

WaitForKeyState::WaitForKeyState(bool extended, Callback callback)
: extended_(extended)
, callback_(std::move(callback))
{
}

void WaitForKeyState::enter()
{
}

void WaitForKeyState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev);

	switch (ev.type)
	{
		case SDL_EVENT_KEY_DOWN:
		{
			uint32_t k = SDLToDOSKey(ev.key.scancode);
			if (extended_ || !isExtendedKey(k))
			{
				result_ = k;
				done_ = true;
			}
			break;
		}

		case SDL_EVENT_JOYSTICK_AXIS_MOTION:
			if (ev.jaxis.value > JoyAxisThreshold)
			{
				result_ = joyButtonToExKey(ev.jaxis.which, 4 + 2 * ev.jaxis.axis);
				done_ = true;
			}
			else if (ev.jaxis.value < -JoyAxisThreshold)
			{
				result_ = joyButtonToExKey(ev.jaxis.which, 5 + 2 * ev.jaxis.axis);
				done_ = true;
			}
			break;

		case SDL_EVENT_JOYSTICK_HAT_MOTION:
			if (ev.jhat.value & SDL_HAT_UP)
				result_ = joyButtonToExKey(ev.jhat.which, 0);
			else if (ev.jhat.value & SDL_HAT_DOWN)
				result_ = joyButtonToExKey(ev.jhat.which, 1);
			else if (ev.jhat.value & SDL_HAT_LEFT)
				result_ = joyButtonToExKey(ev.jhat.which, 2);
			else if (ev.jhat.value & SDL_HAT_RIGHT)
				result_ = joyButtonToExKey(ev.jhat.which, 3);
			else
				break;
			done_ = true;
			break;

		case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
			result_ = joyButtonToExKey(ev.jbutton.which, 16 + ev.jbutton.button);
			done_ = true;
			break;

		default:
			break;
	}
}

bool WaitForKeyState::update()
{
	if (done_)
	{
		gfx->clearKeys();
		callback_(result_);
		return false;
	}
	return true;
}

void WaitForKeyState::draw()
{
	std::string text = "PRESS A KEY";
	int height;
	int width = gfx->common->font.getDims(text, &height);

	int cx = 160 - width / 2 - 2;
	int cy = 100 - height / 2 - 2;

	drawRoundedBox(gfx->playRenderer.bmp, cx, cy, 0, height + 1, width + 1);
	gfx->common->font.drawText(gfx->playRenderer.bmp, text, cx + 2, cy + 2, 50);
}

// --- InfoBoxState ---

InfoBoxState::InfoBoxState(std::string text, int x, int y, bool clearScreen)
: text_(std::move(text))
, x_(x)
, y_(y)
, clearScreen_(clearScreen)
{
}

void InfoBoxState::enter()
{
}

void InfoBoxState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev);

	if (ev.type == SDL_EVENT_KEY_DOWN)
		done_ = true;
}

bool InfoBoxState::update()
{
	if (done_)
	{
		gfx->clearKeys();
		if (clearScreen_)
			fill(gfx->playRenderer.bmp, 0);
		return false;
	}
	return true;
}

void InfoBoxState::draw()
{
	if (clearScreen_)
	{
		gfx->playRenderer.pal = gfx->common->exepal;
		fill(gfx->playRenderer.bmp, 0);
	}

	int height;
	int width = gfx->common->font.getDims(text_, &height);

	int cx = x_ - width / 2 - 2;
	int cy = y_ - height / 2 - 2;

	drawRoundedBox(gfx->playRenderer.bmp, cx, cy, 0, height + 1, width + 1);
	gfx->common->font.drawText(gfx->playRenderer.bmp, text_, cx + 2, cy + 2, 6);
}
