//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Atom Smasher - Particle Accelerator Weapon
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "AI_BaseNPC.h"
#include "player.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "gamestats.h"

#include "Sprite.h"
#include "beam_shared.h"
#include "ammodef.h"

//#include "grenade_ar2.h"
//#include "prop_combine_ball.h"

#define	BEAMWEAPON_BEAM_SPRITE "sprites/crystal_beam1.vmt"
#define	BEAMWEAPON_SMALLBEAM_SPRITE "sprites/hydraspinalcord.vmt"
#define	BEAMWEAPON_KILLERBEAM_SPRITE "sprites/physbeam.vmt"
#define	BEAMWEAPON_KILLERSMALLBEAM_SPRITE "sprites/laser.vmt"

#define	BEAMWEAPON_BEAM_ATTACHMENT "Muzzle"
#define	BEAMWEAPON_TARMBEAM_ATTACHMENT "tarmtip"
#define	BEAMWEAPON_RARMBEAM_ATTACHMENT "rarmtip"
#define	BEAMWEAPON_LARMBEAM_ATTACHMENT "larmtip"

#define BEAMWEAPON_SAMMO_COSUM .15;

extern short	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


/*-----------------------------------------------------------------------------
- CWeaponAtomSmasher
Important Animation Names:

$sequence spinup "spinup" fps 10 snap activity		ACT_VM_PULLPIN 
$sequence fire "fullspeed" fps 20 snap activity		ACT_VM_PRIMARYATTACK 
$sequence spindow "spindown" fps 30 snap activity		ACT_VM_SWINGMISS 

$sequence altpowerup "charge" fps 10 snap activity	ACT_VM_PULLBACK_HIGH 
$sequence altfire "charge_fire" fps 30 snap activity			ACT_VM_SECONDARYATTACK 


Secondary fire lets you charge by holding the secondary fire.
There are 3 stages to this attack
The first, if you release the secondary fire between 0, and 1 second, will shoot a beam like the primary
The second stage, if you release the secondary fire between 1, and 2 seconds, will shoot a thin beam like the primary but has more damage
The third stage, if you release the secondary fire after 3 seconds, will shoot thin red beams and dissolve any any it hits
	this stage will also give the player velocity in the opposite direction as shot, and if you are close to what you are shooting will double this velocity.
	(cool to launch the player up!)

-----------------------------------------------------------------------------*/

class CWeaponAtomSmasher : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CWeaponAtomSmasher, CBaseHLCombatWeapon );

	CWeaponAtomSmasher(void);

	DECLARE_SERVERCLASS();

	void	Precache( void );
	void	ItemPostFrame( void );

	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	AttackCharge( int ChargeType );

	void	DryFire( void );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	bool	CanHolster( void );

	float	GetFireRate( void ) { return 0.075f; }	// 13.3hz

	void	DrawBeam(const Vector &startPos, const Vector &endPos, char* t_sprite, char* t_Attachment, float width, float attackduration);
	void	DoImpactEffect( trace_t &tr, int nDamageType);

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = Vector(0,0,0);
		return cone;
	}

	virtual bool Reload( void );

	DECLARE_ACTTABLE();

private:
	int		m_nChargeType;
	bool	m_bHasCharge;
	int		m_nBulletType;
	int		m_nNumShotsFired;
	float	m_fRandomPushTime;
	float	m_flNextChargeTime;
	float	m_nSecondaryAmmoTime; // Ammo is removed 1 by every beamweapon_secondary_consumption (.3?) sec

	int		m_bWeaponCurState;
	float	m_nNextAnimationTime;

	// beamgrow
	// to gradually widen the beam (add to dataesc)
	float m_fBeamWidth;
	float m_fBeamCounter;

	// sprites used
	int		m_nGlowSpriteIndex;

};


IMPLEMENT_SERVERCLASS_ST(CWeaponAtomSmasher, DT_WeaponAtomSmasher)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_atomsmasher, CWeaponAtomSmasher );
PRECACHE_WEAPON_REGISTER( weapon_atomsmasher );

