#include <iostream>
#include <chrono>
#include <vector>
#include <fstream>
#include <filesystem>
#include <Windows.h>
#include <Psapi.h>
#include <unordered_map>
#include <shellapi.h>

#include "Discord.h"
#include "json.hpp"
namespace json = nlohmann;

HANDLE minecraftStateThread;
HANDLE discordRpcThread;
bool isApplicationWantingToLive = true;
bool richPresenceEnabled = true;

#pragma region utils
std::string readFile(const std::string& path) {
	std::ifstream input(path, std::ios::ate);
	if (!input.is_open()) return "";
	uint32_t filesize = (uint32_t)input.tellg();
	input.seekg(0);
	char* content = new char[filesize + 2];
	input.read(content, filesize);
	content[filesize] = 0;
	std::string result = content;
	delete[] content;
	return result;
}

static DWORD getPID(const char* procName) {
	DWORD* PidList = new DWORD[4096];
	DWORD damn = 0;
	if (!K32EnumProcesses(PidList, 4096 * sizeof(DWORD), &damn)) {
		delete[] PidList;
		return 0;
	}
	damn = min((int)damn, 4096);

	for (DWORD i = 0; i < damn; i++) {
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, PidList[i]);
		if (hProcess) {
			DWORD cbNeeded;
			HMODULE mod;
			if (!K32EnumProcessModules(hProcess, &mod, sizeof(HMODULE), &cbNeeded)) continue;
			char textualContent[128];
			K32GetModuleBaseNameA(hProcess, mod, textualContent, sizeof(textualContent) / sizeof(textualContent[0]));
			CloseHandle(hProcess);
			if (!strcmp(textualContent, procName))
				return PidList[i];
		}
	}
	return 0;
}

bool isMinecraftOpen() {
	return getPID("Minecraft.Windows.exe") != 0;
}

bool isMinecraftOpen_Slow(int delay) {
	DWORD* PidList = new DWORD[4096];
	Sleep(1000);
	DWORD damn = 0;
	if (!K32EnumProcesses(PidList, 4096 * sizeof(DWORD), &damn)) {
		delete[] PidList;
		return 0;
	}
	damn = min((int)damn, 4096);
	Sleep(1000);

	for (DWORD i = 0; i < damn; i++) {
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, PidList[i]);
		if (hProcess) {
			DWORD cbNeeded;
			HMODULE mod;
			if (!K32EnumProcessModules(hProcess, &mod, sizeof(HMODULE), &cbNeeded)) continue;
			char textualContent[128];
			K32GetModuleBaseNameA(hProcess, mod, textualContent, sizeof(textualContent) / sizeof(textualContent[0]));
			CloseHandle(hProcess);
			Sleep(delay);
			if (!strcmp(textualContent, "Minecraft.Windows.exe")) {
				delete[] PidList;
				Sleep(200);
				return true;
			}
		}
	}
	delete[] PidList;
	Sleep(200);
	return false;
}

#pragma endregion

struct WatchedPath {
	std::string path;
	time_t lastUpdate = 0;

	WatchedPath() = default;
	WatchedPath(const std::string& Path) {
		this->path = Path;
	}

	time_t getLastUpdate() const {
		struct stat s; stat(path.c_str(), &s);
		time_t timeNow = s.st_mtime;
		return timeNow;
	}
	bool shouldUpdate() const {
		time_t timeNow = getLastUpdate();
		if (lastUpdate != timeNow) return true;
	}
	void update() { lastUpdate = getLastUpdate(); }
	void update(time_t LastUpdate) { lastUpdate = LastUpdate; }
	std::string getContent() { return readFile(path); }

	void operator=(const std::string& Path) { new(this) WatchedPath(Path); }
	operator std::string& () { return this->path; }
	std::string& operator*() { return this->path; }
	std::string operator+ (const std::string& rhs) const { return this->path + rhs; }
	std::string operator+=(const std::string& rhs) { return this->path += rhs; }
};
struct WatchedPathList {
	std::vector<WatchedPath*> list;

	void add(WatchedPath& Path) { list.emplace_back(&Path); }
	decltype(list)::iterator begin() { return list.begin(); }
	decltype(list)::iterator end() { return list.end(); }

