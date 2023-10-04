#pragma once
#include <string>

class Discord {
	static inline std::string previousApplicationIdentifier;
	static inline time_t applicationStartTime = time(0);

	static inline bool IsReady = false;
	static void Handle_Ready(const struct DiscordUser* request);
	static void Handle_Error(int code, const char* message);
public:


	static void clean();
	static void clear();
	static void start(const std::string& ApplicationIdentifier);
	static void restart(const std::string& ApplicationIdentifier);
	static void setStartTime(time_t time);
	static void update(const std::string& State, const std::string& Details, const std::string& smallImageKey = {}, const std::string& smallImageText = {});
};