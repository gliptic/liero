#include "mainMenuState.hpp"

#include "gfx.hpp"
#include "sfx.hpp"
#include "text.hpp"
#include "keys.hpp"
#include "level.hpp"
#include "filesystem.hpp"
#include "controller/controller.hpp"
#include "menu/mainMenu.hpp"
#include "fileSelectorState.hpp"
#include "weaponMenuState.hpp"
#include "inputState.hpp"
#include "rand.hpp"
#include "net/session.hpp"

#include <cstring>
#include <random>
#include <vector>
#include <limits>
#include <algorithm>
#include <cctype>

using std::vector;

#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

static int levenshtein(char const *s1, char const *s2) {
	std::size_t x, y, s1len, s2len;
	s1len = strlen(s1);
	s2len = strlen(s2);
	std::size_t w = s1len + 1;
	std::vector<unsigned> matrix(w * (s2len + 1));
	matrix[0] = 0;
	for (x = 1; x <= s2len; x++)
		matrix[x * w] = matrix[(x - 1) * w] + 1;
	for (y = 1; y <= s1len; y++)
		matrix[y] = matrix[y - 1] + 1;
	for (x = 1; x <= s2len; x++)
		for (y = 1; y <= s1len; y++)
		{
			int c = std::tolower(s1[y - 1]) == std::tolower(s2[x - 1]) ? 0 : 1;
			matrix[x * w + y] = MIN3(matrix[(x - 1) * w + y] + 1, matrix[x * w + y - 1] + 1, matrix[(x - 1) * w + y - 1] + c);
		}
	return (int)(matrix[s2len * w + s1len]);
}

#undef MIN3

static void resetLeftRight()
{
	gfx.releaseSDLKey(SDL_SCANCODE_LEFT);
	gfx.releaseSDLKey(SDL_SCANCODE_RIGHT);
	gfx.releaseControl(WormSettingsExtensions::Left);
	gfx.releaseControl(WormSettingsExtensions::Right);
}

MainMenuState::MainMenuState()
{
}

void MainMenuState::enter()
{
	Common& common = *gfx->common;
	int centerX = gfx->singleScreenRenderer.renderResX / 2;

	std::memset(gfx->playRenderer.pal.entries, 0, sizeof(gfx->playRenderer.pal.entries));
	std::memset(gfx->singleScreenRenderer.pal.entries, 0, sizeof(gfx->singleScreenRenderer.pal.entries));
	gfx->flip();
	gfx->process();

	fillRect(gfx->playRenderer.bmp, 0, 151, 160, 7, 0);
	common.font.drawText(gfx->playRenderer.bmp, LS(Copyright2), 2, 152, 19);

	if (gfx->controller->running())
	{
		gfx->mainMenu.setVisibility(MainMenu::MaResumeGame, true);
		gfx->mainMenu.itemFromId(MainMenu::MaResumeGame)->string = "RESUME GAME (F1)";
		gfx->mainMenu.itemFromId(MainMenu::MaNewGame)->string = "NEW GAME";
		startItemId_ = MainMenu::MaResumeGame;
	}
	else
	{
		gfx->mainMenu.setVisibility(MainMenu::MaResumeGame, false);
		gfx->mainMenu.itemFromId(MainMenu::MaNewGame)->string = "NEW GAME (F1)";
		startItemId_ = MainMenu::MaNewGame;
	}

	gfx->mainMenu.moveToFirstVisible();
	gfx->settingsMenu.moveToFirstVisible();
	gfx->settingsMenu.updateItems(common);

	gfx->playRenderer.fadeValue = 0;
	gfx->singleScreenRenderer.fadeValue = 0;
	gfx->curMenu = &gfx->mainMenu;

	gfx->frozenScreen.copy(gfx->playRenderer.bmp);
	gfx->singleScreenRenderer.clear();
	if (gfx->controller->currentLevel())
	{
		gfx->controller->currentLevel()->drawMiniature(
			gfx->singleScreenRenderer.bmp,
			centerX - 126,
			gfx->singleScreenRenderer.renderResY - 208, 2);
	}
	gfx->frozenSpectatorScreen.copy(gfx->singleScreenRenderer.bmp);

	gfx->menuCycles = 0;
	selected_ = -1;
	phase_ = Phase::Active;
}

void MainMenuState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev);
}