	bool shouldUpdate() {
		for (WatchedPath* path : list)
			if (path->shouldUpdate()) return true;
	}
	bool shouldUpdateWillUpdate() {
		bool shouldUpdate = false;
		for (WatchedPath* path : list) {
			time_t timeNow = path->getLastUpdate();
			if (timeNow != path->lastUpdate) {
				path->update(shouldUpdate);
				shouldUpdate = true;
			}
		}
		return shouldUpdate;
	}
	void update() {
		for (WatchedPath* path : list)
			path->update();
	}
	void update(time_t Time) {
		for (WatchedPath* path : list)
			path->update(Time);
	}
};
std::string onixPath;
WatchedPath serverPath;
WatchedPath gameModePath;
WatchedPath usernamePath;
WatchedPathList filePaths;

void InitPaths() {
	char appdatapath[MAX_PATH];
	GetEnvironmentVariableA("LOCALAPPDATA", appdatapath, MAX_PATH);
	onixPath = appdatapath + std::string("\\Packages\\Microsoft.MinecraftUWP_8wekyb3d8bbwe\\RoamingState\\OnixClient\\");
	serverPath = onixPath + "Launcher\\";
	if (std::filesystem::exists(onixPath))
		std::filesystem::create_directories(*serverPath);
	serverPath += "server.txt";

	gameModePath = onixPath + "Scripts\\Data\\RPC\\RPCHelperGamemode.txt";
	usernamePath = onixPath + "Scripts\\Data\\RPC\\RPCHelperUsername.txt";

	filePaths.add(serverPath);
	filePaths.add(usernamePath);
	filePaths.add(gameModePath);
}

const char* ServersJson =
R"({"list":[
	{
	  "name":"The Hive",
	  "id":"992501331921211615",
	  "ipContains":["hivebedrock","217.182.250.29","141.94.219.63","57.128.155.56","141.94.219.61","198.244.230.2","51.161.47.73","51.161.47.115","15.235.65.155","51.222.228.185","15.235.42.148","51.79.234.178","51.79.229.192","51.79.228.14","139.99.17.123","51.79.229.174","51.79.229.174"]
	},
	{
	  "name":"Galaxite",
	  "id":"991628139463704647",
	  "ipContains":["galaxite"]
	},
	{
	  "name":"CubeCraft",
	  "id":"992505095851888731",
	  "ipContains":["cubecraft"]
	},
	{
	  "name":"Zeqa",
	  "id":"1026198254410543195",
	  "ipContains":["zeqa.net","51.222.245.157","209.192.254.244","135.148.137.23","51.222.245.195","51.222.10.85","51.222.10.129","51.210.223.196","164.132.200.60","141.94.98.43","51.219.223.195","51.79.163.180"]
	}
]})";

struct MinecraftServer {
	std::string name;
	std::string applicationId;
	std::vector<std::string> ips;
};
MinecraftServer noServerRPC = { "Minecraft", "992508263780335697" };
std::vector<MinecraftServer> servers;

void InitServers() {
	json::json serverListRoot_top = json::json::parse(ServersJson);
	auto serverListRoot = serverListRoot_top.find("list");
	if (serverListRoot == serverListRoot_top.end()) return;

	for (json::json& serverRoot : *serverListRoot) {
		MinecraftServer newServer;
		auto nameI = serverRoot.find("name");
		if (nameI == serverRoot.end() || !nameI->is_string())
			continue;
		newServer.name = nameI->get<std::string>();

		auto idI = serverRoot.find("id");
		if (idI == serverRoot.end() || !idI->is_string())
			continue;
		newServer.applicationId = idI->get<std::string>();

		auto ipContainI = serverRoot.find("ipContains");
		if (ipContainI == serverRoot.end() || !ipContainI->is_array())
			continue;
		for (json::json& ipContain : *ipContainI) {
			if (ipContain.is_string())
				newServer.ips.push_back(ipContain.get<std::string>());
		}

		servers.emplace_back(newServer);
	}
}

