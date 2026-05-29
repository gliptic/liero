#include "fileSelectorState.hpp"

#include "gfx.hpp"
#include "mixer/player.hpp"
#include "text.hpp"
#include "keys.hpp"
#include "level.hpp"
#include "common.hpp"
#include "settings.hpp"
#include "controller/controller.hpp"
#include "controller/replayController.hpp"
#include "menu/mainMenu.hpp"


using std::string;
using std::shared_ptr;

// --- FileSelectorState base ---

FileSelectorState::FileSelectorState(std::string title)
: title_(std::move(title))
{
}

void FileSelectorState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev);
}

bool FileSelectorState::update()
{
	if (done_)
		return false;

	if (!selector_->process())
	{
		done_ = true;
		return false;
	}

	if (gfx->testSDLKeyOnce(SDL_SCANCODE_RETURN)
	 || gfx->testSDLKeyOnce(SDL_SCANCODE_KP_ENTER)
	 || gfx->testControlOnce(WormSettingsExtensions::Fire)
	 || gfx->testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH))
	{
		g_soundPlayer->play(gfx->common->soundHook[SoundMenuSelect]);

		auto* sel = selector_->enter();
		if (sel)
		{
			if (onSelected(sel))
			{
				done_ = true;
				return false;
			}
		}
	}

	return true;
}

void FileSelectorState::draw()
{
	gfx->playRenderer.bmp.copy(gfx->frozenScreen);

	if (!title_.empty())
	{
		string displayTitle = title_;
		if (!selector_->currentNode->fullPath.empty())
		{
			displayTitle += ' ';
			displayTitle += selector_->currentNode->fullPath;
		}

		gfx->common->font.drawFramedText(gfx->playRenderer.bmp, displayTitle, 178, 20, 50);
	}

	drawExtra();
	selector_->draw();
}

// --- LevelSelectorState ---

LevelSelectorState::LevelSelectorState()
: FileSelectorState("")
{
}

void LevelSelectorState::enter()
{
	Common& common = *gfx->common;
	// title_ left empty — LevelSelectorState draws its own title in drawExtra()
	selector_ = std::make_unique<FileSelector>(common);

	randomNode_ = std::make_shared<FileNode>(
		LS(Random), "", "", false, &selector_->rootNode);

	selector_->fill(gfx->getConfigNode(), [](string const& name, string const& ext) {
		return ciCompare(ext, "LEV");
	});

	randomNode_->id = 1;
	selector_->rootNode.children.insert(selector_->rootNode.children.begin(), randomNode_);
	selector_->setFolder(selector_->rootNode);
	selector_->select(gfx->settings->levelFile);

	previewNode_ = nullptr;
}

bool LevelSelectorState::onSelected(FileNode* node)
{
	if (node == randomNode_.get())
	{
		gfx->settings->randomLevel = true;
		gfx->settings->levelFile.clear();
	}
	else
	{
		gfx->settings->randomLevel = false;
		gfx->settings->levelFile = node->fullPath;
	}
	return true;
}

void LevelSelectorState::drawExtra()
{
	Common& common = *gfx->common;
	FileNode* sel = selector_->curSel();

	if (previewNode_ != sel && sel && sel != randomNode_.get() && !sel->folder)
	{
		Level level(common);

		try
		{
			auto r_ptr = sel->getFsNode().toReader(); io::Reader& r = *r_ptr;
			if (level.load(common, *gfx->settings, r))
			{
				int centerX = gfx->singleScreenRenderer.renderResX / 2;

				level.drawMiniature(gfx->frozenScreen, 134, 162, 10);
				level.drawMiniature(gfx->frozenSpectatorScreen, centerX - 126, gfx->singleScreenRenderer.renderResY - 208, 2);
			}
		}
		catch (std::runtime_error&)
		{
			// Ignore
		}

		previewNode_ = sel;
	}

	// Draw title with width for rounded box (level selector uses different title rendering)
	string title = LS(SelLevel);
	if (!selector_->currentNode->fullPath.empty())
	{
		title += ' ';
		title += selector_->currentNode->fullPath;
	}

	int wid = common.font.getDims(title);
	drawRoundedBox(gfx->playRenderer.bmp, 178, 20, 0, 7, wid);
	common.font.drawText(gfx->playRenderer.bmp, title, 180, 21, 50);
}

