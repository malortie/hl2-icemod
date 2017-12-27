//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A apershotgun.
//
//			Primary attack: single barrel shot.
//			Secondary attack: double barrel shot.
//
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "basehlcombatweapon_shared.h"
#include "basecombatcharacter.h"
#include "AI_BaseNPC.h"
#include "player.h"
#include "gamerules.h"		// For g_pGameRules
#include "in_buttons.h"
#include "grenade_ar2.h"	// ICE for grenade
#include "soundent.h"
#include "vstdlib/random.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sk_auto_reload_time;
extern ConVar sk_plr_num_shotgun_pellets;
extern ConVar sk_plr_dmg_smg1_grenade;

class CWeaponAperShotgun : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CWeaponAperShotgun, CBaseHLCombatWeapon );

	DECLARE_SERVERCLASS();

private:
	bool	m_bDelayedFire1;	// Fire primary when finished reloading
	bool	m_bDelayedFire2;	// Fire secondary when finished reloading
	float	m_fNextShotTime;	// ICE the least amount of time before you can shoot again
	bool	m_bReleasedFire;	// ICE you have to release the fire button to fire again

public:
	void	Precache( void );

	int CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector vitalAllyCone = VECTOR_CONE_3DEGREES;
		static Vector cone = VECTOR_CONE_10DEGREES;

		if( GetOwner() && (GetOwner()->Classify() == CLASS_PLAYER_ALLY_VITAL) )
		{
			// Give Alyx's apershotgun blasts more a more directed punch. She needs
			// to be at least as deadly as she would be with her pistol to stay interesting (sjb)
			return vitalAllyCone;
		}

		return cone;
	}

	virtual int				GetMinBurst() { return 1; }
	virtual int				GetMaxBurst() { return 3; }

	virtual float			GetMinRestTime();
	virtual float			GetMaxRestTime();

	virtual float			GetFireRate( void );

	bool Reload( void );
	void CheckHolsterReload( void );
	void ItemHolsterFrame( void );
	void ItemPostFrame( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void DryFire( void );

	void FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	DECLARE_ACTTABLE();

	CWeaponAperShotgun(void);
};

IMPLEMENT_SERVERCLASS_ST(CWeaponAperShotgun, DT_WeaponAperShotgun)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_apershotgun, CWeaponAperShotgun );
PRECACHE_WEAPON_REGISTER(weapon_apershotgun);

BEGIN_DATADESC( CWeaponAperShotgun )

	DEFINE_FIELD( m_bDelayedFire1, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bDelayedFire2, FIELD_BOOLEAN ),

END_DATADESC()

acttable_t	CWeaponAperShotgun::m_acttable[] = 
{
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },	// FIXME: hook to apershotgun unique

	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SHOTGUN,			true },
	{ ACT_RELOAD,					ACT_RELOAD_SHOTGUN,					false },
	{ ACT_WALK,						ACT_WALK_RIFLE,						true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SHOTGUN,				true },

// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SHOTGUN_RELAXED,		false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SHOTGUN_STIMULATED,	false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_SHOTGUN_AGITATED,		false },//always aims

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

	{ ACT_WALK_AIM,					ACT_WALK_AIM_SHOTGUN,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,				true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,			true },
	{ ACT_RUN,						ACT_RUN_RIFLE,						true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_SHOTGUN,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,				true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,			true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SHOTGUN,	true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SHOTGUN_LOW,		true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SHOTGUN_LOW,				false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SHOTGUN,			false },
};

IMPLEMENT_ACTTABLE(CWeaponAperShotgun);

void CWeaponAperShotgun::Precache( void )
{
	// ICE precache the grenade
	UTIL_PrecacheOther("grenade_ar2");

	CBaseCombatWeapon::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOperator - 
//-----------------------------------------------------------------------------
void CWeaponAperShotgun::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles )
{
	Vector vecShootOrigin, vecShootDir;
	CAI_BaseNPC *npc = pOperator->MyNPCPointer();
	ASSERT( npc != NULL );
	WeaponSound( SINGLE_NPC );
	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;

	if ( bUseWeaponAngles )
	{
		QAngle	angShootDir;
		GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
		AngleVectors( angShootDir, &vecShootDir );
	}
	else 
	{
		vecShootOrigin = pOperator->Weapon_ShootPosition();
		vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );
	}

	pOperator->FireBullets( 8, vecShootOrigin, vecShootDir, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAperShotgun::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	FireNPCPrimaryAttack( pOperator, true );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponAperShotgun::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_SHOTGUN_FIRE:
		{
			FireNPCPrimaryAttack( pOperator, false );
		}
		break;

		default:
			CBaseCombatWeapon::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}