BEGIN_DATADESC( CWeaponAtomSmasher )
	
	DEFINE_FIELD( m_nChargeType,			FIELD_INTEGER ),
	DEFINE_FIELD( m_flNextChargeTime,		FIELD_TIME ),
	DEFINE_FIELD( m_nSecondaryAmmoTime,		FIELD_TIME ),
	DEFINE_FIELD( m_bHasCharge,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nNumShotsFired,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nBulletType,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nGlowSpriteIndex,		FIELD_INTEGER ),
	DEFINE_FIELD( m_bWeaponCurState,		FIELD_INTEGER ),
	DEFINE_FIELD( m_nNextAnimationTime,		FIELD_TIME ),
	DEFINE_FIELD( m_fRandomPushTime,		FIELD_TIME ),
	DEFINE_FIELD( m_fBeamWidth,			    FIELD_FLOAT ),
	DEFINE_FIELD( m_fBeamCounter,		    FIELD_FLOAT ),

END_DATADESC()

acttable_t	CWeaponAtomSmasher::m_acttable[] = 
{
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			true },
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,	false },
	{ ACT_COVER_LOW,				ACT_COVER_PISTOL_LOW,			false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,		false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_WALK,						ACT_WALK_PISTOL,				false },
	{ ACT_RUN,						ACT_RUN_PISTOL,					false },
};