// --- ReplaySelectorState ---

ReplaySelectorState::ReplaySelectorState()
: FileSelectorState("Select replay:")
{
}

void ReplaySelectorState::enter()
{
	selector_ = std::make_unique<FileSelector>(*gfx->common, 28);

	selector_->fill(gfx->getConfigNode(), [](string const& name, string const& ext) {
		return ciCompare(ext, "LRP");
	});

	selector_->setFolder(selector_->rootNode);
	if (gfx->prevSelectedReplayPath.empty()
	  || !selector_->select(gfx->prevSelectedReplayPath))
	{
		selector_->select(joinPath(gfx->getConfigNode().fullPath(), "Replays"));
	}
}

bool ReplaySelectorState::onSelected(FileNode* node)
{
	gfx->prevSelectedReplayPath = node->fullPath;

	// Reset controller before opening the replay, since we may be recording it
	gfx->controller.reset();
	gfx->controller.reset(new ReplayController(gfx->common, node->getFsNode().toReader()));

	gfx->pendingMenuSelection = MainMenu::MaReplay;
	return true;
}

// --- ProfileSelectorState ---

ProfileSelectorState::ProfileSelectorState(WormSettings& ws)
: FileSelectorState("Select profile:")
, ws_(ws)
{
}

void ProfileSelectorState::enter()
{
	selector_ = std::make_unique<FileSelector>(*gfx->common, 28);

	selector_->fill(gfx->getConfigNode(), [](string const& name, string const& ext) {
		return ciCompare(ext, "TOML");
	});

	selector_->setFolder(selector_->rootNode);
	selector_->select(joinPath(gfx->getConfigNode().fullPath(), "Profiles"));
}

bool ProfileSelectorState::onSelected(FileNode* node)
{
	ws_.loadProfile(node->getFsNode());
	// Refresh the player menu so it reflects the newly loaded profile
	gfx->playerMenu.updateItems(*gfx->common);
	return true;
}

// --- OptionsSelectorState ---

OptionsSelectorState::OptionsSelectorState()
: FileSelectorState("Select options:")
{
}

void OptionsSelectorState::enter()
{
	selector_ = std::make_unique<FileSelector>(*gfx->common, 28);

	selector_->fill(gfx->getConfigNode(), [](string const& name, string const& ext) {
		return ciCompare(ext, "CFG");
	});

	selector_->setFolder(selector_->rootNode);
	selector_->select(joinPath(gfx->getConfigNode().fullPath(), "Setups"));
}

bool OptionsSelectorState::onSelected(FileNode* node)
{
	gfx->loadSettings(node->getFsNode());
	gfx->settingsMenu.updateItems(*gfx->common);
	return true;
}

// --- TcSelectorState ---

TcSelectorState::TcSelectorState()
: FileSelectorState("Select TC:")
{
}

void TcSelectorState::enter()
{
	selector_ = std::make_unique<FileSelector>(*gfx->common, 28);

	selector_->fill(gfx->getConfigNode() / "TC", 0);
	selector_->setFolder(selector_->rootNode);

	auto end = std::remove_if(
		selector_->rootNode.children.begin(),
		selector_->rootNode.children.end(),
		[](shared_ptr<FileNode> const& n) {
			auto tc = n->getFsNode() / "tc.cfg";
			return !tc.exists();
		});

	selector_->rootNode.children.erase(end, selector_->rootNode.children.end());

	for (auto& c : selector_->rootNode.children)
	{
		c->folder = false;
	}
}

bool TcSelectorState::onSelected(FileNode* node)
{
	std::unique_ptr<Common> newCommon(new Common());
	newCommon->load(node->getFsNode());
	gfx->settings->tc = node->name;
	gfx->common.reset(newCommon.release());
	if (auto* dp = dynamic_cast<DefaultSoundPlayer*>(gfx->soundPlayer.get()))
		dp->setCommon(*gfx->common);
	gfx->pendingMenuSelection = MainMenu::MaTc;
	return true;
}