bool MainMenuState::update()
{
	if (phase_ == Phase::FadingOut)
	{
		if (gfx->playRenderer.fadeValue > 0)
		{
			--gfx->playRenderer.fadeValue;
			--gfx->singleScreenRenderer.fadeValue;
			return true;
		}
		return false;
	}

	// Check if a sub-state left a result for us
	if (gfx->pendingMenuSelection >= 0)
	{
		selected_ = gfx->pendingMenuSelection;
		gfx->pendingMenuSelection = -1;
	}

	// Phase::Active — process input
	Common& common = *gfx->common;

	if(gfx->testSDLKeyOnce(SDL_SCANCODE_ESCAPE)
	|| gfx->testControlOnce(WormSettingsExtensions::Jump)
	|| gfx->testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_EAST))
	{
		if(gfx->curMenu == &gfx->mainMenu)
			gfx->mainMenu.moveToId(MainMenu::MaQuit);
		else
			gfx->curMenu = &gfx->mainMenu;
	}

	if(gfx->testSDLKeyOnce(SDL_SCANCODE_UP)
	|| gfx->testControlOnce(WormSettingsExtensions::Up)
	|| gfx->testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_UP))
	{
		sfx.play(common, 26);
		gfx->curMenu->movement(-1);
	}

	if(gfx->testSDLKeyOnce(SDL_SCANCODE_DOWN)
	|| gfx->testControlOnce(WormSettingsExtensions::Down)
	|| gfx->testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_DOWN))
	{
		sfx.play(common, 25);
		gfx->curMenu->movement(1);
	}

	if(gfx->testSDLKeyOnce(SDL_SCANCODE_RETURN)
	|| gfx->testSDLKeyOnce(SDL_SCANCODE_KP_ENTER)
	|| gfx->testControlOnce(WormSettingsExtensions::Fire)
	|| gfx->testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH))
	{
		if(gfx->curMenu == &gfx->mainMenu)
		{
			sfx.play(common, 27);

			int s = gfx->mainMenu.selectedId();
			switch (s)
			{
				case MainMenu::MaSettings:
				{
					gfx->curMenu = &gfx->settingsMenu;
					break;
				}

				case MainMenu::MaPlayer1Settings:
				case MainMenu::MaPlayer2Settings:
				{
					gfx->playerSettings(s - MainMenu::MaPlayer1Settings);
					break;
				}

				case MainMenu::MaNetPlayerSettings:
				{
					gfx->playerSettings(Settings::NetworkPlayerIdx);
					break;
				}

				case MainMenu::MaAdvanced:
				{
					gfx->openHiddenMenu();
					break;
				}

				case MainMenu::MaReplays:
				{
					gfx->stateStack.push(std::make_unique<ReplaySelectorState>(), gfx);
					break;
				}

				case MainMenu::MaTc:
				{
					gfx->stateStack.push(std::make_unique<TcSelectorState>(), gfx);
					break;
				}

				case MainMenu::MaJoinGame:
				{
					sfx.play(common, 27);
					gfx->stateStack.push(std::make_unique<InputStringState>(
						"", 40, 10, 80, nullptr, "ADDRESS: ", false,
						[this](bool accepted, std::string const& result) {
							if (accepted && !result.empty())
							{
								gfx->pendingNetAddress = result;
								gfx->pendingMenuSelection = MainMenu::MaJoinGame;
							}
						}), gfx);
					break;
				}

				case MainMenu::MaHostOnline:
				{
					sfx.play(common, 27);
					gfx->pendingMenuSelection = MainMenu::MaHostOnline;
					break;
				}

				case MainMenu::MaJoinOnline:
				{
					sfx.play(common, 27);
					gfx->stateStack.push(std::make_unique<InputStringState>(
						"", 6, 10, 80, ::toupper, "ROOM CODE: ", false,
						[this](bool accepted, std::string const& result) {
							if (accepted && result.size() == 6)
							{
								gfx->pendingNetAddress = result;
								gfx->pendingMenuSelection = MainMenu::MaJoinOnline;
							}
						}), gfx);
					break;
				}

				default:
				{
					gfx->curMenu = &gfx->mainMenu;
					selected_ = s;
				}
			}
		}
		else if(gfx->curMenu == &gfx->settingsMenu)
		{
			int itemId = gfx->settingsMenu.selectedId();
			switch (itemId)
			{
				case SettingsMenu::SiLevel:
					sfx.play(common, 27);
					gfx->stateStack.push(std::make_unique<LevelSelectorState>(), gfx);
					break;

				case SettingsMenu::SiWeaponOptions:
					sfx.play(common, 27);
					gfx->stateStack.push(std::make_unique<WeaponMenuState>(), gfx);
					break;

				case SettingsMenu::LoadOptions:
					sfx.play(common, 27);
					gfx->stateStack.push(std::make_unique<OptionsSelectorState>(), gfx);
					break;

				case SettingsMenu::SaveOptions:
				{
					sfx.play(common, 27);
					int x, y;
					auto* item = gfx->settingsMenu.itemFromId(SettingsMenu::SaveOptions);
					if (item && gfx->settingsMenu.itemPosition(*item, x, y))
					{
						x += gfx->settingsMenu.valueOffsetX + 2;
						std::string name = getBasename(getLeaf(gfx->settingsNode.fullPath()));
						gfx->stateStack.push(std::make_unique<InputStringState>(
							name, 30, x, y, nullptr, "", false,
							[this](bool accepted, std::string const& result) {
								if (accepted && !result.empty())
								{
									gfx->saveSettings(gfx->getConfigNode() / "Setups" / (result + ".cfg"));
								}
								sfx.play(*gfx->common, 27);
								gfx->settingsMenu.updateItems(*gfx->common);
							}), gfx);
					}
					break;
				}

				default:
					gfx->settingsMenu.onEnter(common);
					break;
			}
		}
		else if(gfx->curMenu == &gfx->playerMenu)
		{
			int itemId = gfx->playerMenu.selectedId();

			if (itemId == PlayerMenu::PlLoadProfile)
			{
				sfx.play(common, 27);
				gfx->stateStack.push(
					std::make_unique<ProfileSelectorState>(*gfx->playerMenu.ws), gfx);
			}
			else if (itemId == PlayerMenu::PlName)
			{
				sfx.play(common, 27);
				auto& ws = *gfx->playerMenu.ws;
				int x, y;
				auto* item = gfx->playerMenu.itemFromId(itemId);
				if (item && gfx->playerMenu.itemPosition(*item, x, y))
				{
					x += gfx->playerMenu.valueOffsetX + 2;
					gfx->stateStack.push(std::make_unique<InputStringState>(
						ws.name, 20, x, y, nullptr, "", false,
						[this](bool accepted, std::string const& result) {
							auto& ws = *gfx->playerMenu.ws;
							if (accepted)
								ws.name = result;
							if (ws.name.empty())
								Settings::generateName(ws, gfx->rand);
							ws.randomName = false;
							sfx.play(*gfx->common, 27);
							gfx->playerMenu.updateItems(*gfx->common);
						}), gfx);
				}
			}
			else if (itemId == PlayerMenu::PlSaveProfileAs)
			{
				sfx.play(common, 27);
				int x, y;
				auto* item = gfx->playerMenu.itemFromId(itemId);
				if (item && gfx->playerMenu.itemPosition(*item, x, y))
				{
					x += gfx->playerMenu.valueOffsetX + 2;
					gfx->stateStack.push(std::make_unique<InputStringState>(
						"", 30, x, y, nullptr, "", false,
						[this](bool accepted, std::string const& result) {
							if (accepted && !result.empty())
							{
								gfx->playerMenu.ws->saveProfile(
									gfx->getConfigNode() / "Profiles" / (result + ".toml"));
							}
							sfx.play(*gfx->common, 27);
							gfx->playerMenu.updateItems(*gfx->common);
						}), gfx);
				}
			}
			else if ((itemId >= PlayerMenu::PlUp && itemId <= PlayerMenu::PlJump)
				|| itemId == PlayerMenu::PlDig)
			{
				sfx.play(common, 27);
				bool extended = gfx->settings->extensions;
				int keyIdx = itemId - PlayerMenu::PlUp;

				gfx->stateStack.push(std::make_unique<WaitForKeyState>(
					extended,
					[this, keyIdx](uint32_t k, bool isGamepad) {
						auto& ws = *gfx->playerMenu.ws;
						if (k != DkEscape)
						{
							if (isGamepad)
							{
								ws.gamepadControls[keyIdx] = k;
							}
							else
							{
								if (!isExtendedKey(k))
									ws.controls[keyIdx] = k;
								ws.controlsEx[keyIdx] = k;
							}
							gfx->playerMenu.updateItems(*gfx->common);
						}
					}), gfx);
			}
			else if (itemId >= PlayerMenu::PlWeap0 && itemId < PlayerMenu::PlWeap0 + 5)
			{
				sfx.play(common, 27);
				int x, y;
				auto* item = gfx->playerMenu.itemFromId(itemId);
				if (item && gfx->playerMenu.itemPosition(*item, x, y))
				{
					x += gfx->playerMenu.valueOffsetX + 2;
					int weapIdx = itemId - PlayerMenu::PlWeap0;
					gfx->stateStack.push(std::make_unique<InputStringState>(
						"", 10, x, y, nullptr, "", false,
						[this, weapIdx](bool accepted, std::string const& result) {
							if (accepted && !result.empty())
							{
								Common& common = *gfx->common;
								auto& ws = *gfx->playerMenu.ws;
								uint32_t numWeapons = (uint32_t)common.weapons.size();

								uint32_t best = ws.weapons[weapIdx];
								double bestDist = std::numeric_limits<double>::max();
								for (uint32_t i = 1; i <= numWeapons; ++i)
								{
									std::string& name = common.weapons[common.weapOrder[i - 1]].name;
									double dist = levenshtein(name.c_str(), result.c_str())
										/ (double)name.length();
									if (dist < bestDist)
									{
										best = i;
										bestDist = dist;
									}
								}
								ws.weapons[weapIdx] = best;
								gfx->playerMenu.updateItems(common);
							}
						}), gfx);
				}
			}
			else
			{
				selected_ = gfx->curMenu->onEnter(common);
			}
		}
		else
		{
			selected_ = gfx->curMenu->onEnter(common);
		}
	}

	if(gfx->testSDLKeyOnce(SDL_SCANCODE_F1))
	{
		gfx->curMenu = &gfx->mainMenu;
		gfx->mainMenu.moveToId(startItemId_);
		selected_ = startItemId_;
	}
	if(gfx->testSDLKeyOnce(SDL_SCANCODE_F2))
	{
		gfx->mainMenu.moveToId(MainMenu::MaAdvanced);
		gfx->openHiddenMenu();
	}
	if(gfx->testSDLKeyOnce(SDL_SCANCODE_F3))
	{
		gfx->curMenu = &gfx->mainMenu;
		gfx->mainMenu.moveToId(MainMenu::MaReplays);
		gfx->stateStack.push(std::make_unique<ReplaySelectorState>(), gfx);
	}

	if (gfx->testSDLKeyOnce(SDL_SCANCODE_F5))
	{
		gfx->mainMenu.moveToId(MainMenu::MaPlayer1Settings);
		gfx->playerSettings(0);
	}
	if (gfx->testSDLKeyOnce(SDL_SCANCODE_F6))
	{
		gfx->mainMenu.moveToId(MainMenu::MaPlayer2Settings);
		gfx->playerSettings(1);
	}
	if (gfx->testSDLKeyOnce(SDL_SCANCODE_F7))
	{
		gfx->mainMenu.moveToId(MainMenu::MaSettings);
		gfx->curMenu = &gfx->settingsMenu;
	}

	if (gfx->testSDLKeyOnce(SDL_SCANCODE_F9))
	{
		gfx->mainMenu.moveToId(MainMenu::MaNetPlayerSettings);
		gfx->playerSettings(Settings::NetworkPlayerIdx);
	}

	if (gfx->testSDLKeyOnce(SDL_SCANCODE_F8))
	{
		uint32_t s = 14;

		Rand r;
		r.seed(s);

		Common& common = *gfx->common;

		vector<std::size_t> nobjMap;

		for (std::size_t i = 0; i < common.nobjectTypes.size(); ++i)
		{
			nobjMap.push_back(i);
		}
		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(nobjMap.begin(), nobjMap.end(), g);

		for (auto& w : common.weapons)
		{
			w.addSpeed = r(30) - 5;
			w.affectByExplosions = r(2) == 0;
			w.affectByWorm = r(3) == 0;
			w.ammo = r(20) + 1;
			w.bloodOnHit = r(50);
			w.blowAway = r(10);
			w.bounce = r(90);
			w.collideWithObjects = r(10) == 0;
			w.colorBullets = 3 + r(250);
			w.createOnExp = r(common.sobjectTypes.size());
			w.delay = r(70);
			w.detectDistance = r(20);
			w.dirtEffect = r(9);
			w.distribution = r(5000) - 2500;
			w.explGround = r(2) == 0;
			w.exploSound = r(common.sounds.size());
			w.fireCone = r(10);
			w.gravity = r(2000) - 1000;
			w.hitDamage = r(20);
			w.laserSight = r(5) == 0;
			w.launchSound = r(common.sounds.size());
			w.leaveShellDelay = r(30);
			w.leaveShells = r(1) == 0;
			w.loadingTime = r(70 * 3);
			w.loopAnim = r(10) == 0;
			w.loopSound = false;
			w.multSpeed = r(2) ? 100 : 99 + r(5);
			w.objTrailDelay = 10 + r(70);
			w.objTrailType = r(4) == 0 ? r(common.sobjectTypes.size()) : -1;
			w.parts = r(2) == 0 ? r(10) : 1;
			w.partTrailDelay = 10 + r(70);
			w.partTrailObj = r(4) == 0 ? r(common.nobjectTypes.size()) : -1;
			w.partTrailType = r(2);
			w.playReloadSound = r(2) == 0;
			w.recoil = r(20);
			w.shadow = r(2) == 0;
			w.shotType = r(5);
			w.speed = r(200);
			w.splinterAmount = r(5) == 0 ? r(10) : 0;
			w.splinterColour = r(256);
			w.splinterScatter = r(2);
			w.splinterType = r(common.nobjectTypes.size());
			w.startFrame = r((uint32_t)common.smallSprites.count - 13);
			w.numFrames = r(5);
			w.timeToExplo = 50 + r(200);
			w.timeToExploV = 10 + r(50);
			w.wormCollide = r(3) > 0;
			w.wormExplode = r(3) > 0;
		}

		for (std::size_t idx = 0; idx < common.nobjectTypes.size(); ++idx)
		{
			auto& n = common.nobjectTypes[nobjMap[idx]];
			n.affectByExplosions = r(5) == 0;
			n.bloodOnHit = r(5);
			n.bloodTrail = r(10) == 0;
			n.bloodTrailDelay = r(20) + 3;
			n.blowAway = r(10);
			n.bounce = r(90);
			n.colorBullets = 3 + r(250);
			n.createOnExp = r(3) == 0 ? r(common.sobjectTypes.size()) : -1;
			n.detectDistance = r(20);
			n.dirtEffect = r(9);
			n.distribution = r(5000) - 2500;
			n.drawOnMap = r(20) == 0;
			n.explGround = r(4) > 0;
			n.gravity = r(2000) - 1000;
			n.hitDamage = r(10);
			n.leaveObj = r(5) == 0 ? r(common.sobjectTypes.size()) : -1;
			n.leaveObjDelay = 10 + r(80);
			n.startFrame = r((uint32_t)common.smallSprites.count - 13);
			n.numFrames = r(5);
			n.speed = r(150);
			n.splinterAmount = idx > 0 && r(5) == 0 ? r(10) : 0;
			n.splinterColour = r(256);
			n.splinterType = idx > 0 ? nobjMap[r(idx)] : 0;
			n.timeToExplo = 50 + r(70 * 3);
			n.timeToExploV = r(30);
			n.wormDestroy = r(3) == 0;
			n.wormExplode = r(2) == 0;
		}

		for (auto& s : common.sobjectTypes)
		{
			s.animDelay = 1 + r(10);
			s.blowAway = r(2) == 0 ? r(10000) : 0;
			s.damage = r(30);
			s.detectRange = r(20);
			s.dirtEffect = r(9);
			s.flash = r(5);
			s.startFrame = r((uint32_t)common.largeSprites.count - 7);
			s.numFrames = r(7);
			s.startSound = r(common.sounds.size());
			s.shake = r(10);
			s.shadow = r(2);
			s.numSounds = 1;
		}
	}

	if(gfx->testSDLKey(SDL_SCANCODE_LEFT)
	|| gfx->testControl(WormSettingsExtensions::Left)
	|| gfx->testGamepadDir(SDL_GAMEPAD_BUTTON_DPAD_LEFT))
	{
		if(!gfx->curMenu->onLeftRight(common, -1))
			resetLeftRight();
	}
	if(gfx->testSDLKey(SDL_SCANCODE_RIGHT)
	|| gfx->testControl(WormSettingsExtensions::Right)
	|| gfx->testGamepadDir(SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
	{
		if(!gfx->curMenu->onLeftRight(common, 1))
			resetLeftRight();
	}

	if(gfx->testSDLKeyOnce(SDL_SCANCODE_PAGEUP))
	{
		sfx.play(common, 26);
		gfx->curMenu->movementPage(-1);
	}

	if(gfx->testSDLKeyOnce(SDL_SCANCODE_PAGEDOWN))
	{
		sfx.play(common, 25);
		gfx->curMenu->movementPage(1);
	}

	if (selected_ >= 0)
	{
		// Transition to fade-out
		phase_ = Phase::FadingOut;
		gfx->playRenderer.fadeValue = 32;
		gfx->singleScreenRenderer.fadeValue = 32;
	}

	return true;
}

void MainMenuState::draw()
{
	gfx->drawBasicMenu();
	gfx->drawSpectatorInfo();

	Common& common = *gfx->common;

	if(gfx->curMenu == &gfx->mainMenu)
		gfx->settingsMenu.draw(common, gfx->playRenderer, true);
	else
		gfx->curMenu->draw(common, gfx->playRenderer, false);
}
