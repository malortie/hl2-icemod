//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "NPCevent.h"
#include "basecombatcharacter.h"
#include "AI_BaseNPC.h"
#include "player.h"
#include "game.h"
#include "in_buttons.h"
#include "grenade_ar2.h"
#include "AI_Memory.h"
#include "soundent.h"
#include "rumble_shared.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sk_weapon_machpistol_max_burst( "sk_weapon_machpistol_max_burst", "4" );

class CWeaponMachPistol : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CWeaponMachPistol, CBaseHLCombatWeapon );

	CWeaponMachPistol();

	DECLARE_SERVERCLASS();
	
	void	AddViewKick( void );
	void	Precache( void );

	void	PrimaryAttack( void );
	void	FireWeapon( void );
	void	SecondaryAttack( void );
	void	ItemPostFrame( void );
	bool	CanHolster( void );
	bool	Reload( void );

	virtual void Equip( CBaseCombatCharacter *pOwner );
	float	GetFireRate( void ) { return 0.125f; }	// 13.3hz 0.075f
	float	GetFireRateBurst( void ) { return 0.055f; }
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_3DEGREES;
		return cone;
	}

	const WeaponProficiencyInfo_t *GetProficiencyValues();

	DECLARE_ACTTABLE();

protected:
	bool	m_bFireAllowed;
	bool	m_bFireMode;	// ICETODO declared as a bool, but I later used it as an INT.. watch for bugs!
	int		m_nBurstRound;

};

IMPLEMENT_SERVERCLASS_ST(CWeaponMachPistol, DT_WeaponMachPistol)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_machpistol, CWeaponMachPistol );
PRECACHE_WEAPON_REGISTER(weapon_machpistol);

BEGIN_DATADESC( CWeaponMachPistol )

	DEFINE_FIELD( m_bFireAllowed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nBurstRound, FIELD_INTEGER ),
	DEFINE_FIELD( m_bFireMode, FIELD_BOOLEAN ),

END_DATADESC()

acttable_t	CWeaponMachPistol::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SMG1,			true },
	{ ACT_RELOAD,					ACT_RELOAD_SMG1,				true },
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SMG1,			true },

	{ ACT_WALK,						ACT_WALK_RIFLE,					true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true  },
	
// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SMG1_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SMG1_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
//End readiness activities

	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
	{ ACT_RUN,						ACT_RUN_RIFLE,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SMG1,	true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SMG1_LOW,		true },
	{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_SMG1_LOW,			false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG1_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,		true },
};

IMPLEMENT_ACTTABLE(CWeaponMachPistol);