//-----------------------------------------------------------------------------
// Purpose:	When we shipped HL2, the apershotgun weapon did not override the
//			BaseCombatWeapon default rest time of 0.3 to 0.6 seconds. When
//			NPC's fight from a stationary position, their animation events
//			govern when they fire so the rate of fire is specified by the
//			animation. When NPC's move-and-shoot, the rate of fire is 
//			specifically controlled by the shot regulator, so it's imporant
//			that GetMinRestTime and GetMaxRestTime are implemented and provide
//			reasonable defaults for the weapon. To address difficulty concerns,
//			we are going to fix the combine's rate of apershotgun fire in episodic.
//			This change will not affect Alyx using a apershotgun in EP1. (sjb)
//-----------------------------------------------------------------------------
float CWeaponAperShotgun::GetMinRestTime()
{
	if( hl2_episodic.GetBool() && GetOwner() && GetOwner()->Classify() == CLASS_COMBINE )
	{
		return 1.2f;
	}
	
	return BaseClass::GetMinRestTime();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CWeaponAperShotgun::GetMaxRestTime()
{
	if( hl2_episodic.GetBool() && GetOwner() && GetOwner()->Classify() == CLASS_COMBINE )
	{
		return 1.5f;
	}

	return BaseClass::GetMaxRestTime();
}

//-----------------------------------------------------------------------------
// Purpose: Time between successive shots in a burst. Also returned for EP2
//			with an eye to not messing up Alyx in EP1.
//-----------------------------------------------------------------------------
float CWeaponAperShotgun::GetFireRate()
{
	if( hl2_episodic.GetBool() && GetOwner() && GetOwner()->Classify() == CLASS_COMBINE )
	{
		return 0.8f;
	}

	return 0.7;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CWeaponAperShotgun::Reload( void )
{
	bool fRet;
	//float fCacheTime = m_flNextSecondaryAttack;

	fRet = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( fRet )
	{

		WeaponSound( RELOAD );

		// ICE this is my hack to reload to our max shells
		CBaseCombatCharacter *pOwner  = GetOwner();
	
		if ( pOwner == NULL )
			return fRet;

		// we have enough to top up our clip from empty
		if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) >= GetMaxClip1() )
		{
			if ( Clip1() < GetMaxClip1() )
			{
				// how many do we need to top up?
				int tempShellsNeeded = GetMaxClip1() - Clip1();

				// top it up
				m_iClip1 += tempShellsNeeded;
				pOwner->RemoveAmmo( tempShellsNeeded, m_iPrimaryAmmoType );
			}
		// just add the remander shells
		}else if(pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0){
			if ( Clip1() < GetMaxClip1() )
			{
				// how many do we need to top up?
				int tempShellsNeeded = GetMaxClip1() - Clip1();

				// how many do we have to work with?
				int tempbullets = pOwner->GetAmmoCount( m_iPrimaryAmmoType );

				// we have more than enough, use them
				if(tempbullets > tempShellsNeeded){
					m_iClip1 += tempShellsNeeded;
					pOwner->RemoveAmmo( tempShellsNeeded, m_iPrimaryAmmoType );
				} else {
					// we have less then we need, just toss them in there
					m_iClip1 += tempbullets;
					pOwner->RemoveAmmo( tempbullets, m_iPrimaryAmmoType );
				}
			}
		}
	}

	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponAperShotgun::DryFire( void )
{
	WeaponSound(EMPTY);
	SendWeaponAnim( ACT_VM_DRYFIRE );
	
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponAperShotgun::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(SINGLE);

	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_iClip1 -= 1;

	Vector	vecSrc		= pPlayer->Weapon_ShootPosition( );
	Vector	vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );	

	//pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 1.0 );
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + .2 );
	
	// Fire the bullets, and force the first shot to be perfectly accuracy
	pPlayer->FireBullets( sk_plr_num_shotgun_pellets.GetInt(), vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0, -1, -1, 0, NULL, true, true );
	
	//pPlayer->ViewPunch( QAngle( random->RandomFloat( -2, -1 ), random->RandomFloat( -2, 2 ), 0 ) );
	pPlayer->ViewPunch( QAngle( random->RandomFloat( -4, -3 ), random->RandomFloat( -4, 4 ), 0 ) );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2, GetOwner() );

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponAperShotgun::SecondaryAttack( void )
{
	// ICE Secondary Attack changed to fire off smg1 Grenades

	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	//Must have ammo
	if ( ( pPlayer->GetAmmoCount( m_iSecondaryAmmoType ) <= 0 ) || ( pPlayer->GetWaterLevel() == 3 ) )
	{
		SendWeaponAnim( ACT_VM_DRYFIRE );
		BaseClass::WeaponSound( EMPTY );
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		return;
	}

	if( m_bInReload )
		m_bInReload = false;

	// MUST call sound before removing a round from the clip of a CMachineGun
	BaseClass::WeaponSound( WPN_DOUBLE );

	// ICE removed this rumble effect because I could't compile, Im poor coder
	//pPlayer->RumbleEffect( RUMBLE_357, 0, RUMBLE_FLAGS_NONE );

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector	vecThrow;
	// Don't autoaim on grenade tosses
	AngleVectors( pPlayer->EyeAngles() + pPlayer->GetPunchAngle(), &vecThrow );
	VectorScale( vecThrow, 1000.0f, vecThrow );
	
	//Create the grenade
	QAngle angles;
	VectorAngles( vecThrow, angles );
	CGrenadeAR2 *pGrenade = (CGrenadeAR2*)Create( "grenade_ar2", vecSrc, angles, pPlayer );
	pGrenade->SetAbsVelocity( vecThrow );

	pGrenade->SetLocalAngularVelocity( RandomAngle( -400, 400 ) );
	pGrenade->SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE ); 
	pGrenade->SetThrower( GetOwner() );
	pGrenade->SetDamage( sk_plr_dmg_smg1_grenade.GetFloat() );

	SendWeaponAnim( ACT_VM_SECONDARYATTACK );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 1000, 0.2, GetOwner(), SOUNDENT_CHANNEL_WEAPON );

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// Decrease ammo
	pPlayer->RemoveAmmo( 1, m_iSecondaryAmmoType );

	// Can shoot again immediately
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;

	// Register a muzzleflash for the AI.
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );	

	m_iSecondaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, false, GetClassname() );
}
	
