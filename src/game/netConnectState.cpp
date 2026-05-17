#include "netConnectState.hpp"

#include "gfx.hpp"
#include "text.hpp"
#include "keys.hpp"
#include "common.hpp"
#include "gamePlayState.hpp"
#include "inputState.hpp"
#include "controller/controller.hpp"
#include "net/localaddr.hpp"

#include <memory>
#include <string>

NetConnectState::NetConnectState(NetSession::Role role, std::string address, uint16_t port)
: role_(role)
, address_(std::move(address))
, port_(port)
{
}

void NetConnectState::enter()
{
	auto session = std::make_unique<NetSession>(gfx->common, gfx->settings);

	bool ok = false;
	if (role_ == NetSession::Host)
	{
		ok = session->hostGame(port_);
		if (ok)
			localAddresses_ = getLocalAddresses();
	}
	else
		ok = session->joinGame(address_, port_);

	if (!ok)
	{
		gfx->pendingErrorMessage = "FAILED TO START NETWORK";
		cancel_ = true;
		gfx->netSession.reset();
		return;
	}

	gfx->netSession = std::move(session);
}

void NetConnectState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev);
}

bool NetConnectState::update()
{
	if (cancel_)
	{
		if (!gfx->pendingErrorMessage.empty())
		{
			std::string msg = std::move(gfx->pendingErrorMessage);
			gfx->pendingErrorMessage.clear();
			gfx->stateStack.scheduleReplaceTop(
				std::make_unique<InfoBoxState>(std::move(msg), 160, 100, true));
		}
		return false;
	}

	if (gfx->testSDLKeyOnce(SDL_SCANCODE_ESCAPE))
	{
		gfx->netSession.reset();
		gfx->clearKeys();
		return false;
	}

	if (!gfx->netSession)
		return false;

	gfx->netSession->update();

	auto state = gfx->netSession->sessionState();

	if (state == NetSession::Playing)
	{
		// Connection established — transfer controller and start game
		auto ctrl = gfx->netSession->releaseController();
		gfx->controller = std::unique_ptr<Controller>(ctrl.release());
		gfx->stateStack.scheduleReplaceTop(std::make_unique<GamePlayState>());
		return true;
	}

	if (state == NetSession::Failed)
	{
		gfx->netSession.reset();
		gfx->stateStack.scheduleReplaceTop(
			std::make_unique<InfoBoxState>("CONNECTION FAILED", 160, 100, true));
		return true;
	}

	if (state == NetSession::Disconnected)
	{
		gfx->netSession.reset();
		gfx->stateStack.scheduleReplaceTop(
			std::make_unique<InfoBoxState>("PEER DISCONNECTED", 160, 100, true));
		return true;
	}

	return true;
}

void NetConnectState::draw()
{
	Common& common = *gfx->common;
	Font& font = common.font;

	gfx->playRenderer.pal = common.exepal;
	fill(gfx->playRenderer.bmp, 0);

	std::string line1;
	if (role_ == NetSession::Host)
		line1 = "HOSTING ON PORT " + std::to_string(port_);
	else
		line1 = "CONNECTING TO " + address_;

	std::string line2;
	if (gfx->netSession)
	{
		switch (gfx->netSession->sessionState())
		{
			case NetSession::WaitingForPeer:
				line2 = "WAITING FOR PEER...";
				break;
			case NetSession::Handshaking:
				line2 = "HANDSHAKING...";
				break;
			default:
				line2 = "CONNECTING...";
				break;
		}
	}
	else
	{
		line2 = "STARTING...";
	}

	std::string line3 = "PRESS ESC TO CANCEL";

	int cx = 160;
	int cy = 60;

	int w1 = font.getDims(line1);
	font.drawText(gfx->playRenderer.bmp, line1, cx - w1 / 2, cy, 50);

	int w2 = font.getDims(line2);
	font.drawText(gfx->playRenderer.bmp, line2, cx - w2 / 2, cy + 12, 7);

	// Show local addresses when hosting
	if (role_ == NetSession::Host && !localAddresses_.empty())
	{
		int addrY = cy + 30;
		std::string hdr = "CONNECT USING:";
		int wh = font.getDims(hdr);
		font.drawText(gfx->playRenderer.bmp, hdr, cx - wh / 2, addrY, 6);
		addrY += 10;

		for (auto& addr : localAddresses_)
		{
			std::string display = addr.ip + ":" + std::to_string(port_);
			int wd = font.getDims(display);
			font.drawText(gfx->playRenderer.bmp, display, cx - wd / 2, addrY, 7);
			addrY += 10;
		}

		int w3 = font.getDims(line3);
		font.drawText(gfx->playRenderer.bmp, line3, cx - w3 / 2, addrY + 6, 6);
	}
	else
	{
		int w3 = font.getDims(line3);
		font.drawText(gfx->playRenderer.bmp, line3, cx - w3 / 2, cy + 30, 6);
	}
}
