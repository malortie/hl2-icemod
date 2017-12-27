//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Snack Food pickups, restore a little health
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "gamerules.h"
#include "player.h"
#include "items.h"
#include "in_buttons.h"
#include "engine/IEngineSound.h"

#include "gib.h" // for peanutbutter lid pop-off

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
		
ConVar	sk_snackfood( "sk_snackfood","0" );	

//-----------------------------------------------------------------------------
// Snack Food. Heals the player when picked up.
//-----------------------------------------------------------------------------
class CSnackFood : public CItem
{
public:
	DECLARE_CLASS( CSnackFood, CItem );
	DECLARE_DATADESC();

	CSnackFood(void);

	void Spawn( void );
	void Precache( void );
	bool CreateVPhysics(void);

	void SetSnackType(int snackType);

	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

private:
	int m_iSnackType;
	bool m_bIsEmpty;
};

LINK_ENTITY_TO_CLASS( item_snackfood, CSnackFood );
PRECACHE_REGISTER(item_snackfood);

BEGIN_DATADESC( CSnackFood )

	DEFINE_FIELD( m_bIsEmpty, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iSnackType, FIELD_INTEGER ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSnackFood::CSnackFood( void )
{
	m_bIsEmpty = false;
	m_iSnackType = 2;
}

void CSnackFood::SetSnackType(int snackType)
{
	m_iSnackType = snackType;
}

void CSnackFood::Spawn( void )
{
	const char *pModelName = STRING( GetModelName() );

	if( !Q_stricmp(pModelName, "models/items/cheese.mdl") )
	{
		SetSnackType(0); //cheese
	}
	else if( !Q_stricmp(pModelName, "models/items/jerky.mdl") )
	{
		SetSnackType(1); //jerky
	}
	else if( !Q_stricmp(pModelName, "models/items/peanutbutter.mdl") )
	{
		SetSnackType(2); //peanut butter
	}
	else if( !GetModelName() ) // fuk it, its cheese
	{
		SetSnackType(0);
	}

	Precache();

	// make physics
	CreateVPhysics();

	SetModel( STRING( GetModelName() ) );

	m_bIsEmpty = false;

	BaseClass::Spawn();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSnackFood::Precache( void )
{
	PrecacheModel( STRING( GetModelName() ) );
	m_bIsEmpty = false;

	//const char *pModelName = STRING( GetModelName() );
	//Warning("model: %s\n", STRING( GetModelName() ));

	if( m_iSnackType == 0 )
	{
		PrecacheScriptSound( "SnackFood.Cheese" );
	}
	else if( m_iSnackType == 1 )
	{
		PrecacheScriptSound( "SnackFood.Jerky" );
	}
	else if( m_iSnackType == 2 )
	{
		PrecacheScriptSound( "SnackFood.PeanutButter" );
		// precache the lid
		PrecacheModel( "models/items/peanutbutter_lid.mdl" );
	}
	else if( m_iSnackType >= 3 || m_iSnackType < 0) // fuk it, its cheese
	{
		SetSnackType(0);
		SetModelName( MAKE_STRING( "models/Items/cheese.mdl" ) );
		PrecacheScriptSound( "SnackFood.Cheese" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSnackFood::CreateVPhysics(void)
{
	VPhysicsInitNormal(SOLID_VPHYSICS, 0, false);
	return true;
}

void CSnackFood::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// if it's not a player, ignore
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	if ( !pPlayer )
		return;

	if( !pPlayer->m_bPlayerWearingHelmet)
	{
		if(m_bIsEmpty == false)
		{
			if ( pPlayer->TakeHealth( sk_snackfood.GetFloat(), DMG_GENERIC ) )
			{
				CSingleUserRecipientFilter user( pPlayer );
				user.MakeReliable();

				UserMessageBegin( user, "ItemPickup" );
					WRITE_STRING( GetClassname() );
				MessageEnd();

				if(m_iSnackType == 0)
				{
					CPASAttenuationFilter filter( pPlayer, "SnackFood.Cheese" );
					EmitSound( filter, pPlayer->entindex(), "SnackFood.Cheese" );
				}
				else if (m_iSnackType == 1)
				{
					CPASAttenuationFilter filter( pPlayer, "SnackFood.Jerky" );
					EmitSound( filter, pPlayer->entindex(), "SnackFood.Jerky" );
				}
				else if(m_iSnackType == 2)
				{
					CPASAttenuationFilter filter( pPlayer, "SnackFood.PeanutButter" );
					EmitSound( filter, pPlayer->entindex(), "SnackFood.PeanutButter" );
					// pop the lid
					CGib::SpawnSpecificGibs( this, 1, 100, 200, "models/Items/peanutbutter_lid.mdl", 5 );
				}
				else if(m_iSnackType >= 3)
				{
					CPASAttenuationFilter filter( pPlayer, "SnackFood.Cheese" );
					EmitSound( filter, pPlayer->entindex(), "SnackFood.Cheese" );
				}

				//UTIL_Remove(this);
				static int BodyGroup_Empty = FindBodygroupByName("Body");
				SetBodygroup(BodyGroup_Empty, 1);
				m_bIsEmpty = true;
			}
		}
	}
	else
	{
		// player has helmet on
		// code below works
		//pPlayer->SetHelmetBloodColor(2);
		//pPlayer->SetHelmetBloodAlpha(2);
	}

	// calling the BaseClass USE will allow us to lift and hold the item
	BaseClass::Use(pActivator, pCaller, useType, value);
}