//-----------------------------------------------------------------------------
// Purpose: Override so apershotgun can do mulitple reloads in a row
//-----------------------------------------------------------------------------
void CWeaponAperShotgun::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if (!pOwner)
	{
		return;
	}


	// AperShotgun shoot secondary
	if ((m_bDelayedFire2 || pOwner->m_nButtons & IN_ATTACK2)&&(m_flNextSecondaryAttack <= gpGlobals->curtime))
	{
		m_bDelayedFire2 = false;
		
		//Warning("ERROR: we are in!\n");
		
		// ICE secondary dont use clips, thats from shotgun code

		if ( pOwner->GetAmmoCount(m_iSecondaryAmmoType) )
		{
			SecondaryAttack();
		} 
		else if (!pOwner->GetAmmoCount(m_iSecondaryAmmoType))
		{
				DryFire();
		}
	}
	else if ((m_bDelayedFire1 || pOwner->m_nButtons & IN_ATTACK) && m_flNextPrimaryAttack <= gpGlobals->curtime)
	{
		m_bDelayedFire1 = false;
		if ( (m_iClip1 <= 0 && UsesClipsForAmmo1()) || ( !UsesClipsForAmmo1() && !pOwner->GetAmmoCount(m_iPrimaryAmmoType) ) )
		{
			if (!pOwner->GetAmmoCount(m_iPrimaryAmmoType))
			{
				DryFire();
			}
			else
			{
				Reload();
			}
		}
		// Fire underwater?
		else if (pOwner->GetWaterLevel() == 3 && m_bFiresUnderwater == false)
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
			return;
		}
		else
		{
			// If the firing button was just pressed, reset the firing time
			CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
			if ( pPlayer && pPlayer->m_afButtonPressed & IN_ATTACK )
			{
				 m_flNextPrimaryAttack = gpGlobals->curtime + m_fNextShotTime;
			}

			if(m_bReleasedFire){
				m_bReleasedFire = false;
				PrimaryAttack();
			}
		}
	}

	// ICE check to see if you ever released the fire button and allow firing again
	if(((pOwner->m_nButtons & IN_ATTACK) == false) && (!m_bReleasedFire)){
		m_bReleasedFire = true;
	}

	//if ( pOwner->m_nButtons & IN_RELOAD && UsesClipsForAmmo1() && !m_bInReload ) 

	if ( pOwner->m_nButtons & IN_RELOAD && UsesClipsForAmmo1()) 
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}
	else 
	{
		// no fire buttons down
		m_bFireOnEmpty = false;

		if (!HasAnyAmmo()){
			//Warning("ERROR: switch was called!\n");
			pOwner->SwitchToNextBestWeapon( this );
		}


		if ( !HasAnyAmmo() && m_flNextPrimaryAttack < gpGlobals->curtime ) 
		{
			pOwner->SwitchToNextBestWeapon( this );
			return;
			// weapon isn't useable, switch.
		}
		else
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( m_iClip1 <= 0 && !(GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < gpGlobals->curtime )
			{
				if (Reload())
				{
					// if we've successfully started to reload, we're done
					return;
				}
			}
		}

		WeaponIdle( );
		return;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponAperShotgun::CWeaponAperShotgun( void )
{
	m_bReloadsSingly = true;

	//m_bNeedPump		= false;
	m_bDelayedFire1 = false;
	m_bDelayedFire2 = false;
	m_fNextShotTime = .1;
	m_bReleasedFire	= true;

	m_fMinRange1		= 0.0;
	m_fMaxRange1		= 500;
	m_fMinRange2		= 0.0;
	m_fMaxRange2		= 200;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAperShotgun::ItemHolsterFrame( void )
{

}