IMPLEMENT_ACTTABLE( CWeaponAtomSmasher );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponAtomSmasher::CWeaponAtomSmasher( void )
{
	m_fMinRange1		= 24;
	m_fMaxRange1		= 1500;
	m_fMinRange2		= 24;
	m_fMaxRange2		= 200;
	m_bFiresUnderwater	= false;
	m_nBulletType = -1;
	m_nChargeType = 0;
	m_bHasCharge = false;
	m_fRandomPushTime = gpGlobals->curtime;

	m_bWeaponCurState	 = 0;
	m_nNextAnimationTime = gpGlobals->curtime;
	m_flNextChargeTime = gpGlobals->curtime;

	//beamgrow
	m_fBeamWidth = .5f;
	m_fBeamCounter = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAtomSmasher::Precache( void )
{
	m_nGlowSpriteIndex = PrecacheModel("sprites/glow08.vmt");

	BaseClass::Precache();
	UTIL_PrecacheOther( "env_entity_dissolver" );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponAtomSmasher::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_PISTOL_FIRE:
		{
			Vector vecShootOrigin, vecShootDir;
			vecShootOrigin = pOperator->Weapon_ShootPosition();

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			ASSERT( npc != NULL );

			vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

			CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );

			WeaponSound( SINGLE_NPC );
			pOperator->FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2 );
			pOperator->DoMuzzleFlash();
			m_iClip1 = m_iClip1 - 1;
		}
		break;
		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponAtomSmasher::DryFire( void )
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration( ACT_VM_SECONDARYATTACK );

	if(m_bWeaponCurState != 0)	 // do I need this?
		m_bWeaponCurState = 0;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponAtomSmasher::PrimaryAttack( void )
{
	if(m_bHasCharge)
		return;

	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		Reload();
		return;
	}

	// Cannot fire underwater
	if ( GetOwner() && GetOwner()->GetWaterLevel() == 3 )
	{
		SendWeaponAnim( ACT_VM_DRYFIRE );
		BaseClass::WeaponSound( EMPTY );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration( ACT_VM_DRYFIRE );

		if(m_bWeaponCurState != 0)	 // do I need this?
			m_bWeaponCurState = 0;

		// beamgrow
		if(m_fBeamWidth != 0)
		{
			m_fBeamCounter = 0;
			m_fBeamWidth = .5f;
		}
		return;
	}

	// Check to see if pre-fire spinup animation has ran through, or start it, and keep checking to see if it finished, then move on.
	if(m_bWeaponCurState == 0)
	{
		m_bWeaponCurState = 1;
		SendWeaponAnim( ACT_VM_PULLPIN );
		m_nNextAnimationTime = gpGlobals->curtime + SequenceDuration( ACT_VM_PULLPIN ); //+ .4f; hackin?

		//beamgrow
		if(m_fBeamWidth != 0)
		{
			m_fBeamCounter = 0;
			m_fBeamWidth = .5f;
		}
		return;
	} 
	else 
	{
		if (m_nNextAnimationTime <= gpGlobals->curtime)
		{
			if (m_bWeaponCurState == 1)
			{
				m_nNextAnimationTime = gpGlobals->curtime + SequenceDuration( ACT_VM_PRIMARYATTACK );
				SendWeaponAnim( ACT_VM_PRIMARYATTACK );
				// player "shoot" animation
				pPlayer->SetAnimation( PLAYER_ATTACK1 );
			}
		} 
		else 
		{
			if (m_nNextAnimationTime > gpGlobals->curtime)
			{
				if(m_bWeaponCurState == 1)
				{
					return;
				}
			} 
		}
		// beamgrow
		if( m_fBeamWidth < 3)
		{
			if( m_fBeamCounter < 3)
			{
				m_fBeamCounter += 1;
			}
			else
			{
				m_fBeamCounter = 0;
				m_fBeamWidth += .5f;
			}
		}
	}


	FireBulletsInfo_t info;
	info.m_vecSrc	 = pPlayer->Weapon_ShootPosition( );
	
	info.m_vecDirShooting = pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );

	// To make the firing framerate independent, we may have to fire more than one bullet here on low-framerate systems, 
	// especially if the weapon we're firing has a really fast rate of fire.
	info.m_iShots = 0;
	float fireRate = GetFireRate();

	while ( m_flNextPrimaryAttack <= gpGlobals->curtime )
	{
		// MUST call sound before removing a round from the clip of a CMachineGun
		WeaponSound(SINGLE, m_flNextPrimaryAttack);
		
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration( ACT_VM_PULLBACK_HIGH );

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
	info.m_flDamage = 6;
	info.m_iPlayerDamage = 6; // if hit player
	info.m_flDamageForceScale = 100.0f;

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

	// push the player back randomly
	if(m_fRandomPushTime < gpGlobals->curtime)
	{
		m_fRandomPushTime = gpGlobals->curtime + RandomFloat( .02f, .2f);

		Vector forward;
		AngleVectors( pPlayer->GetAbsAngles(), &forward );
		forward = -(forward * 45);
		pPlayer->VelocityPunch( forward );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAtomSmasher::SecondaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	// The way it works now, it uses up ammo every .5 sec (need faster), then it should stop for a sec, when
	// it reaches the next charge level, then continue.. but in the end it never stops consuming ammo.. has to change.

	// make sure we have enough ammo every BEAMWEAPON_SAMMO_COSUM sec or you can't charge anymore.
	if(m_iClip1 >= 1){
		if(m_nChargeType < 3)
		{
			if(m_nSecondaryAmmoTime < gpGlobals->curtime)
			{
				m_iClip1--;
				m_nSecondaryAmmoTime = gpGlobals->curtime + BEAMWEAPON_SAMMO_COSUM;
			}
		}
	}
	else
	{
		if(!m_bHasCharge)
		{
			Reload();
		}
		return;
	}

	if(m_flNextChargeTime < gpGlobals->curtime)
	{
		if(!m_bHasCharge)
			m_bHasCharge = true;

		if(m_nChargeType < 3)
		{
			if(m_nChargeType == 0)
			{
				WeaponSound( BURST );
			} 
			else if(m_nChargeType == 1)
			{
				WeaponSound( SPECIAL1 );
				UTIL_ScreenShake( GetAbsOrigin(), (m_nChargeType * 5), 100.0f, 1.5f, 128, SHAKE_START, false );
			} 
			else if(m_nChargeType == 2)
			{
				WeaponSound( SPECIAL2 );
				UTIL_ScreenShake( GetAbsOrigin(), (m_nChargeType * 5), 100.0f, 1.5f, 128, SHAKE_START, false );
			}
			m_nChargeType++;
			m_nSecondaryAmmoTime = gpGlobals->curtime + .8f; // pause the ammo consumption
		}

		if(m_nChargeType == 3)
		{
			// play charge animation
			SendWeaponAnim( ACT_VM_PRIMARYATTACK );		// the primary attack has a good shake to it, lets reuse this animation (im lazy)
			m_flNextChargeTime = gpGlobals->curtime + SequenceDuration( ACT_VM_PRIMARYATTACK );
		}
		else if(m_nChargeType == 2)
		{
			// play charge animation
			SendWeaponAnim( ACT_VM_PULLBACK_HIGH );
			m_flNextChargeTime = gpGlobals->curtime + 2.4f;
		}
		else
		{
			// play charge animation
			SendWeaponAnim( ACT_VM_PULLBACK_HIGH );
			// set the next charge time
			m_flNextChargeTime = gpGlobals->curtime + 1.6f;
		}
	}
	
	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}
}



