#include "../Hooks.h"

MAKE_HOOK(EngineClient_IsPlayingTimeDemo, Utils::GetVFuncPtr(I::EngineClient, 78), bool, __fastcall,
		  void* ecx, void* edx)
{
	/* [implement] really get rid of
	static DWORD dwInterpolateServerEntities = g_Pattern.Find(L"client.dll", L"55 8B EC 83 EC 30 8B 0D ? ? ? ? 53 33 DB 89 5D DC 89 5D E0");

	if (G::Recharging)
	{
		if (reinterpret_cast<DWORD>(_ReturnAddress()) == (dwInterpolateServerEntities + 0xB8))
		{
			return true;
		}
	}
	*/

	return Hook.Original<FN>()(ecx, edx);
}