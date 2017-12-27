//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Dragons. Simple ambient birds that fly away when they hear gunfire or
//			when anything gets too close to them.
//
// TODO: landing
// TODO: death
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "game.h"
#include "ai_basenpc.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_squad.h"
#include "ai_motor.h"
#include "ai_navigator.h"
#include "hl2_shareddefs.h"
#include "ai_route.h"
#include "npcevent.h"
#include "gib.h"
#include "ai_interactions.h"
#include "ndebugoverlay.h"
#include "soundent.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "movevars_shared.h"
#include "npc_dragon.h" //
#include "ai_moveprobe.h"

// had to add for attack obstructions
#include "props.h"
// had to add for phys gun holding stuff
#include "weapon_physcannon.h"
// for claw attack and vehicles
#include "vehicle_base.h"
// for particle test
#include "particle_parse.h"
// for giving eachother room
#include "ai_squad.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	DRAGON_MODEL			"models/dragon/dragon.mdl"

#define DRAGON_TORCH_DISTANCE 180

#define MELEE_MAX_DISTANCE 90
#define RANGE_MAX_DISTANCE 180
#define RANGE_MIN_DISTANCE 96
#define DRAGON_FOV_NORMAL -0.4f

//#define DRAGON_ILLUME_REMOVE_RATE .05
#define DRAGON_ALIVE_ILLUME_MIN .1
#define DRAGON_DEAD_ILLUME_MIN 0
//
// Custom activities.
//

//
// Animation events.
//
static int AE_DRAGON_MELEE_HEADBUT;
static int AE_DRAGON_RANGE_TORCH;
static int AE_DRAGON_RANGE_TORCH_END;

static int AE_DRAGON_FOOTPRINT_RIGHT;
static int AE_DRAGON_FOOTPRINT_FRONTRIGHT;
static int AE_DRAGON_FOOTPRINT_LEFT;
static int AE_DRAGON_FOOTPRINT_FRONTLEFT;

//
// Skill settings.
//
ConVar sk_dragon_health( "sk_dragon_health","0");
ConVar sk_dragon_melee_dmg( "sk_dragon_melee_dmg","0");
ConVar sk_dragon_torch_dmg( "sk_dragon_torch_dmg","0");


// slots
enum
{	
	SQUAD_SLOT_DRAGON_TORCH = LAST_SHARED_SQUADSLOT,
};

LINK_ENTITY_TO_CLASS( npc_dragon, CNPC_Dragon );

BEGIN_DATADESC( CNPC_Dragon )

	DEFINE_FIELD( m_flGroundIdleMoveTime, FIELD_TIME ),
	DEFINE_FIELD( m_flEnemyDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_vDesiredTarget, FIELD_VECTOR ),
	DEFINE_FIELD( m_vCurrentTarget, FIELD_VECTOR ),
	DEFINE_FIELD( m_bPlayedLoopingSound, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vLastStoredOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flLastStuckCheck, FIELD_TIME ),
	DEFINE_FIELD( m_flSkinIllume, FIELD_FLOAT ),
	DEFINE_FIELD( m_bIsTorching, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_iSkinColor, FIELD_INTEGER, "skincolor" ),

	// Inputs
	//DEFINE_INPUTFUNC( FIELD_STRING, "FlyAway", InputFlyAway ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CNPC_Dragon, DT_Npc_Dragon )
	SendPropFloat( SENDINFO( m_flSkinIllume ) ),
	SendPropBool( SENDINFO( m_bIsTorching ) ),
	SendPropInt( SENDINFO( m_iSkinColor ) ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Dragon::Spawn( void )
{

	Precache();

	SetModel( DRAGON_MODEL );
	AddSpawnFlags( SF_NPC_LONG_RANGE );

	SetHullType(HULL_WIDE_SHORT);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_STEP );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetNavType( NAV_GROUND );

	m_flFieldOfView = VIEW_FIELD_FULL;
	SetViewOffset( Vector(6, 0, 11) );		// Position of the eyes relative to NPC's origin.

	CapabilitiesClear();
	
	//Only do this if a squadname appears in the entity
	if ( m_SquadName != NULL_STRING )
	{
		CapabilitiesAdd( bits_CAP_SQUAD );
	}
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_RANGE_ATTACK1 );

	m_flGroundIdleMoveTime = gpGlobals->curtime + random->RandomFloat( 0.0f, 5.0f );

	SetBloodColor( DONT_BLEED );
	m_NPCState = NPC_STATE_NONE;

	SetCollisionGroup( COLLISION_GROUP_NPC );

	m_iHealth = sk_dragon_health.GetFloat();
	m_iMaxHealth = m_iHealth;
	m_flFieldOfView	= DRAGON_FOV_NORMAL;

	// We don't mind zombies/penguins so much.
	//AddClassRelationship( CLASS_ZOMBIE, D_NU, 0 );

	if( m_iSkinColor == 0 )
		m_nSkin = 0;
	else if (m_iSkinColor == 1 )
		m_nSkin = 1;
	else
		m_nSkin = 2;

	// Do not dissolve
	AddEFlags( EFL_NO_DISSOLVE );

	NPCInit();

	BaseClass::Spawn();

	m_vLastStoredOrigin = vec3_origin;
	m_flLastStuckCheck = gpGlobals->curtime;

	SetGoalEnt( NULL );

	m_flNextTorchTrace = gpGlobals->curtime;
	m_flNextTorchAttack =  gpGlobals->curtime;

	m_bIsAlert = false;

	m_flSkinIllume = 0.0f;
	m_bIsTorching = false;

	m_nPoseFaceVert = LookupPoseParameter( "turnhead_vert" );
	m_nPoseFaceHoriz = LookupPoseParameter( "turnhead" );
}


