#pragma once
#include "../../SDK/SDK.h"

class CNoSpread
{
private:
	bool ShouldRun(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, CUserCmd* pCmd);

public:
	void Run(CUserCmd* pCmd);
};

ADD_FEATURE(CNoSpread, NoSpread)