bool IsMinecraftOpenedNow = false;
DWORD TrackMinecraftOpenState(LPVOID) {
	while (true) {
		if (IsMinecraftOpenedNow = isMinecraftOpen()) {
			DWORD* PidList = new DWORD[4096];
			DWORD damn = 0;
			if (!K32EnumProcesses(PidList, 4096 * sizeof(DWORD), &damn)) {
				delete[] PidList;
				return 0;
			}
			damn = min((int)damn, 4096);

			for (DWORD i = 0; i < damn; i++) {
				HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, FALSE, PidList[i]);
				if (hProcess) {
					DWORD cbNeeded;
					HMODULE mod;
					if (!K32EnumProcessModules(hProcess, &mod, sizeof(HMODULE), &cbNeeded)) continue;
					char textualContent[128];
					K32GetModuleBaseNameA(hProcess, mod, textualContent, sizeof(textualContent) / sizeof(textualContent[0]));
					if (!strcmp(textualContent, "Minecraft.Windows.exe")) {
						WaitForSingleObject(hProcess, INFINITE);
						CloseHandle(hProcess);
						IsMinecraftOpenedNow = false;
						break;
					}
					else
						CloseHandle(hProcess);
				}
			}
			delete[] PidList;
		}
		else {
			while (!isMinecraftOpen_Slow(1)) Sleep(200);
			IsMinecraftOpenedNow = true;
		}
	}
	return 0;
}

std::vector <std::pair<std::string, std::string>> formattedHiveGamemodes = {
	{"Block Drop",	    "hive_drop"},
	{"Capture The Flag","hive_ctf"},
	{"The Bridge",      "hive_bridge"},
	{"Ground Wars",     "hive_ground"},
	{"Survival Games",  "hive_sg"},
	{"Murder Mystery",  "hive_murder"},
	{"Treasure Wars",   "hive_wars"},
	{"Skywars",         "hive_sky"},
	{"Skywars Kits",    "hive_sky-kits"},
	{"Just Build",      "hive_build"},
	{"Hide And Seek",   "hive_hide"},
	{"Death Run",       "hive_dr"},
	{"Hub",             "hive_hub"},
	{"Replay",			"hive_replay"},
	{"Block Party",		"hive_party"},
	{"Gravity",			"hive_grav"},
};

std::vector <std::pair<std::string, std::string>> formattedZeqaGamemodes = {
	{"Archer",		 "zeqa_archer"},
	{"BattleRush",	 "zeqa_battlerush"},
	{"BedFight",	 "zeqa_bedfight"},
	{"Boxing",		 "zeqa_boxing"},
	{"Bridge",		 "zeqa_bridge"},
	{"BuildUHC",	 "zeqa_build"},
	{"Classic",		 "zeqa_classicduels"},
	{"Combo",		 "zeqa_combo"},
	{"FinalUHC",	 "zeqa_finaluhc"},
	{"FireballFight","zeqa_fireballfight"},
	{"Fist",		 "zeqa_fist"},
	{"Gapple",		 "zeqa_gapple"},
	{"MLGRush",		 "zeqa_mlgrush"},
	{"Nodebuff",	 "zeqa_nodebuff"},
	{"PearlFight",	 "zeqa_pearlfight"},
	{"Skywars",		 "zeqa_skywars"},
	{"Soup",		 "zeqa_soup"},
	{"Spleef",		 "zeqa_spleef"},
	{"StickFight",	 "zeqa_stickfight"},
	{"Sumo",		 "zeqa_sumo"},
};

std::string getUglyGamemodeName(const std::string& server, const std::string& prettyString) {
	std::vector <std::pair<std::string, std::string>>* vec = nullptr;
	if (server.find("The Hive") != std::string::npos)
		vec = &formattedHiveGamemodes;
	else if (server.find("Zeqa") != std::string::npos)
	vec = &formattedZeqaGamemodes;
	if (vec == nullptr) return {};
	for (const auto& [formatted, unformatted] : *vec) {
		if (prettyString.find(formatted) != std::string::npos)
			return unformatted;
	}
	return {};
}

