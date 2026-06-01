#include "inputState.hpp"

#include "gfx.hpp"
#include "mixer/player.hpp"
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
		g_soundPlayer->play(gfx->common->soundHook[SoundMenuSelect]);
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

		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
			if (ev.gaxis.value > JoyAxisThreshold)
			{
				result_ = WormSettingsExtensions::gamepadAxisPositive(ev.gaxis.axis);
				isGamepadResult_ = true;
				done_ = true;
			}
			else if (ev.gaxis.value < -JoyAxisThreshold)
			{
				result_ = WormSettingsExtensions::gamepadAxisNegative(ev.gaxis.axis);
				isGamepadResult_ = true;
				done_ = true;
			}
			break;

		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			result_ = ev.gbutton.button;
			isGamepadResult_ = true;
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
		callback_(result_, isGamepadResult_);
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

InfoBoxState::InfoBoxState(std::string text, int x, int y, bool clearScreen,
	DismissCallback onDismiss)
: text_(std::move(text))
, x_(x)
, y_(y)
, clearScreen_(clearScreen)
, onDismiss_(std::move(onDismiss))
{
}

void InfoBoxState::enter()
{
}

void InfoBoxState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev);

	if (ev.type == SDL_EVENT_KEY_DOWN
	 || ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
		done_ = true;
}

bool InfoBoxState::update()
{
	if (done_)
	{
		gfx->clearKeys();
		if (clearScreen_)
			fill(gfx->playRenderer.bmp, 0);
		// onDismiss_ runs before this state is popped. It may call
		// scheduleReplaceTop() on the StateStack to chain into another
		// state; that takes precedence over the normal pop because the
		// stack checks pendingReplace_ before keepRunning.
		if (onDismiss_)
			onDismiss_();
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