//-----------------------------------------------------------------------------
// Purpose: Override if we're waiting to release a shot
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponAtomSmasher::CanHolster( void )
{
	if(m_bHasCharge)
		return false;

	if(m_bWeaponCurState != 0)
		m_bWeaponCurState = 0;

	return BaseClass::CanHolster();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponAtomSmasher::Reload( void )
{
	if(m_bHasCharge)
		return false;

	return DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
}

//-----------------------------------------------------------------------------
// Purpose: Just need to check if you released the fire button, 
//          if so you'll have to wait the spin up time again.
// Input  : 
//-----------------------------------------------------------------------------

void CWeaponAtomSmasher::ItemPostFrame( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
		return;

	// test
	BaseClass::ItemPostFrame();

	// check if we reloading
	if ( m_bInReload )
	{
		if(m_bWeaponCurState != 0)
			m_bWeaponCurState = 0;
		if(m_bHasCharge)
			m_bHasCharge = false;
		return;
	}

	// check if we are max charge and restric running
	if( m_nChargeType > 2 ){
		if( pPlayer->IsSuitEquipped() ){
			pPlayer->SetMaxSpeed( 100 );
		} else {
			pPlayer->SetMaxSpeed( 90 );
		}
	} else {
		if( pPlayer->IsSuitEquipped() ){
			pPlayer->SetMaxSpeed( 190 );
		} else {
			pPlayer->SetMaxSpeed( 150 );
		}
	}


	// -----------------------
	//  No buttons down
	// -----------------------
	if (!((pPlayer->m_nButtons & IN_ATTACK) || (pPlayer->m_nButtons & IN_ATTACK2) || (pPlayer->m_nButtons & IN_RELOAD)))
	{
		// no fire buttons down or reloading
		if ( !ReloadOrSwitchWeapons() && ( m_bInReload == false ) )
		{
			if(m_nChargeType)
			{
				// send sound
				WeaponSound( SINGLE ); // change for a new sound

				// reset the charge time to now
				m_flNextChargeTime = gpGlobals->curtime;

				// do the attack
				AttackCharge(m_nChargeType);

				m_bHasCharge = false;
				m_nChargeType = 0;
			}
			else if(m_bWeaponCurState != 0)
			{
				// send the spin down animation
				SendWeaponAnim( ACT_VM_SWINGMISS );

				// reset m_bWeaponCurState to 0
				m_bWeaponCurState = 0;
			}
		}
	}
}

void CWeaponAtomSmasher::AttackCharge( int ChargeType )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	FireBulletsInfo_t info;
	info.m_vecSrc	 = pPlayer->Weapon_ShootPosition( );
	info.m_vecDirShooting = pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );
	info.m_flDistance = MAX_TRACE_LENGTH;
	info.m_iAmmoType = m_iPrimaryAmmoType;
	info.m_iTracerFreq = 2;

	if(ChargeType == 1)
	{
		info.m_flDamage = 20;
		info.m_iPlayerDamage = 20;
		info.m_flDamageForceScale = 100.0f;
	} 
	else if(ChargeType == 2)
	{
		info.m_flDamage = 80;
		info.m_iPlayerDamage = 80;
		info.m_flDamageForceScale = 300.0f;
	} 
	else if(ChargeType == 3)
	{
		info.m_flDamage = 220;
		info.m_iPlayerDamage = 220;
		info.m_flDamageForceScale = 1000.0f;
	}
	