std::string getPrettyGamemodeName(const std::string& server, const std::string& prettyString) {
	std::vector <std::pair<std::string, std::string>>* vec = nullptr;
	if (server.find("The Hive") != std::string::npos)
		vec = &formattedHiveGamemodes;
	else if (server.find("Zeqa") != std::string::npos)
		vec = &formattedZeqaGamemodes;
	if (vec == nullptr) return {};
	for (const auto& [formatted, unformatted] : *vec) {
		if (prettyString.find(formatted) != std::string::npos)
			return formatted;
	}
	return {};
}

DWORD RpcThreadFuntion(LPVOID) {
	InitPaths();
	InitServers();

	MinecraftServer* mcserver = &noServerRPC;
	bool wasMinecraftStarted = false;
	bool wasRpcEnabled = false;
	bool forceReUpdate = false;

	IsMinecraftOpenedNow = isMinecraftOpen();
	minecraftStateThread = CreateThread(0, 0, TrackMinecraftOpenState, 0, 0, 0);


	while (isApplicationWantingToLive) {
		if (wasRpcEnabled != richPresenceEnabled) {
			wasRpcEnabled = richPresenceEnabled;
			forceReUpdate = false;
			if (!richPresenceEnabled)
				Discord::clean();
			else {
				mcserver = nullptr;
				forceReUpdate = true;
			}
		}
		if (!richPresenceEnabled) {
			Sleep(500);
			continue;
		}

		if (IsMinecraftOpenedNow != wasMinecraftStarted) {
			wasMinecraftStarted = IsMinecraftOpenedNow;
			if (IsMinecraftOpenedNow) {
				mcserver = &noServerRPC;
				Discord::restart(mcserver->applicationId);
				Discord::update("In the menus", usernamePath.getContent());
			}
			else {
				Discord::clean();
			}

		}
		if (IsMinecraftOpenedNow && (filePaths.shouldUpdateWillUpdate() || forceReUpdate)) {
			forceReUpdate = false;
			std::string serverIp = serverPath.getContent();
			std::string gamingMode = gameModePath.getContent();
			std::string usernameMode = usernamePath.getContent();
			MinecraftServer* newServer = nullptr;

			if (serverIp.empty())
				newServer = &noServerRPC;
			else
				for (int servindex = 0; servindex < (int)servers.size(); servindex++) {
					MinecraftServer* serv = &servers.at(servindex);
					for (std::string& ip : serv->ips)
						if (strstr(serverIp.c_str(), ip.c_str()) != nullptr) {
							newServer = serv;
							goto loopExit;
						}
				}
		loopExit:
			if (!newServer) //that server isn't in our presets
				newServer = &noServerRPC;

			if (!mcserver || (mcserver->name != newServer->name || mcserver->applicationId != newServer->applicationId)) {
				Discord::setStartTime(time(0));
				Discord::restart(newServer->applicationId);
			}
			mcserver = newServer;

			if (mcserver->name == noServerRPC.name && mcserver->applicationId == noServerRPC.applicationId)
				Discord::update(serverIp.empty() ? "In the menus" : strstr(serverIp.c_str(), "n a World") != nullptr ? "In a World" : "On a Server", usernameMode);
			else {
				Discord::update(gamingMode, usernameMode, getUglyGamemodeName(mcserver->name, gamingMode), getPrettyGamemodeName(mcserver->name, gamingMode));
			}
		}
		Sleep(1000);
		forceReUpdate = false;
	}
	Discord::clean();
	return 0;
}

#define WM_TRAYICON (WM_USER + 1)
NOTIFYICONDATA nid;
bool isWindowVisible = true;

