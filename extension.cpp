#include "extension.h"

SVParallelSendSnapshotHack  g_pssh;		/**< Global singleton for extension's main interface */
SMEXT_LINK(&g_pssh);


//memory addresses below 0x10000 are automatically considered invalid for dereferencing
//this is copied over from smn_core.cpp
#define VALID_MINIMUM_MEMORY_ADDRESS 0x10000

DETOUR_DECL_MEMBER1(CBaseClient__IgnoreTempEntity, bool, void*, event)
{
/*
    bool CBaseClient::IgnoreTempEntity( CEventInfo *event )
    {
        int iPlayerIndex = GetPlayerSlot()+1;

        return !event->filter.IncludesPlayer( iPlayerIndex );
    }


    char __thiscall CBaseClient::IgnoreTempEntity(void *this, int a2)
    {
      int v3; // edi
      // (*(int (__thiscall **)(void *))(*(int *)this + 12))(this)
      v3 = 0;
      if ( (*(int (__thiscall **)(void *))(*(_DWORD *)this + 12))(this) <= 0 )
        return 0;
      while ( a2 != (*(int (__thiscall **)(void *, int))(*(_DWORD *)this + 16))(this, v3) )
      {
        if ( ++v3 >= (*(int (__thiscall **)(void *))(*(_DWORD *)this + 12))(this) )
          return 0;
      }
      return 1;
    }
*/
    uintptr_t _this = reinterpret_cast<uintptr_t>(this);

    // HACK since somehow `this` keeps ending up as 0x1c
    // definitely threading shennanigans...
    if ( !_this || _this <= VALID_MINIMUM_MEMORY_ADDRESS )
    {
        return false;
    }
    // GetPlayerSlot is a function that gets called from the this ptr
    // It would look like this in code
    // auto offs = (*(uint32_t*)this + 12);
    // int slot = ( (*(int(__thiscall**)(void*)) offs )(this)) + 1;
    // Luckily, it just is a simple dereference of a member var
    // and doesn't do anything fancy,
    // so we can just directly do that instead
    // mov eax,dword ptr [ecx+14h]
    int slot = *( reinterpret_cast<int*>( _this + 0x14 ) );
    slot++;
    if (slot <= 0)
    {
        return false;
    }
    // Yes, the slot logic will rerun - that's ok.
    // Tiny amount of code, and computers are very fast.
    bool ret = DETOUR_MEMBER_CALL(CBaseClient__IgnoreTempEntity)(event);
    return ret;
}

CDetour* Detour_CBaseClient__IgnoreTempEntity = nullptr;

bool SVParallelSendSnapshotHack::SDK_OnLoad(char* error, size_t maxlen, bool late)
{
    IGameConfig* pGameConfig = nullptr;

    if (!gameconfs->LoadGameConfigFile("pssh", &pGameConfig, error, maxlen))
    {
        smutils->Format(error, maxlen - 1, "Failed to load gamedata");
        return false;
    }

    CDetourManager::Init(g_pSM->GetScriptingEngine(), pGameConfig);

    Detour_CBaseClient__IgnoreTempEntity = DETOUR_CREATE_MEMBER(CBaseClient__IgnoreTempEntity, "CBaseClient__IgnoreTempEntity");

    if (!Detour_CBaseClient__IgnoreTempEntity)
    {
        smutils->Format(error, maxlen - 1, "Failed to create detour for CBaseClient__IgnoreTempEntity");
        return false;
    }

    Detour_CBaseClient__IgnoreTempEntity->EnableDetour();

    return true;
}

void SVParallelSendSnapshotHack::SDK_OnUnload()
{
    if (Detour_CBaseClient__IgnoreTempEntity)
    {
        Detour_CBaseClient__IgnoreTempEntity->Destroy();
        Detour_CBaseClient__IgnoreTempEntity = nullptr;
    }
    return;
}