//=========================================================
CWeaponMachPistol::CWeaponMachPistol( )
{
	m_fMinRange1		= 0;// No minimum range. 
	m_fMaxRange1		= 1400;

	m_bFireAllowed		= true;
	m_bFireMode			= 0; // 0 = Auto, 1 = Burst
	m_nBurstRound		= 0; // current shot, for burst fire

	m_bAltFiresUnderwater = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMachPistol::Precache( void )
{
	PrecacheScriptSound( "NPC_Citizen.FootstepLeft" );
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Give this weapon longer range when wielded by an ally NPC.
//-----------------------------------------------------------------------------
void CWeaponMachPistol::Equip( CBaseCombatCharacter *pOwner )
{
	if( pOwner->Classify() == CLASS_PLAYER_ALLY )
	{
		m_fMaxRange1 = 3000;
	}
	else
	{
		m_fMaxRange1 = 1400;
	}

	BaseClass::Equip( pOwner );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponMachPistol::PrimaryAttack( void )
{
	if (!m_bFireAllowed)
		return;
	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		if(m_nBurstRound != 0)
		{
			m_nBurstRound = 0; // ICETODO check to see if pressing "r" to reload, fires off the remaining burst after.. if it does, stick this in RELOAD function
		}
		Reload();
		return;
	}

	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if( pPlayer )
	{
		// Each time the player fires the pistol, reset the view punch. This prevents
		// the aim from 'drifting off' when the player fires very quickly. This may
		// not be the ideal way to achieve this, but it's cheap and it works, which is
		// great for a feature we're evaluating. (sjb)
		pPlayer->ViewPunchReset();
	} else {
		return;
	}

	if(m_bFireMode == 1){
		m_nBurstRound++;
	}

	FireWeapon();
}

void CWeaponMachPistol::FireWeapon( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	FireBulletsInfo_t info;
	info.m_vecSrc	 = pPlayer->Weapon_ShootPosition( );
	
	info.m_vecDirShooting = pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );

	// To make the firing framerate independent, we may have to fire more than one bullet here on low-framerate systems, 
	// especially if the weapon we're firing has a really fast rate of fire.
	info.m_iShots = 0;

	float fireRate = 0;
	if(m_bFireMode == 0)
		fireRate = GetFireRate();
	else
		fireRate = GetFireRateBurst();

	while ( m_flNextPrimaryAttack <= gpGlobals->curtime )
	{
		// MUST call sound before removing a round from the clip of a CMachineGun
		WeaponSound(SINGLE, m_flNextPrimaryAttack);
		m_flNextPrimaryAttack = m_flNextPrimaryAttack + fireRate;
		info.m_iShots++;
		if ( !fireRate )
			break;
	}

	// Make sure we don't fire more than the amount in the clip
	if ( UsesClipsForAmmo1() )
	{
		info.m_iShots = min( info.m_iShots, m_iClip1 );
		m_iClip1 -= info.m_iShots;
	}
	else
	{
		info.m_iShots = min( info.m_iShots, pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) );
		pPlayer->RemoveAmmo( info.m_iShots, m_iPrimaryAmmoType );
	}

	info.m_flDistance = MAX_TRACE_LENGTH;
	info.m_iAmmoType = m_iPrimaryAmmoType;
	info.m_iTracerFreq = 2;

#if !defined( CLIENT_DLL )
	// Fire the bullets
	info.m_vecSpread = pPlayer->GetAttackSpread( this );
#else
	//!!!HACKHACK - what does the client want this function for? 
	info.m_vecSpread = GetActiveWeapon()->GetBulletSpread();
#endif // CLIENT_DLL

	pPlayer->FireBullets( info );

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}

	//Add our view kick in
	AddViewKick();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponMachPistol::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
	{
		if(m_nBurstRound != 0)
		{
			m_nBurstRound = 0;
		}
		return;
	}
	
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	if (m_bFireMode == 1)
	{
		if((m_nBurstRound > 0) && (m_nBurstRound < sk_weapon_machpistol_max_burst.GetInt()))
		{
			if(m_flNextPrimaryAttack < gpGlobals->curtime)
				PrimaryAttack();
			else return;
		} 
		else
		{
			if(m_nBurstRound != 0)
			{
				m_nBurstRound = 0;
				m_bFireAllowed = false;
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + .2f;
			}

			// -----------------------
			//  No buttons down
			// -----------------------
			if (!((pOwner->m_nButtons & IN_ATTACK) || (pOwner->m_nButtons & IN_ATTACK2) || (pOwner->m_nButtons & IN_RELOAD)))
			{
				// no fire buttons down or reloading
				if ( !ReloadOrSwitchWeapons() && ( m_bInReload == false ) )
				{
					if(!m_bFireAllowed)
						m_bFireAllowed = true;
				}
			}

			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMachPistol::SecondaryAttack( void )
{
	// switch the fire mode
	if(m_bFireMode == 0)
		m_bFireMode = 1;
	else
		m_bFireMode = 0;

	SendWeaponAnim( ACT_VM_FIDGET );
	WeaponSound( SPECIAL1 );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime  + .5f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponMachPistol::Reload( void )
{
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );

	if ( pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
		return false;
	
	WeaponSound( RELOAD );
	return DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMachPistol::AddViewKick( void )
{
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	//Disorient the player like 357 but not so heavy
	QAngle angles = pPlayer->GetLocalAngles();

	angles.x += random->RandomFloat( -.2, .2 );
	angles.y += random->RandomFloat( -.2, .2 );
	angles.z = 0;

	pPlayer->SnapEyeAngles( angles );

	pPlayer->ViewPunch( QAngle( 0, random->RandomFloat( -.02, .02 ), 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Override if we're waiting to release a shot
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponMachPistol::CanHolster( void )
{
	if ( m_bFireMode == 1 )
	{
		if(m_nBurstRound != 0)
			m_nBurstRound = 0;
	}

	return BaseClass::CanHolster();
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CWeaponMachPistol::GetProficiencyValues()
{
	static WeaponProficiencyInfo_t proficiencyTable[] =
	{
		{ 7.0,		0.75	},
		{ 5.00,		0.75	},
		{ 10.0/3.0, 0.75	},
		{ 5.0/3.0,	0.75	},
		{ 1.00,		1.0		},
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return proficiencyTable;
}