HICON hCustomIcon;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			hCustomIcon = (HICON)LoadImage(NULL, L"MinecraftRPCBadIcon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);

			if (hCustomIcon == NULL) {
				MessageBox(NULL, L"Failed to load custom icon!", L"Error", MB_ICONERROR);
				return -1;
			}

			nid.cbSize = sizeof(NOTIFYICONDATA);
			nid.hWnd = hwnd;
			nid.uID = 1;
			nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
			nid.uCallbackMessage = WM_TRAYICON;
			nid.hIcon = hCustomIcon;
			lstrcpy(nid.szTip, TEXT("MCBE RPC"));

			Shell_NotifyIcon(NIM_ADD, &nid);
			break;
		}

		case WM_TRAYICON: {
			if (lParam == WM_RBUTTONUP) {
				HMENU hMenu = CreatePopupMenu();
				//AppendMenu(hMenu, MF_STRING, 1, isWindowVisible ? TEXT("Hide") : TEXT("Show"));
				AppendMenu(hMenu, MF_STRING, 1, TEXT("Exit"));

				POINT pt;
				GetCursorPos(&pt);

				SetForegroundWindow(hwnd);
				TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
				DestroyMenu(hMenu);
			}
			break;
		}

		case WM_COMMAND: {
			//if (wParam == 1) {
			//	if (isWindowVisible) {
			//		ShowWindow(hwnd, SW_HIDE);
			//	}
			//	else {
			//		ShowWindow(hwnd, SW_SHOWNORMAL);
			//	}
			//	isWindowVisible = !isWindowVisible;
			//}
			/*else*/ if (wParam == 1) {
				Shell_NotifyIcon(NIM_DELETE, &nid);
				isApplicationWantingToLive = false;
				TerminateProcess(GetCurrentProcess(), 0);
			}
			break;
		}

		case WM_DESTROY: {
			Shell_NotifyIcon(NIM_DELETE, &nid);

			isApplicationWantingToLive = false;
			DestroyIcon(hCustomIcon);
			TerminateProcess(GetCurrentProcess(), 0);
			break;
		}

		default: {
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
	}
	return 0;
}

//int main() {
int WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR args, int nCmdShow) {
	discordRpcThread = CreateThread(0, 0, RpcThreadFuntion, 0, 0, 0);

	// Declare a window class
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = TEXT("MCRPCClass");

	// Register the window class
	RegisterClass(&wc);

	// Create a window
	HWND hwnd = CreateWindow(TEXT("MCRPCClass"), TEXT("MCBE RPC"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 260, 80, NULL, NULL, hInst, NULL);
	//RECT windowClientArea;
	//GetClientRect(hwnd, &windowClientArea);
	//HWND checkboxHandle = CreateWindowExA(
	//	0, "button", "Enable RPC",
	//	WS_VISIBLE | WS_CHILD | BS_CHECKBOX,
	//	0, 0,
	//	windowClientArea.right - windowClientArea.left,
	//	windowClientArea.bottom - windowClientArea.top,
	//	hwnd,
	//	(HMENU)1,
	//	hInst,
	//	NULL
	//);
	//
	//// Set the checkbox state to checked
	//CheckDlgButton(hwnd, 1, BST_CHECKED);
	//
	//// Set up a custom message handler for the checkbox
	//static WNDPROC oldCheckboxWindowProcedure = (WNDPROC)SetWindowLongPtrA(checkboxHandle, GWLP_WNDPROC, (LONG_PTR)(WNDPROC)[](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) -> LRESULT {
	//	if (msg == WM_LBUTTONDOWN) {
	//		CheckDlgButton(GetParent(hwnd), (int)(size_t)GetMenu(hwnd), (bool)IsDlgButtonChecked(GetParent(hwnd), (int)(size_t)GetMenu(hwnd)) ? BST_UNCHECKED : BST_CHECKED);
	//		richPresenceEnabled = (bool)IsDlgButtonChecked(GetParent(hwnd), (int)(size_t)GetMenu(hwnd));
	//	}
	//	return CallWindowProc(oldCheckboxWindowProcedure, hwnd, msg, wparam, lparam);
	//	});
	//// Show the window
	ShowWindow(hwnd, 0);
	MSG msg;
	while (isApplicationWantingToLive) {
		while (PeekMessageA(&msg, 0, 0, 0, 1)) {
			if (msg.message == WM_CLOSE) TerminateProcess(GetCurrentProcess(), 0);
			if (msg.message == WM_QUIT)  break;
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
		Sleep(200);
	}

	TerminateThread(minecraftStateThread, 0);
	if (WaitForSingleObject(discordRpcThread, 5000) == WAIT_TIMEOUT)
		TerminateThread(discordRpcThread, 0);
	CloseHandle(minecraftStateThread);
	CloseHandle(discordRpcThread);
	isApplicationWantingToLive = false;
	return (int)msg.wParam;
}
