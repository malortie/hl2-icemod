//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_NPC_DRAGON_H
#define C_NPC_DRAGON_H
#ifdef _WIN32
#pragma once
#endif


#include "c_ai_basenpc.h"
//for dlight
#include "dlight.h"
#include "iefx.h"

class C_Npc_Dragon : public C_AI_BaseNPC
{
public:
	C_Npc_Dragon() { }

	DECLARE_CLASS( C_Npc_Dragon, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();
	DECLARE_DATADESC();

	virtual void OnDataChanged( DataUpdateType_t type );
	virtual void ClientThink(); // for dlight

	float SkinIllume() const { return m_flSkinIllume; }
	int DragonSkinColor() const { return m_iSkinColor; }
	bool IsTorching() const { return m_bIsTorching; }

private:

	color32			m_cFlameColor;
	color32			m_cSkinColor;
	color32			m_cCurrentColor;
	float			m_iTargetRadius;

	C_Npc_Dragon( const C_Npc_Dragon &other ) { }

	float m_flSkinIllume;
	bool m_bIsTorching;
	int	m_iSkinColor;

	dlight_t *m_dlight;
};


#endif // C_NPC_DRAGON_H