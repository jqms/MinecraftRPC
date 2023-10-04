#include <iostream>
#include "Discord.h"
#include "RPC/discord_rpc.h"
#include "Discord.h"
#include <Windows.h>
#include <unordered_map>

void Discord::Handle_Ready(const DiscordUser* request) {
	IsReady = true;
	std::cout << "RPC Initialised for " << request->userId << " (" << request->username << '#' << request->discriminator << ")" << std::endl;
	update("", "");
}

void Discord::Handle_Error(int code, const char* message) {
	std::cerr << "RPC FAILED " << code << " " << message << std::endl;
}


void Discord::clean() {
	if (previousApplicationIdentifier.empty()) return;
	previousApplicationIdentifier.clear();
	clear();
	Discord_Shutdown();
}

void Discord::clear() {
	previousApplicationIdentifier.clear();
	Discord_ClearPresence();
}

void Discord::start(const std::string& ApplicationIdentifier) {
	previousApplicationIdentifier = ApplicationIdentifier;
	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));
	handlers.ready = Discord::Handle_Ready;
	handlers.errored = Discord::Handle_Error;
	Discord_Initialize(ApplicationIdentifier.c_str(), &handlers, 1, 0);
	IsReady = false;
	for (int i = 0; i < 500; i++) {
		Discord_RunCallbacks();
		if (IsReady) break;
		Sleep(10);
	}


}

void Discord::restart(const std::string& ApplicationIdentifier) {
	if (!previousApplicationIdentifier.empty() && ApplicationIdentifier != previousApplicationIdentifier) clean();
	start(ApplicationIdentifier);
}

void Discord::setStartTime(time_t time) {
	applicationStartTime = time;
}

void Discord::update(const std::string& State, const std::string& Details, const std::string& smallImageKey, const std::string& smallImageText) {
	DiscordRichPresence rpc;
	memset(&rpc, 0, sizeof(rpc));
	rpc.startTimestamp = applicationStartTime;
	rpc.largeImageKey = "logo";
	rpc.largeImageText = "discord.gg/onixclient";
	rpc.smallImageKey = smallImageKey.c_str();
	rpc.smallImageText = smallImageText.c_str();

	rpc.state = State.c_str();
	rpc.details = Details.c_str();
	Discord_UpdatePresence(&rpc);
}