//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "voloxelican_spit.h"
#include "soundent.h"
#include "decals.h"
#include "smoke_trail.h"
#include "hl2_shareddefs.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "particle_parse.h"
#include "particle_system.h"
#include "soundenvelope.h"
#include "ai_utils.h"
#include "te_effect_dispatch.h"
#include "gib.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar    sk_voloxelican_spit_grenade_dmg		  ( "sk_voloxelican_spit_grenade_dmg", "20", FCVAR_NONE, "Total damage done by an individual voloxelican loogie.");
ConVar	  sk_voloxelican_spit_grenade_radius		  ( "sk_voloxelican_spit_grenade_radius","40", FCVAR_NONE, "Radius of effect for an voloxelican spit grenade.");
ConVar	  sk_voloxelican_spit_grenade_poison_ratio ( "sk_voloxelican_spit_grenade_poison_ratio","0.3", FCVAR_NONE, "Percentage of an voloxelican's spit damage done as poison (which regenerates)"); 

LINK_ENTITY_TO_CLASS( voloxelican_spit, CVoloxelicanSpit );

BEGIN_DATADESC( CVoloxelicanSpit )

	DEFINE_FIELD( m_bPlaySound, FIELD_BOOLEAN ),

	// Function pointers
	DEFINE_ENTITYFUNC( VoloxelicanSpitTouch ),

END_DATADESC()

CVoloxelicanSpit::CVoloxelicanSpit( void ) : m_bPlaySound( true ), m_pHissSound( NULL )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVoloxelicanSpit::Spawn( void )
{
	Precache( );
	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetSolidFlags( FSOLID_NOT_STANDABLE );

	SetModel( "models/voloxelican_spit.mdl" );
	UTIL_SetSize( this, vec3_origin, vec3_origin );

	SetUse( &CBaseGrenade::DetonateUse );
	SetTouch( &CVoloxelicanSpit::VoloxelicanSpitTouch );
	SetNextThink( gpGlobals->curtime + 0.1f );

	m_flDamage		= sk_voloxelican_spit_grenade_dmg.GetFloat();
	m_DmgRadius		= sk_voloxelican_spit_grenade_radius.GetFloat();
	m_takedamage	= DAMAGE_NO;
	m_iHealth		= 1;

	SetGravity( UTIL_ScaleForGravity( SPIT_GRAVITY ) );
	SetFriction( 0.8f );

	SetCollisionGroup( HL2COLLISION_GROUP_SPIT );

	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	// We're self-illuminating, so we don't take or give shadows
	AddEffects( EF_NOSHADOW|EF_NORECEIVESHADOW );

	//Vector	dir = GetAbsVelocity();
	UTIL_BloodDrips( GetAbsOrigin(), GetAbsVelocity(), BLOOD_COLOR_GREEN, 20 ); //vec3_origin = dir, vecSpot = GetAbsOrigin()
	EmitSound("BaseCombatCharacter.CorpseGib");

	/* Create the dust effect in place
	m_hSpitEffect = (CParticleSystem *) CreateEntityByName( "info_particle_system" );
	if ( m_hSpitEffect != NULL )
	{
		// Setup our basic parameters
		m_hSpitEffect->KeyValue( "start_active", "1" );
		m_hSpitEffect->KeyValue( "effect_name", "antlion_spit_trail" );
		m_hSpitEffect->SetParent( this );
		m_hSpitEffect->SetLocalOrigin( vec3_origin );
		DispatchSpawn( m_hSpitEffect );
		if ( gpGlobals->curtime > 0.5f )
			m_hSpitEffect->Activate();
	}*/
}

void CVoloxelicanSpit::Event_Killed( const CTakeDamageInfo &info )
{
	Detonate( );
}

