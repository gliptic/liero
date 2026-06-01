#include "mainMenu.hpp"

#include "../gfx.hpp"
#include "../mixer/player.hpp"

ItemBehavior* MainMenu::getItemBehavior(Common& common, MenuItem& item) {
  // MaReplays and MaTc are intercepted by MainMenuState before
  // onEnter() is reached. Return the default no-op behavior.
  return Menu::getItemBehavior(common, item);
}
