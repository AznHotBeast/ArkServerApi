#include "Teleport.h"
#include "TeleportConfig.h"
#include "TeleportCommands.h"
#include "TeleportHooks.h"
#pragma comment(lib, "ArkApi.lib")

void Init()
{
	Log::Get().Init("Teleport");
	if (InitConfig())
	{
		InitCommands();
		InitHooks();
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Init();
		break;
	case DLL_PROCESS_DETACH:
		RemoveCommands();
		RemoveHooks();
		break;
	}
	return TRUE;
}