//-----------------------------------------------------------------------------
// Purpose: Handle spitting
//-----------------------------------------------------------------------------
void CVoloxelicanSpit::VoloxelicanSpitTouch( CBaseEntity *pOther )
{
	if ( pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER) )
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ( ( pOther->m_takedamage == DAMAGE_NO ) || ( pOther->m_takedamage == DAMAGE_EVENTS_ONLY ) )
			return;
	}

	// Don't hit other spit
	if ( pOther->GetCollisionGroup() == HL2COLLISION_GROUP_SPIT )
		return;

	// We want to collide with water
	const trace_t *pTrace = &CBaseEntity::GetTouchTrace();

	// copy out some important things about this trace, because the first TakeDamage
	// call below may cause another trace that overwrites the one global pTrace points
	// at.
	bool bHitWater = ( ( pTrace->contents & CONTENTS_WATER ) != 0 );
	CBaseEntity *const pTraceEnt = pTrace->m_pEnt;
	const Vector tracePlaneNormal = pTrace->plane.normal;

	if ( bHitWater )
	{
		// Splash!
		CEffectData data;
		data.m_fFlags = 0;
		data.m_vOrigin = pTrace->endpos;
		data.m_vNormal = Vector( 0, 0, 1 );
		data.m_flScale = 8.0f;

		DispatchEffect( "watersplash", data );
	}
	else
	{
		// Make a splat decal
		trace_t *pNewTrace = const_cast<trace_t*>( pTrace );
		UTIL_DecalTrace( pNewTrace, "BulletProof" );
		// Break into bits & randomize the models bodygroups
		// bodygroup is selectd by a new overloaded CGib::AltSpawnSpecificGibs 
		static int BodyGroup_Empty = FindBodygroupByName("Body");
		for(int i = 0; i < 3; i++)
		{
			CGib::AltSpawnSpecificGibs( this, 1, 300, 400, "models/gibs/voloxelican_spit/voloxelican_spit_gibs.mdl", i, 5 );
		}
	}

	// Part normal damage, part poison damage
	float poisonratio = sk_voloxelican_spit_grenade_poison_ratio.GetFloat();

	// Take direct damage if hit
	// NOTE: assume that pTrace is invalidated from this line forward!
	if ( pTraceEnt )
	{
		pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage * (1.0f-poisonratio), DMG_ACID ) );
		pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage * poisonratio, DMG_POISON ) );
	}

	CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin(), m_DmgRadius * 2.0f, 0.5f, GetThrower() );

	QAngle vecAngles;
	VectorAngles( tracePlaneNormal, vecAngles );
	
	/*if ( pOther->IsPlayer() || bHitWater )
	{
		// Do a lighter-weight effect if we just hit a player
		DispatchParticleEffect( "antlion_spit_player", GetAbsOrigin(), vecAngles );
	}
	else
	{
		DispatchParticleEffect( "antlion_spit", GetAbsOrigin(), vecAngles );
	}*/

	if ( pOther->IsPlayer() )
	{
		EmitSound( "NPC_Stalker.Hit" );	//NPC_Stalker.Hit, Boulder.ImpactHard
	}
	else if ( bHitWater )
	{
		EmitSound( "Water.ImpactHard" );	//Water.ImpactHard, Water.BulletImpact, Underwater.BulletImpact
	}
	else
	{
		EmitSound( "Concrete.BulletImpact" );	//Breakable.Concrete, Boulder.ImpactHard
	}

	Detonate();
}

void CVoloxelicanSpit::Detonate(void)
{
	m_takedamage = DAMAGE_NO;

	// Stop our hissing sound
	if ( m_pHissSound != NULL )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pHissSound );
		m_pHissSound = NULL;
	}

	if ( m_hSpitEffect )
	{
		UTIL_Remove( m_hSpitEffect );
	}

	UTIL_Remove( this );
}

void CVoloxelicanSpit::InitHissSound( void )
{
	if ( m_bPlaySound == false )
		return;

	/*
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if ( m_pHissSound == NULL )
	{
		CPASAttenuationFilter filter( this );
		m_pHissSound = controller.SoundCreate( filter, entindex(), "NPC_Antlion.PoisonBall" );
		controller.Play( m_pHissSound, 1.0f, 100 );
	}*/
}

void CVoloxelicanSpit::Think( void )
{
	InitHissSound();
	if ( m_pHissSound == NULL )
		return;
	
	// Add a doppler effect to the balls as they travel
	CBaseEntity *pPlayer = AI_GetSinglePlayer();
	if ( pPlayer != NULL )
	{
		Vector dir;
		VectorSubtract( pPlayer->GetAbsOrigin(), GetAbsOrigin(), dir );
		VectorNormalize(dir);

		float velReceiver = DotProduct( pPlayer->GetAbsVelocity(), dir );
		float velTransmitter = -DotProduct( GetAbsVelocity(), dir );
		
		// speed of sound == 13049in/s
		int iPitch = 100 * ((1 - velReceiver / 13049) / (1 + velTransmitter / 13049));

		// clamp pitch shifts
		if ( iPitch > 250 )
		{
			iPitch = 250;
		}
		if ( iPitch < 50 )
		{
			iPitch = 50;
		}

		// Set the pitch we've calculated
		CSoundEnvelopeController::GetController().SoundChangePitch( m_pHissSound, iPitch, 0.1f );
	}

	// Set us up to think again shortly
	SetNextThink( gpGlobals->curtime + 0.05f );
}

void CVoloxelicanSpit::Precache( void )
{
	PrecacheModel( "models/voloxelican_spit.mdl" ); 
	PrecacheModel( "models/gibs/voloxelican_spit/voloxelican_spit_gibs.mdl" );

	PrecacheScriptSound( "NPC_Stalker.Hit" ); // hit player NPC_Stalker.Hit, Boulder.ImpactHard
	PrecacheScriptSound( "Concrete.BulletImpact" ); // hit world
	PrecacheScriptSound( "Water.ImpactHard" ); // hit water
	PrecacheScriptSound( "BaseCombatCharacter.CorpseGib" ); // spitting sound

	//PrecacheParticleSystem( "antlion_spit_player" );
	//PrecacheParticleSystem( "antlion_spit" );
}