//-----------------------------------------------------------------------------
// Purpose: Returns this monster's classification in the relationship table.
//-----------------------------------------------------------------------------
Class_T	CNPC_Dragon::Classify( void )
{
	return( CLASS_ANTLION ); // was CLASS_EARTH_FAUNA, using CLASS_ANTLION for now 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEnemy - 
//-----------------------------------------------------------------------------
void CNPC_Dragon::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	ClearCondition( COND_DRAGON_LOCAL_MELEE_OBSTRUCTION );

	m_flEnemyDist = (GetLocalOrigin() - pEnemy->GetLocalOrigin()).Length();

	if( gpGlobals->curtime >= m_flNextAttack )
	{
		if( (m_flEnemyDist > RANGE_MIN_DISTANCE) &&
			(m_flEnemyDist < RANGE_MAX_DISTANCE) )
		{
			// trace from mouth, to see if an ally is in the way
			Vector vecMouthPos;

			if( GetAttachment( "mouth", vecMouthPos ) )
			{
				trace_t tr;
				AI_TraceLine ( vecMouthPos, (pEnemy->GetAbsOrigin() - Vector(150)), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );

				//NDebugOverlay::Line( EyePosition(), (pEnemy->GetAbsOrigin() - 150), 0, 0, 255, true, 0.1f );
				if ( tr.fraction != 1.0f)
					SetCondition( COND_DRAGON_ENEMY_GOOD_RANGE_DISTANCE );
			}
			m_flNextAttack = gpGlobals->curtime + RandomFloat( 1 , 3 );
		}
		else if ( m_flEnemyDist < MELEE_MAX_DISTANCE )
			SetCondition( COND_DRAGON_ENEMY_GOOD_MELEE_DISTANCE );
		else
			SetCondition( COND_DRAGON_ENEMY_TOO_FAR );
	}

	BaseClass::GatherEnemyConditions(pEnemy);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : posSrc - 
// Output : Vector
//-----------------------------------------------------------------------------
Vector CNPC_Dragon::BodyTarget( const Vector &posSrc, bool bNoisy ) 
{ 
	Vector vecResult;
	vecResult = GetAbsOrigin();
	vecResult.z += 6;
	return vecResult;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Dragon::StopLoopingSounds( void )
{
	//
	// Stop whatever flap sound might be playing.
	//
	if ( m_bPlayedLoopingSound )
		StopSound( "Missile.Accelerate" );

	BaseClass::StopLoopingSounds();
}


//-----------------------------------------------------------------------------
// Purpose: Catches the monster-specific messages that occur when tagged
//			animation frames are played.
// Input  : pEvent - 
//-----------------------------------------------------------------------------
void CNPC_Dragon::HandleAnimEvent( animevent_t *pEvent )
{
	// Footprints in da snow
	if ( pEvent->event == AE_DRAGON_FOOTPRINT_LEFT )
	{
		SnowFootPrint( true, false );
		return;
	}
	if ( pEvent->event == AE_DRAGON_FOOTPRINT_RIGHT )
	{
		SnowFootPrint( false, false );
		return;
	}
	if ( pEvent->event == AE_DRAGON_FOOTPRINT_FRONTLEFT )
	{
		SnowFootPrint( true, true );
		return;
	}
	if ( pEvent->event == AE_DRAGON_FOOTPRINT_FRONTRIGHT )
	{
		SnowFootPrint( false, true );
		return;
	}

	// attacks
	if ( pEvent->event == AE_DRAGON_MELEE_HEADBUT )
	{
		Vector right, forward;
		AngleVectors( GetLocalAngles(), &forward, &right, NULL );
		right = right * 100;
		forward = forward * 200;

		ClawAttack( 55, sk_dragon_melee_dmg.GetFloat(), QAngle( -15, -20, -10 ), right + forward );
		return;
	}
	if ( pEvent->event == AE_DRAGON_RANGE_TORCH )
	{
		if(!m_bParticle)
		{
			m_bParticle = true;
			m_bPlayedLoopingSound = true;
			EmitSound( "Missile.Accelerate" );
			int iAttachment = LookupAttachment( "mouth" );
			DispatchParticleEffect( "flamethrower", PATTACH_POINT_FOLLOW, this, iAttachment, false );

			m_flNextTorchAttack = gpGlobals->curtime + RandomInt(7, 12); 
			SetTorchBool( true );
		}
		return;
	}
	if ( pEvent->event == AE_DRAGON_RANGE_TORCH_END )
	{
		if(m_bParticle)
		{
			m_bParticle = false;
			StopLoopingSounds();
			StopParticleEffects( this ); // end the particle
			VacateStrategySlot();

			SetTorchBool( false );
		}
		return;
	}


	CAI_BaseNPC::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eNewActivity - 
//-----------------------------------------------------------------------------
void CNPC_Dragon::OnChangeActivity( Activity eNewActivity )
{
	bool fRandomize = false;
	if ( eNewActivity == ACT_FLY )
	{
		fRandomize = true;
	}

	BaseClass::OnChangeActivity( eNewActivity );
	if ( fRandomize )
	{
		SetCycle( random->RandomFloat( 0.0, 0.75 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
inline CBaseEntity *CNPC_Dragon::EntityToWatch( void )
{
	return ( GetTarget() != NULL ) ? GetTarget() : GetEnemy();	// Okay if NULL
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flInterval - 
//-----------------------------------------------------------------------------
void CNPC_Dragon::UpdateHead( void )
{
	float yaw = GetPoseParameter( m_nPoseFaceHoriz );
	float pitch = GetPoseParameter( m_nPoseFaceVert );

	// If we should be watching our enemy, turn our head
	if ( ( GetEnemy() != NULL ) && HasCondition( COND_IN_PVS ) )
	{
		Vector	enemyDir = GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter();
		VectorNormalize( enemyDir );
		
		float angle = VecToYaw( BodyDirection3D() );
		float angleDiff = VecToYaw( enemyDir );
		angleDiff = UTIL_AngleDiff( angleDiff, angle + yaw );

		SetPoseParameter( m_nPoseFaceHoriz, UTIL_Approach( yaw + angleDiff, yaw, 50 ) );

		angle = UTIL_VecToPitch( BodyDirection3D() );
		angleDiff = UTIL_VecToPitch( enemyDir );
		angleDiff = UTIL_AngleDiff( angleDiff, angle + pitch );

		SetPoseParameter( m_nPoseFaceVert, UTIL_Approach( pitch + angleDiff, pitch, 50 ) );
	}
	else
	{
		SetPoseParameter( m_nPoseFaceHoriz,	UTIL_Approach( 0, yaw, 10 ) );
		SetPoseParameter( m_nPoseFaceVert, UTIL_Approach( 0, pitch, 10 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles all flight movement because we don't ever build paths when
//			when we are flying.
// Input  : flInterval - Seconds to simulate.
//-----------------------------------------------------------------------------
bool CNPC_Dragon::OverrideMove( float flInterval )
{
	// Update what we're looking at
	UpdateHead();

	// Trace if torching
	if(m_bParticle)
	{
		//DoMuzzleFlash(); // do flashing?
		if(gpGlobals->curtime > m_flNextTorchTrace)
		{
			m_flNextTorchTrace = gpGlobals->curtime + .5f;
			TorchTrace();
		}
	}
	// running?
	if( IsCurSchedule( SCHED_DRAGON_RANGE_ATTACK ) ||
		IsCurSchedule( SCHED_DRAGON_RUN_TO_ATTACK ) ||
		IsCurSchedule( SCHED_DRAGON_MELEE_ATTACK ) )
	{
		m_bIsAlert = true;
	}
	else
		m_bIsAlert = false;

	if(!m_bIsAlert)
	{
		if( IsAlive())
		{
			m_flSkinIllume = RemapVal( sin( gpGlobals->curtime ), -1.0f, 1.0f, 0.05, 1.00 );
		}
		else
		{
			if( m_flSkinIllume > DRAGON_DEAD_ILLUME_MIN)
			{
				m_flSkinIllume -= .0005;
			} 
			else
			{
				if( m_flSkinIllume != DRAGON_DEAD_ILLUME_MIN)
					m_flSkinIllume = DRAGON_DEAD_ILLUME_MIN;
			}
		}
		clamp(m_flSkinIllume, 0, 1);
		SetSkinIllume( m_flSkinIllume );
	}
	else
	{
		float flScale = 0;
		CBasePlayer *pPlayer = ToBasePlayer( GetEnemy() );
		if(pPlayer)
		{
			int m_flEnemyDist = ((pPlayer->GetLocalOrigin() - GetLocalOrigin()).Length());
			flScale = RemapVal( (sin( gpGlobals->curtime * m_flEnemyDist ) * .1f), -1.0f, 1.0f, 1.00, 0.05 );
		}
		clamp(flScale, 0, 1);
		SetSkinIllume( flScale );
	}

	int newColorVal = RemapVal( m_flSkinIllume, 0, 1, 96, 196 );
	SetRenderColor( newColorVal, newColorVal, newColorVal );

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output : 
//-----------------------------------------------------------------------------
float CNPC_Dragon::MaxYawSpeed ( void )
{
	switch ( GetActivity() )
	{
	case ACT_IDLE:			
		return 30;

	case ACT_RUN:
		return 15;
	case ACT_WALK:			
		return 20;

	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		return 40; //15

	case ACT_RANGE_ATTACK1:
		return 0;
	case ACT_MELEE_ATTACK1:
		return 0;

	default:
		return 30;
	}

	return BaseClass::MaxYawSpeed();
}

Activity CNPC_Dragon::NPC_TranslateActivity( Activity eNewActivity )
{
	return BaseClass::NPC_TranslateActivity( eNewActivity );
}


void CNPC_Dragon::TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator)
{
	CTakeDamageInfo	newInfo = info;

	if ( info.GetDamageType() & DMG_PHYSGUN )
	{
		Vector	puntDir = ( info.GetDamageForce() * 5000.0f );

		newInfo.SetDamage( m_iMaxHealth );

		PainSound( newInfo );
		newInfo.SetDamageForce( puntDir );
	}

	BaseClass::TraceAttack(newInfo, vecDir, ptr, pAccumulator);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTask - 
//-----------------------------------------------------------------------------
void CNPC_Dragon::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_DRAGON_PICK_RANDOM_GOAL:
		{
			m_vSavePosition = GetLocalOrigin() + Vector( random->RandomFloat( -48.0f, 48.0f ), random->RandomFloat( -48.0f, 48.0f ), 0 );
			TaskComplete();
			break;
		}
	case TASK_DRAGON_WAIT_POST_RANGE:
		{
			// Don't wait when attacking the player
			if ( GetEnemy() && GetEnemy()->IsPlayer() )
			{
				TaskComplete();
				return;
			}

			// Wait a single think
			SetWait( 0.1 );
		}
		break;
		case TASK_DRAGON_WAIT_POST_MELEE:
		{
			// Don't wait when attacking the player
			if ( GetEnemy() && GetEnemy()->IsPlayer() )
			{
				TaskComplete();
				return;
			}

			// Wait a single think
			SetWait( 0.1 );
		}
		break;
		default:
		{
			BaseClass::StartTask( pTask );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Dragon::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( failedSchedule == SCHED_DRAGON_RANGE_ATTACK )
	{
		VacateStrategySlot();
		return SCHED_DRAGON_RUN_TO_ATTACK;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTask - 
//-----------------------------------------------------------------------------
void CNPC_Dragon::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_DRAGON_WAIT_POST_RANGE:
		{
			if ( IsWaitFinished() )
				TaskComplete();
		}
		break;
		case TASK_DRAGON_WAIT_POST_MELEE:
		{
			if ( IsWaitFinished() )
				TaskComplete();
		}
		break;
		case TASK_WAIT_FOR_MOVEMENT:
		{
			if( IsCurSchedule( SCHED_DRAGON_RUN_TO_ATTACK ) )
			{
				if( !IsStrategySlotRangeOccupied( SQUAD_SLOT_DRAGON_TORCH, SQUAD_SLOT_DRAGON_TORCH ) && gpGlobals->curtime > m_flNextTorchAttack)
				{
					if(HasCondition( COND_DRAGON_ENEMY_GOOD_RANGE_DISTANCE ) )
						TaskComplete();
				}
				if(HasCondition( COND_DRAGON_ENEMY_GOOD_MELEE_DISTANCE ) )
					TaskComplete();
			}
			if( IsCurSchedule( 	SCHED_IDLE_STAND || SCHED_IDLE_WALK || SCHED_IDLE_WANDER ) )
			{
				if(HasCondition( COND_SEE_ENEMY ))
					TaskComplete();
			}
			if ( IsWaitFinished() )
				TaskComplete();
		}
		break;
		default:
		{
			CAI_BaseNPC::RunTask( pTask );
		}
	}
}


//------------------------------------------------------------------------------
// Purpose: Override to do dragon specific gibs.
// Output : Returns true to gib, false to not gib.
//-----------------------------------------------------------------------------
bool CNPC_Dragon::CorpseGib( const CTakeDamageInfo &info )
{
	return true;
}

//-----------------------------------------------------------------------------
// Don't allow ridiculous forces to be applied to the dragon. It only weighs
// 1.5kg, so extreme forces will give it ridiculous velocity.
//-----------------------------------------------------------------------------
//#define DRAGON_RAGDOLL_SPEED_LIMIT	1000.0f  // Dragon ragdoll speed limit in inches per second.
bool CNPC_Dragon::BecomeRagdollOnClient( const Vector &force )
{
	return BaseClass::BecomeRagdollOnClient( force );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CNPC_Dragon::Event_Killed( const CTakeDamageInfo &info )
{
	StopLoopingSounds();
	StopParticleEffects( this );
	if(m_bParticle)
	{
		m_bParticle = false;
	}
	
	VacateStrategySlot();
	SetTorchBool( false );

	BaseClass::Event_Killed( info );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pevInflictor - 
//			pevAttacker - 
//			flDamage - 
//			bitsDamageType - 
//-----------------------------------------------------------------------------
int CNPC_Dragon::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	CTakeDamageInfo newInfo = info;

	// reduce damage, Im hard as nails!
	if(!(info.GetDamageType() & (DMG_FALL | DMG_DROWN )))
	{
		if(info.GetDamage() > (m_iMaxHealth/3) )
		{
			if(m_bParticle)
			{
				m_bParticle = false;
				StopLoopingSounds();
				StopParticleEffects( this ); // end the particle
				VacateStrategySlot();
				SetTorchBool( false );
			}
			AddGesture( ACT_GESTURE_FLINCH_HEAD );
		}

		float flNewDMG = newInfo.GetDamage() * .4f;
		newInfo.SetDamage( flNewDMG );
	}
	
	// TODO: spew a chunk or two?
	return BaseClass::OnTakeDamage_Alive( newInfo );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the best new schedule for this NPC based on current conditions.
//-----------------------------------------------------------------------------
int CNPC_Dragon::SelectSchedule( void )
{
	//
	// If someone we hate is getting a little too close for comfort, avoid them.
	//
	if( GetEnemy() != NULL )
	{
		// attack
		if ( HasCondition( COND_DRAGON_ENEMY_GOOD_RANGE_DISTANCE ) )
		{
			ClearCondition( COND_DRAGON_ENEMY_GOOD_RANGE_DISTANCE );

			if( !IsStrategySlotRangeOccupied( SQUAD_SLOT_DRAGON_TORCH, SQUAD_SLOT_DRAGON_TORCH ) && gpGlobals->curtime > m_flNextTorchAttack)
			{
				OccupyStrategySlot( SQUAD_SLOT_DRAGON_TORCH );
				return SCHED_DRAGON_RANGE_ATTACK;
			}
			else
			{
				return SCHED_DRAGON_RUN_TO_ATTACK;
			}
		}
		if ( HasCondition( COND_DRAGON_ENEMY_GOOD_MELEE_DISTANCE ) )
		{
			ClearCondition( COND_DRAGON_ENEMY_GOOD_MELEE_DISTANCE );
			return SCHED_DRAGON_MELEE_ATTACK;
		}

		return SCHED_DRAGON_RUN_TO_ATTACK;
	}
	else
	{
		if ( HasCondition( COND_HEAR_DANGER ) )
		{
			ClearCondition( COND_HEAR_DANGER );
			return SCHED_DRAGON_IVESTIGATE_SOUND;
		}
		if ( HasCondition( COND_HEAR_COMBAT ) )
		{
			ClearCondition( COND_HEAR_COMBAT );
			return SCHED_DRAGON_IVESTIGATE_SOUND;
		}
	}

	switch ( m_NPCState )
	{
		case NPC_STATE_IDLE:
		case NPC_STATE_ALERT:
		case NPC_STATE_COMBAT:
		{
			//
			// If we are hanging out on the ground, see if it is time to pick a new place to walk to.
			//
			if ( gpGlobals->curtime > m_flGroundIdleMoveTime )
			{
				m_flGroundIdleMoveTime = gpGlobals->curtime + random->RandomFloat( 10.0f, 20.0f );
				return SCHED_DRAGON_IDLE_WALK;
			}

			return SCHED_IDLE_STAND;
		}
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Dragon::RangeAttack1Conditions( float flDot, float flDist )
{
	if ( GetNextAttack() > gpGlobals->curtime )
		return COND_NOT_FACING_ATTACK;

	if ( flDot < DOT_10DEGREE )
		return COND_NOT_FACING_ATTACK;
	
	if ( flDist > RANGE_MAX_DISTANCE )
		return COND_TOO_FAR_TO_ATTACK;

	if ( flDist < RANGE_MIN_DISTANCE )
		return COND_TOO_CLOSE_TO_ATTACK;

	return COND_CAN_RANGE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: For innate melee attack
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Dragon::MeleeAttack1Conditions ( float flDot, float flDist )
{
	float range = MELEE_MAX_DISTANCE;

	if (flDist > range )
	{
		// Translate a hit vehicle into its passenger if found
		if ( GetEnemy() != NULL )
		{
			// If the player is holding an object, knock it down.
			if( GetEnemy()->IsPlayer() )
			{
				CBasePlayer *pPlayer = ToBasePlayer( GetEnemy() );

				Assert( pPlayer != NULL );

				// Is the player carrying something?
				CBaseEntity *pObject = GetPlayerHeldEntity(pPlayer);

				if( !pObject )
				{
					pObject = PhysCannonGetHeldEntity( pPlayer->GetActiveWeapon() );
				}

				if( pObject )
				{
					float flDist = pObject->WorldSpaceCenter().DistTo( WorldSpaceCenter() );

					if( flDist <= MELEE_MAX_DISTANCE ) // Jason 55 hardcoded for now, this is the zombie reach
						return COND_CAN_MELEE_ATTACK1;
				}
			}
		}
		return COND_TOO_FAR_TO_ATTACK;
	}

	if (flDot < 0.7)
	{
		return COND_NOT_FACING_ATTACK;
	}

	// Build a cube-shaped hull, the same hull that ClawAttack() is going to use.
	Vector vecMins = GetHullMins();
	Vector vecMaxs = GetHullMaxs();
	vecMins.z = vecMins.x;
	vecMaxs.z = vecMaxs.x;

	Vector forward;
	GetVectors( &forward, NULL, NULL );

	trace_t	tr;
	CTraceFilterNav traceFilter( this, false, this, COLLISION_GROUP_NONE );
	// Jason 55 hardcoded for now, this is the zombie reach
	AI_TraceHull( WorldSpaceCenter(), WorldSpaceCenter() + forward * MELEE_MAX_DISTANCE, vecMins, vecMaxs, MASK_NPCSOLID, &traceFilter, &tr );

	if( tr.fraction == 1.0 || !tr.m_pEnt )
	{
		// If our trace was unobstructed but we were shooting 
		if ( GetEnemy() && GetEnemy()->Classify() == CLASS_BULLSEYE )
			return COND_CAN_MELEE_ATTACK1;

		// This attack would miss completely. Trick the zombie into moving around some more.
		return COND_TOO_FAR_TO_ATTACK;
	}

	if( tr.m_pEnt == GetEnemy() || 
		tr.m_pEnt->IsNPC() || 
		( tr.m_pEnt->m_takedamage == DAMAGE_YES && (dynamic_cast<CBreakableProp*>(tr.m_pEnt) ) ) )
	{
		// -Let the zombie swipe at his enemy if he's going to hit them.
		// -Also let him swipe at NPC's that happen to be between the zombie and the enemy. 
		//  This makes mobs of zombies seem more rowdy since it doesn't leave guys in the back row standing around.
		// -Also let him swipe at things that takedamage, under the assumptions that they can be broken.
		return COND_CAN_MELEE_ATTACK1;
	}

	Vector vecTrace = tr.endpos - tr.startpos;
	float lenTraceSq = vecTrace.Length2DSqr();

	if ( GetEnemy() && GetEnemy()->MyCombatCharacterPointer() && tr.m_pEnt == static_cast<CBaseCombatCharacter *>(GetEnemy())->GetVehicleEntity() )
	{
		if ( lenTraceSq < Square( MELEE_MAX_DISTANCE * 0.75f ) ) // Jason 55 hardcoded for now, this is the zombie reach
		{
			return COND_CAN_MELEE_ATTACK1;
		}
	}

	if( tr.m_pEnt->IsBSPModel() )
	{
		// The trace hit something solid, but it's not the enemy. If this item is closer to the zombie than
		// the enemy is, treat this as an obstruction.
		Vector vecToEnemy = GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter();

		if( lenTraceSq < vecToEnemy.Length2DSqr() )
		{
			return COND_DRAGON_LOCAL_MELEE_OBSTRUCTION;
		}
	}

	if ( !tr.m_pEnt->IsWorld() && GetEnemy() && GetEnemy()->GetGroundEntity() == tr.m_pEnt )
	{
		//Try to swat whatever the player is standing on instead of acting like a dill.
		return COND_CAN_MELEE_ATTACK1;
	}

	// Bullseyes are given some grace on if they can be hit
	if ( GetEnemy() && GetEnemy()->Classify() == CLASS_BULLSEYE )
		return COND_CAN_MELEE_ATTACK1;

	// Move around some more
	return COND_TOO_FAR_TO_ATTACK;
}

//-----------------------------------------------------------------------------
// Purpose: Look in front and see if the claw hit anything.
//
// Input  :	flDist				distance to trace		
//			iDamage				damage to do if attack hits
//			vecViewPunch		camera punch (if attack hits player)
//			vecVelocityPunch	velocity punch (if attack hits player)
//
// Output : The entity hit by claws. NULL if nothing.
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_Dragon::ClawAttack( float flDist, int iDamage, QAngle &qaViewPunch, Vector &vecVelocityPunch  )
{
	// Added test because claw attack anim sometimes used when for cases other than melee
	int iDriverInitialHealth = -1;
	CBaseEntity *pDriver = NULL;
	if ( GetEnemy() )
	{
		trace_t	tr;
		AI_TraceHull( WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), -Vector(8,8,8), Vector(8,8,8), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction < 1.0f )
			return NULL;

		// CheckTraceHullAttack() can damage player in vehicle as side effect of melee attack damaging physics objects, which the car forwards to the player
		// need to detect this to get correct damage effects
		CBaseCombatCharacter *pCCEnemy = ( GetEnemy() != NULL ) ? GetEnemy()->MyCombatCharacterPointer() : NULL;
		CBaseEntity *pVehicleEntity;
		if ( pCCEnemy != NULL && ( pVehicleEntity = pCCEnemy->GetVehicleEntity() ) != NULL )
		{
			if ( pVehicleEntity->GetServerVehicle() && dynamic_cast<CPropVehicleDriveable *>(pVehicleEntity) )
			{
				pDriver = static_cast<CPropVehicleDriveable *>(pVehicleEntity)->GetDriver();
				if ( pDriver && pDriver->IsPlayer() )
				{
					iDriverInitialHealth = pDriver->GetHealth();
				}
				else
				{
					pDriver = NULL;
				}
			}
		}
	}

	//
	// Trace out a cubic section of our hull and see what we hit.
	//
	Vector vecMins = GetHullMins();
	Vector vecMaxs = GetHullMaxs();
	vecMins.z = vecMins.x;
	vecMaxs.z = vecMaxs.x;

	CBaseEntity *pHurt = NULL;
	if ( GetEnemy() && GetEnemy()->Classify() == CLASS_BULLSEYE )
	{ 
		// We always hit bullseyes we're targeting
		pHurt = GetEnemy();
		CTakeDamageInfo info( this, this, vec3_origin, GetAbsOrigin(), iDamage, DMG_SLASH );
		pHurt->TakeDamage( info );
	}
	else 
	{
		// Try to hit them with a trace
		pHurt = CheckTraceHullAttack( flDist, vecMins, vecMaxs, iDamage, DMG_SLASH );
	}

	if ( pDriver && iDriverInitialHealth != pDriver->GetHealth() )
	{
		pHurt = pDriver;
	}

	if ( pHurt )
	{
			AttackHitSound();

			CBasePlayer *pPlayer = ToBasePlayer( pHurt );

			if ( pPlayer != NULL && !(pPlayer->GetFlags() & FL_GODMODE ) )
			{
				pPlayer->ViewPunch( qaViewPunch );
			
				pPlayer->VelocityPunch( vecVelocityPunch );
			}
			else if( !pPlayer && UTIL_ShouldShowBlood(pHurt->BloodColor()) )
			{
				// Hit an NPC. Bleed them!
				Vector vecBloodPos;

				if( GetAttachment( "head", vecBloodPos ) )
						SpawnBlood( vecBloodPos, g_vecAttackDir, pHurt->BloodColor(), min( iDamage, 30 ) );
			}
	}
	else 
	{
		AttackMissSound();
	}

	m_flNextAttack = gpGlobals->curtime + random->RandomFloat( 1, 2 );

	return pHurt;
}

//-----------------------------------------------------------------------------
// Purpose: Look in front and see if the claw hit anything.
//
// Input  :	flDist				distance to trace		
//			iDamage				damage to do if attack hits
//			vecViewPunch		camera punch (if attack hits player)
//			vecVelocityPunch	velocity punch (if attack hits player)
//
// Output : The entity hit by claws. NULL if nothing.
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_Dragon::TorchTrace( void )
{
	// Added test because claw attack anim sometimes used when for cases other than melee
	int iDriverInitialHealth = -1;
	CBaseEntity *pDriver = NULL;
	if ( GetEnemy() )
	{
		trace_t	tr;
		AI_TraceHull( WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), -Vector(8,8,8), Vector(8,8,8), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction < 1.0f )
			return NULL;

		// CheckTraceHullAttack() can damage player in vehicle as side effect of melee attack damaging physics objects, which the car forwards to the player
		// need to detect this to get correct damage effects
		CBaseCombatCharacter *pCCEnemy = ( GetEnemy() != NULL ) ? GetEnemy()->MyCombatCharacterPointer() : NULL;
		CBaseEntity *pVehicleEntity;
		if ( pCCEnemy != NULL && ( pVehicleEntity = pCCEnemy->GetVehicleEntity() ) != NULL )
		{
			if ( pVehicleEntity->GetServerVehicle() && dynamic_cast<CPropVehicleDriveable *>(pVehicleEntity) )
			{
				pDriver = static_cast<CPropVehicleDriveable *>(pVehicleEntity)->GetDriver();
				if ( pDriver && pDriver->IsPlayer() )
					iDriverInitialHealth = pDriver->GetHealth();
				else
					pDriver = NULL;
			}
		}
	}

	//
	// Trace out a cubic section of our hull and see what we hit.
	//
	Vector vecMins = GetHullMins();
	Vector vecMaxs = GetHullMaxs();
	vecMins.z = vecMins.x;
	vecMaxs.z = vecMaxs.x;

	float iDamage = sk_dragon_torch_dmg.GetFloat();

	CBaseEntity *pHurt = NULL;
	if ( GetEnemy() && GetEnemy()->Classify() == CLASS_BULLSEYE )
	{ 
		// We always hit bullseyes we're targeting
		pHurt = GetEnemy();
		CTakeDamageInfo info( this, this, vec3_origin, GetAbsOrigin(), iDamage, DMG_BURN );
		pHurt->TakeDamage( info );
	}
	else 
	{
		// Try to hit them with a trace
		pHurt = CheckTraceHullAttack( DRAGON_TORCH_DISTANCE, vecMins, vecMaxs, iDamage, DMG_BURN );
	}

	if ( pDriver && iDriverInitialHealth != pDriver->GetHealth() )
	{
		pHurt = pDriver;
	}

	if ( pHurt )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pHurt );

		if ( pPlayer != NULL && !(pPlayer->GetFlags() & FL_GODMODE ) )
		{
			// burn player?
			pPlayer->Ignite(1.5, false, 1, false);
			
			//Red damage indicator
			color32 red = {128,0,0,128};
			UTIL_ScreenFade( pHurt, red, 1.0f, 0.1f, FFADE_IN );
		}
	}

	return pHurt;
}

//-----------------------------------------------------------------------------
// Purpose: Footprints left and right
// Output :
//-----------------------------------------------------------------------------
void CNPC_Dragon::SnowFootPrint( bool IsLeft, bool IsFront )
{
	trace_t tr;
	Vector traceStart;
	QAngle angles;

	int attachment;

	if(	IsFront )
	{
		if( IsLeft )
		{
			EmitSound( "Flesh.StepLeft" );
			attachment = this->LookupAttachment( "frontleftfoot" );
		}
		else
		{
			EmitSound( "Flesh.StepRight" );
			attachment = this->LookupAttachment( "frontrightfoot" );
		}
	}
	else
	{
		if( IsLeft )
		{
			EmitSound( "Flesh.StepLeft" );
			attachment = this->LookupAttachment( "leftfoot" );
		}
		else
		{
			EmitSound( "Flesh.StepRight" );
			attachment = this->LookupAttachment( "rightfoot" );
		}
	}
	if( attachment == -1 )
	{
		// Exit if this NPC doesn't have the proper attachments.
		return;
	}

	this->GetAttachment( attachment, traceStart, angles );

	UTIL_TraceLine( traceStart, traceStart - Vector( 0, 0, 48.0f), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
	if( tr.fraction < 1.0 && tr.m_pEnt )
	{
		surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
		if( psurf )
		{
			// Can't plant footprints on fake materials (ladders, wading)
			if ( psurf->game.material != 'X' )
			{
				int footprintDecal = -1;
	   
				// NOTE: We could add in snow, mud, others here
				switch(psurf->game.material)
				{
				case 'J': // ICEMOD snow, defind in decals.h
					footprintDecal = 1;
					break;
				}

				if (footprintDecal != -1)
				{
					Vector right;
					AngleVectors( GetAbsAngles(), 0, &right, 0 );//AngleVectors( angles, 0, &right, 0 ); <-- GetAbsAngles() fixed the "straigt" issue

					// Figure out where the top of the stepping leg is 
					//trace_t tr;
					Vector hipOrigin;

					VectorMA( this->GetAbsOrigin(), 
						IsLeft ? -12 : 12, // 8 = half width
						right, hipOrigin );

					// Find where that leg hits the ground
					UTIL_TraceLine( hipOrigin, hipOrigin + Vector(0, 0, -COORD_EXTENT * 1.74), 
						MASK_SOLID_BRUSHONLY,
						this,
						COLLISION_GROUP_NONE,
						&tr );

					// plant the decal
					CPVSFilter filter( tr.endpos );

					// get decal index number by name
					int fpindex = FootPrintDecal(); // get the decal by script name
					te->FootprintDecal( filter, 0.0f, &tr.endpos, &right, 0, //0 = COLLISION_GROUP_NONE?
										fpindex, 'J' ); // J can be something else if this has any issues, 0 = fpindex
				}
			} 
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Dragon::Precache( void )
{
	BaseClass::Precache();
	
	PrecacheModel( DRAGON_MODEL );
	PrecacheParticleSystem( "flamethrower" );

	//Dragon
	PrecacheScriptSound( "NPC_FastHeadcrab.Attack" );
	PrecacheScriptSound( "NPC_PoisonZombie.Idle" ); //NPC_PoisonZombie.Alert
	PrecacheScriptSound( "NPC_PoisonZombie.Attack" ); //Alert
	PrecacheScriptSound( "NPC_PoisonZombie.Die" ); //NPC_Crow.Die
	PrecacheScriptSound( "NPC_PoisonZombie.Pain" );

	// torching
	PrecacheScriptSound( "Missile.Accelerate" );

	// stepping
	PrecacheScriptSound( "Flesh.StepRight" );
	PrecacheScriptSound( "Flesh.StepLeft" );

	// attack sounds
	PrecacheScriptSound( "Zombie.AttackHit" );
	PrecacheScriptSound( "Zombie.AttackMiss" );
	PrecacheScriptSound( "NPC_Antlion.RunOverByVehicle" );
}


//-----------------------------------------------------------------------------
// Purpose: Sounds.
//-----------------------------------------------------------------------------
void CNPC_Dragon::IdleSound( void )
{
	EmitSound( "NPC_PoisonZombie.Idle" );
}


void CNPC_Dragon::AlertSound( void )
{
	EmitSound( "NPC_PoisonZombie.Attack" );
}


void CNPC_Dragon::PainSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_PoisonZombie.Pain" );
}


void CNPC_Dragon::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_PoisonZombie.Die" );
}

// attack sounds
//-----------------------------------------------------------------------------
// Purpose: Play a random attack hit sound
//-----------------------------------------------------------------------------
void CNPC_Dragon::AttackHitSound( void )
{
	EmitSound( "Zombie.AttackHit" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack miss sound
//-----------------------------------------------------------------------------
void CNPC_Dragon::AttackMissSound( void )
{
	// Play a random attack miss sound
	EmitSound( "Zombie.AttackMiss" );
}


//---------------------------------------------------------
//---------------------------------------------------------
int CNPC_Dragon::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
		case SCHED_CHASE_ENEMY:
			if ( HasCondition( COND_DRAGON_LOCAL_MELEE_OBSTRUCTION ) && !HasCondition(COND_TASK_FAILED) && (IsCurSchedule( SCHED_DRAGON_RUN_TO_ATTACK ), false ) )
			{
				return SCHED_COMBAT_PATROL;
			}
			return SCHED_DRAGON_RUN_TO_ATTACK;
			break;
		case SCHED_MELEE_ATTACK1:
			return SCHED_DRAGON_MELEE_ATTACK;
		case SCHED_RANGE_ATTACK1:
			return SCHED_DRAGON_RANGE_ATTACK;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Dragon::DrawDebugTextOverlays( void )
{
	int nOffset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];
		EntityText( nOffset, tempstr, 0 );
		nOffset++;

		if ( GetEnemy() != NULL )
		{
			Q_snprintf( tempstr, sizeof( tempstr ), "enemy (dist): %s (%g)", GetEnemy()->GetClassname(), ( double )m_flEnemyDist );
			EntityText( nOffset, tempstr, 0 );
			nOffset++;
		}
	}

	return nOffset;
}


//-----------------------------------------------------------------------------
// Purpose: Determines which sounds the dragon cares about.
//-----------------------------------------------------------------------------
int CNPC_Dragon::GetSoundInterests( void )
{
	return	SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_DANGER;
}


//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_dragon, CNPC_Dragon )
	
	DECLARE_SQUADSLOT( SQUAD_SLOT_DRAGON_TORCH )

	DECLARE_TASK( TASK_DRAGON_WAIT_POST_MELEE )
	DECLARE_TASK( TASK_DRAGON_WAIT_POST_RANGE )
	DECLARE_TASK( TASK_DRAGON_PICK_RANDOM_GOAL )

	DECLARE_ANIMEVENT( AE_DRAGON_MELEE_HEADBUT )
	DECLARE_ANIMEVENT( AE_DRAGON_RANGE_TORCH )
	DECLARE_ANIMEVENT( AE_DRAGON_RANGE_TORCH_END )
	DECLARE_ANIMEVENT( AE_DRAGON_FOOTPRINT_RIGHT )
	DECLARE_ANIMEVENT( AE_DRAGON_FOOTPRINT_LEFT )
	DECLARE_ANIMEVENT( AE_DRAGON_FOOTPRINT_FRONTRIGHT )
	DECLARE_ANIMEVENT( AE_DRAGON_FOOTPRINT_FRONTLEFT )

	DECLARE_CONDITION( COND_DRAGON_ENEMY_TOO_FAR )
	DECLARE_CONDITION( COND_DRAGON_ENEMY_GOOD_RANGE_DISTANCE )
	DECLARE_CONDITION( COND_DRAGON_ENEMY_GOOD_MELEE_DISTANCE )
	DECLARE_CONDITION( COND_DRAGON_LOCAL_MELEE_OBSTRUCTION )


	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_DRAGON_IDLE_WALK,
		
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_IDLE_STAND"
		"		TASK_DRAGON_PICK_RANDOM_GOAL		0"
		"		TASK_GET_PATH_TO_SAVEPOSITION	0"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_WAIT_PVS					0"
		"		"
		"	Interrupts"
		"		COND_PROVOKED"
		"		COND_NEW_ENEMY"
		"		COND_HEAVY_DAMAGE"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK1"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_DRAGON_IVESTIGATE_SOUND,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_STORE_BESTSOUND_REACTORIGIN_IN_SAVEPOSITION		0"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_SAVEPOSITION		0"
		"		TASK_MOVE_TO_TARGET_RANGE			350"
		"		TASK_STOP_MOVING			0"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_DRAGON_POST_RANGE_WAIT"
		""
		"	Interrupts"
		"		COND_SEE_ENEMY"
		"		COND_PROVOKED"
		"		COND_NEW_ENEMY"
		"		COND_HEAVY_DAMAGE"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_LOST_PLAYER"
		"		COND_SCHEDULE_DONE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_DRAGON_RUN_TO_ATTACK,
		
		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
		"		 TASK_SET_TOLERANCE_DISTANCE	64"
		"		 TASK_GET_CHASE_PATH_TO_ENEMY	600"
		"		 TASK_RUN_PATH					0"
		"		 TASK_WAIT_FOR_MOVEMENT			0"
		"		 TASK_FACE_ENEMY				0"

		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_TASK_FAILED"
		"		COND_SCHEDULE_DONE"
	)
	//=========================================================

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_DRAGON_MELEE_ATTACK,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		0" 
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_DRAGON_POST_MELEE_WAIT"
		""
		"	Interrupts"
	)
	DEFINE_SCHEDULE
	(
		SCHED_DRAGON_POST_MELEE_WAIT,

		"	Tasks"
		"		TASK_DRAGON_WAIT_POST_MELEE		0"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_DRAGON_RANGE_ATTACK,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_RANGE_ATTACK1		0"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_DRAGON_POST_RANGE_WAIT"
		""
		"	Interrupts"
		"		COND_HEAVY_DAMAGE"
	)
	DEFINE_SCHEDULE
	(
		SCHED_DRAGON_POST_RANGE_WAIT,

		"	Tasks"
		"		TASK_DRAGON_WAIT_POST_RANGE		0"
	)


AI_END_CUSTOM_NPC()