#if !defined( CLIENT_DLL )
	// Fire the bullets
	info.m_vecSpread = pPlayer->GetAttackSpread( this );
#else
	//!!!HACKHACK - what does the client want this function for? 
	info.m_vecSpread = GetActiveWeapon()->GetBulletSpread();
#endif // CLIENT_DLL

	pPlayer->FireBullets( info );

	// push the player back
	Vector forward;
	AngleVectors( pPlayer->GetAbsAngles(), &forward );
	if(ChargeType == 1)
	{
		forward = -(forward * 300);
	} 
	else if(ChargeType == 2)
	{
		forward = -(forward * 400);
	} 
	else if(ChargeType == 3)
	{
		forward = -(forward * 480);
	}
	pPlayer->VelocityPunch( forward );

	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &startPos - where the beam should begin
//          &endPos - where the beam should end
//          width - what the diameter of the beam should be (units?)
//-----------------------------------------------------------------------------
void CWeaponAtomSmasher::DrawBeam( const Vector &startPos, const Vector &endPos, char* t_sprite, char* t_Attachment, float width, float attackDuration )
{
	//Tracer down the middle
	UTIL_Tracer( startPos, endPos, 0, TRACER_DONT_USE_ATTACHMENT, 6500, false, "GaussTracer" );
 
	//Draw the main beam shaft
	CBeam *pBeam = CBeam::BeamCreate( t_sprite, width ); //width here use to be 15.5

	// It starts at startPos
	pBeam->SetStartPos( startPos );
 
	// This sets up some things that the beam uses to figure out where
	// it should start and end
	pBeam->PointEntInit( endPos, this );
 
	// This makes it so that the laser appears to come from the assigned attachment
	pBeam->SetEndAttachment( LookupAttachment(t_Attachment) );
	//pBeam->SetEndAttachment( LookupAttachment("Muzzle") );

	if(m_nChargeType > 0)
	{
		float newWidth = width / (m_nChargeType + 1);
		pBeam->SetWidth( newWidth );
	}
	else
	{
		pBeam->SetWidth( width );
	}

	// Higher brightness means less transparent
	pBeam->SetBrightness( 255 );
	pBeam->SetColor( 255, 185+random->RandomInt( -16, 16 ), 40 );
	pBeam->RelinkBeam();
 
	// The beam should only exist for a very short time
	pBeam->LiveForTime( attackDuration );

	// The Beams should scroll foward
	pBeam->SetScrollRate( 30 );

}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &tr - used to figure out where to do the effect
//          nDamageType - ???
//-----------------------------------------------------------------------------
void CWeaponAtomSmasher::DoImpactEffect( trace_t &tr, int nDamageType )
{
	//Draw beams
	if(m_bHasCharge && m_nChargeType > 0)
	{
		DrawBeam( tr.startpos, tr.endpos, BEAMWEAPON_KILLERBEAM_SPRITE, BEAMWEAPON_BEAM_ATTACHMENT, (m_nChargeType), SequenceDuration( ACT_VM_PRIMARYATTACK ) ); // m_nChargeType was 3
		DrawBeam( tr.startpos, tr.endpos, BEAMWEAPON_KILLERSMALLBEAM_SPRITE, BEAMWEAPON_TARMBEAM_ATTACHMENT, (m_nChargeType + 1), SequenceDuration( ACT_VM_PRIMARYATTACK ) ); // m_nChargeType was 4
		DrawBeam( tr.startpos, tr.endpos, BEAMWEAPON_KILLERSMALLBEAM_SPRITE, BEAMWEAPON_RARMBEAM_ATTACHMENT, (m_nChargeType + 1), SequenceDuration( ACT_VM_PRIMARYATTACK ) ); // m_nChargeType was 4
		DrawBeam( tr.startpos, tr.endpos, BEAMWEAPON_KILLERSMALLBEAM_SPRITE, BEAMWEAPON_LARMBEAM_ATTACHMENT, (m_nChargeType + 1), SequenceDuration( ACT_VM_PRIMARYATTACK ) ); // m_nChargeType was 4
	}
	else
	{
		DrawBeam( tr.startpos, tr.endpos, BEAMWEAPON_BEAM_SPRITE, BEAMWEAPON_BEAM_ATTACHMENT, (m_fBeamWidth + 1), SequenceDuration( ACT_VM_PRIMARYATTACK ) ); // m_fBeamWidth 4 before beamgrow
		DrawBeam( tr.startpos, tr.endpos, BEAMWEAPON_SMALLBEAM_SPRITE, BEAMWEAPON_TARMBEAM_ATTACHMENT, m_fBeamWidth, SequenceDuration( ACT_VM_PRIMARYATTACK ) ); // m_fBeamWidth 3 before beamgrow
		DrawBeam( tr.startpos, tr.endpos, BEAMWEAPON_SMALLBEAM_SPRITE, BEAMWEAPON_RARMBEAM_ATTACHMENT, m_fBeamWidth, SequenceDuration( ACT_VM_PRIMARYATTACK ) ); // m_fBeamWidth 3 before beamgrow
		DrawBeam( tr.startpos, tr.endpos, BEAMWEAPON_SMALLBEAM_SPRITE, BEAMWEAPON_LARMBEAM_ATTACHMENT, m_fBeamWidth, SequenceDuration( ACT_VM_PRIMARYATTACK ) ); // m_fBeamWidth 3 before beamgrow
	}
	
	// not shooting the sky && tracer did not end up underwater, if so, do not do the impact effects!
	if (!(tr.surface.flags & SURF_SKY))
	{
		CPVSFilter filter( tr.endpos );
		if(!( enginetrace->GetPointContents( tr.endpos ) & (CONTENTS_WATER|CONTENTS_SLIME) ))
		{
			te->GaussExplosion( filter, 0.0f, tr.endpos, tr.plane.normal, 0 );

			// if you hit the world, it should lave a glow sprite
			if ( tr.DidHitWorld() )
			{
				// new proper glow sprite
				CSprite *pSprite = CSprite::SpriteCreate( "sprites/glow08.vmt", tr.endpos, false );

				if ( pSprite )
				{
					pSprite->FadeAndDie( 1.5f );
					pSprite->SetScale( .4f );
					if(m_bHasCharge)
					{
						if(m_nChargeType > 0)
						{
							pSprite->SetTransparency( kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation );
						}
					}
					else
					{
						pSprite->SetTransparency( kRenderGlow, 255, 255, 255, 200, kRenderFxNoDissipation );
					}
					pSprite->SetGlowProxySize( 4.0f );
				}
			}

			UTIL_DecalTrace( &tr, "FadingScorch" );
		}

		// if you are at charge level 3, you should always explode, even under water
		if(m_nChargeType == 3)
		{
			te->Explosion( filter, 0.0,
				&tr.endpos,   // &GetAbsOrigin()
				g_sModelIndexFireball,
				2.0, 
				15,
				TE_EXPLFLAG_NONE,
				200, //m_DmgRadius was 90
				120 ); //m_flDamage
		}
	}  
	// do dissolve damage level 3
	if(m_bHasCharge)
	{
		if(m_nChargeType == 2)
		{
			RadiusDamage( CTakeDamageInfo( this, this, 120, DMG_DISSOLVE ), tr.endpos, 150.0f, CLASS_NONE, NULL );
		} 
		else if(m_nChargeType == 3)
		{
			RadiusDamage( CTakeDamageInfo( this, this, 120, DMG_DISSOLVE ), tr.endpos, 300.0f, CLASS_NONE, NULL );
		}
	}
}