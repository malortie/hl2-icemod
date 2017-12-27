//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef WEAPON_TROPHY_H
#define WEAPON_TROPHY_H

#include "basebludgeonweapon.h"

#if defined( _WIN32 )
#pragma once
#endif

#define	TROPHY_RANGE	75.0f
#define	TROPHY_REFIRE	0.4f

//-----------------------------------------------------------------------------
// CWeaponTrophy
//-----------------------------------------------------------------------------

class CWeaponTrophy : public CBaseHLBludgeonWeapon
{
	DECLARE_CLASS( CWeaponTrophy, CBaseHLBludgeonWeapon );
	DECLARE_DATADESC();
public:

	DECLARE_SERVERCLASS();
	DECLARE_ACTTABLE();

	CWeaponTrophy();

	void Precache( void ); // for particle
	bool Holster( CBaseCombatWeapon *pSwitchingTo ); // for particle remove
	bool Deploy( void ); // fire the particle

	float		GetRange( void )		{	return	TROPHY_RANGE;	}
	float		GetFireRate( void )		{	return	TROPHY_REFIRE;	}

	//void	PrimaryAttack( void );
	void	SecondaryAttack( void );

	void		AddViewKick( void );
	float		GetDamageForActivity( Activity hitActivity );

	virtual int WeaponMeleeAttack1Condition( float flDot, float flDist );

	// Animation event
	virtual void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	bool ShouldDisplayAltFireHUDHint() { return true; }

private:
	// Animation event handlers
	void HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	bool	m_bFirstDeploy;

};

#endif // WEAPON_TROPHY_H