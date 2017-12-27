//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Malamute, a cute pup in the rebel base that just plays around
//
//=============================================================================//

#include "cbase.h"

#include "ai_baseactor.h"
#include "ai_hull.h"
#include "ai_navigator.h" // just added

#include "ammodef.h"
#include "gamerules.h"
#include "IEffects.h"
#include "engine/IEngineSound.h"

#include "ai_behavior.h"
#include "ai_behavior_follow.h" //
#include "ai_behavior_assault.h"
#include "ai_behavior_lead.h"

#include "npcevent.h"
#include "ai_playerally.h"
#include "ai_senses.h"
#include "soundent.h"

// for footprints
#include "decals.h"
// for carrying object
#include "weapon_physcannon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	MALAMUTE_MODEL		"models/malamute/malamute.mdl"
#define MALAMUTE_FETCH_WEIGHT 4.5 // Exact mass of ball physics prop
#define BALL_MIN_TOSS_DISTANCE 128

// SPAWN FLAGS
#define SF_MALAMUTE_NCHWL			( 1 << 16 )	//65536 - Never Call Home Wander Limit
#define	SF_MALAMUTE_SFP				( 1 << 17 )	//131072 - Start Following Player


//ConVar malamute_headshot_freq( "malamute_headshot_freq", "2" );

// Activities
Activity ACT_MALAMUTE_HAPPYIDLE;
Activity ACT_MALAMUTE_HAPPYPAUSE;
Activity ACT_MALAMUTE_SIT;
Activity ACT_MALAMUTE_STAND;
Activity ACT_MALAMUTE_SIT_IDLE;
Activity ACT_MALAMUTE_SIT_IDLE_B;
Activity ACT_MALAMUTE_SIT_PANT;
Activity ACT_MALAMUTE_SIT_BARK;
Activity ACT_MALAMUTE_BARK;
Activity ACT_MALAMUTE_BARKEXCITED;
Activity ACT_MALAMUTE_PICKUPITEM;
Activity ACT_MALAMUTE_DROPITEM;

// Animation events
int AE_MALAMUTE_STANDING;
int AE_MALAMUTE_SITTING;
int AE_MALAMUTE_BARK;
int AE_MALAMUTE_PANT;
int AE_MALAMUTE_PICKUPITEM;
int AE_MALAMUTE_DROPITEM;
int AE_MALAMUTE_FOOTPRINT_FRIGHT;
int AE_MALAMUTE_FOOTPRINT_FLEFT;
int AE_MALAMUTE_FOOTPRINT_RIGHT;
int AE_MALAMUTE_FOOTPRINT_LEFT;

class CNPC_Malamute : public CAI_PlayerAlly
{
	DECLARE_CLASS( CNPC_Malamute, CAI_PlayerAlly );

public:

	CNPC_Malamute() {}
	void Spawn();
	void Precache();

	bool CreateBehaviors();
	int GetSoundInterests();
	void BuildScheduleTestBits( void );
	Class_T	Classify( void );

	void PostNPCInit();
	int	SelectSchedule();

	void HandleAnimEvent( animevent_t *pEvent );
	Activity NPC_TranslateActivity( Activity eNewActivity );

	void PainSound( const CTakeDamageInfo &info );
	void DeathSound( const CTakeDamageInfo &info );

	void GatherConditions( void );

	void StartTask( const Task_t *pTask );
	void RunTask( const Task_t *pTask );

	float MaxYawSpeed ( void );
	void UpdateHead( void );
	bool OverrideMove( float flInterval );

	int SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );

	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;

	// Inputs
	void	InputReturnToOrigin ( inputdata_t &inputdata );

	CAI_AssaultBehavior		m_AssaultBehavior;
	CAI_LeadBehavior		m_LeadBehavior;
	CAI_FollowBehavior		m_FollowBehavior;

private:
	void SnowFootPrint( bool IsLeft, bool IsFront );
	int FootPrintDecal( void ){ int fpdid = decalsystem->GetDecalIndexForName( "FootPrintMalamute" ); return fpdid; };

	Vector					m_vSpawnOrigin;

	bool					m_bStartPosture;
	int						m_iMaxWander;

	bool					m_bMournedPlayer;
	bool					m_bExcited;
	bool					m_bRunHome;
	bool					m_bSitting;
	int						m_iNextWander;
	bool					m_bFirstWander;
	
	int						m_iPO_State; //0:Static, 1:PlayerHeld, 2:PlayerRemoved, 3:GoodFetchDistance, 4:HeldByMalamute
	int						m_iPO_NextStatic;

	bool					m_bSwitchSit;
	int						m_iNextSit;


// Conditions
enum
{
	COND_MALAMUTE_NONE = BaseClass::NEXT_CONDITION,
	COND_MALAMUTE_SIT,
	COND_MALAMUTE_STAND,
	COND_MALAMUTE_CALLED_HOME,
	COND_MALAMUTE_CLOSEPLAYER,
	COND_MALAMUTE_FARPLAYER,
	COND_MALAMUTE_CHASE,
	COND_MALAMUTE_WATCHOBJECT,
	COND_MALAMUTE_FETCH,
};
// Scheduals
enum
{
	SCHED_MALAMUTE_NONE = BaseClass::NEXT_SCHEDULE,
	SCHED_MALAMUTE_SIT,
	SCHED_MALAMUTE_STAND,
	SCHED_MALAMUTE_WANDER,
	SCHED_MALAMUTE_RUN_TO_GREET_PLAYER,
	SCHED_MALAMUTE_RETURN_TO_ORIGIN,
	SCHED_MALAMUTE_BARK,
	SCHED_MALAMUTE_STAND_BARK,
	SCHED_MALAMUTE_WATCHOBJECT,
	SCHED_MALAMUTE_FETCH,
	SCHED_MALAMUTE_GRAB_RETURN,
	SCHED_MALAMUTE_DROPOBJECT,
};
// Tasks
enum
{
	TASK_MALAMUTE_NONE = BaseClass::NEXT_TASK,
	TASK_MALAMUTE_STAND,
	TASK_MALAMUTE_SIT,
	TASK_MALAMUTE_SET_GOAL,
	TASK_MALAMUTE_BARK,
	TASK_WATCH_OBJECT,
	TASK_MALAMUTE_SET_ITEM_GOAL,
	TASK_MALAMUTE_PICKUPITEM,
	TASK_MALAMUTE_DROPITEM,
};
// POS - Physic Object State ( state of the ball )
enum
{
	POS_STATIC,
	POS_PLAYER_OCCUPIED,
	POS_TOSSED,
	POS_MALAMUTE_JUDGING,
	POS_FETCHABLE,
	POS_MALAMUTE_FETCHING,
	POS_MALAMUTE_OCCUPIED,
};


protected:
	// Pose parameters
	int					m_nPoseFaceVert;
	int					m_nPoseFaceHoriz;
};

BEGIN_DATADESC( CNPC_Malamute )
	DEFINE_FIELD( m_bExcited, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMournedPlayer, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bRunHome, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bSitting, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iPO_State, FIELD_INTEGER ),
	DEFINE_FIELD( m_iPO_NextStatic, FIELD_TIME ),
	DEFINE_FIELD( m_iNextWander, FIELD_TIME ),
	DEFINE_KEYFIELD( m_bStartPosture, FIELD_BOOLEAN, "startsit" ),
	DEFINE_FIELD( m_bFirstWander, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_iMaxWander, FIELD_INTEGER, "maxwander" ),
	DEFINE_FIELD( m_bSwitchSit, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iNextSit, FIELD_TIME ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID,	"RunBackToSpawnPoint", InputReturnToOrigin ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_malamute, CNPC_Malamute );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Malamute::CreateBehaviors()
{
	return BaseClass::CreateBehaviors();
}

//PostNPCInit-----------------------------------------------------------------------------
void CNPC_Malamute::PostNPCInit()
{
	BaseClass::PostNPCInit();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Malamute::GetSoundInterests()
{
	return	SOUND_PLAYER;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Malamute::BuildScheduleTestBits( void )
{
	// Don't interrupt while braking
	const Task_t* pTask = GetTask();
	if ( pTask && (pTask->iTask == TASK_MALAMUTE_BARK) )
	{
		ClearCustomInterruptCondition( COND_HEAVY_DAMAGE );
		ClearCustomInterruptCondition( COND_ENEMY_OCCLUDED );
		ClearCustomInterruptCondition( COND_HEAR_DANGER );
		ClearCustomInterruptCondition( COND_WEAPON_BLOCKED_BY_FRIEND );
		ClearCustomInterruptCondition( COND_WEAPON_SIGHT_OCCLUDED );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Class_T	CNPC_Malamute::Classify( void )
{
	return CLASS_PLAYER_ALLY;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_Malamute::NPC_TranslateActivity( Activity newActivity )
{
	newActivity = BaseClass::NPC_TranslateActivity( newActivity );

	if (m_bExcited)
	{
		if( newActivity == ACT_WALK )
			return ACT_RUN;
		if( newActivity == ACT_IDLE )
		{
			int rInt = RandomInt(0, 10);
			if( m_bSitting )
			{
				if( rInt == 0)
					return ACT_MALAMUTE_SIT_IDLE_B;
				else
					return ACT_MALAMUTE_SIT_PANT;
			}
			else
			{
				if( rInt <= 4)
					return ACT_MALAMUTE_HAPPYIDLE;
				else if( rInt >= 8)
					return ACT_MALAMUTE_HAPPYPAUSE;
				else
					return ACT_IDLE;
			}
		}
		if( m_bSitting )
		{
			if( newActivity == ACT_TURN_LEFT )
				return ACT_MALAMUTE_SIT_IDLE;
			if( newActivity == ACT_TURN_RIGHT )
				return ACT_MALAMUTE_SIT_IDLE;
			if( newActivity == ACT_MALAMUTE_BARK )
				return ACT_MALAMUTE_SIT_BARK;
		}
		else
		{
			if( newActivity == ACT_MALAMUTE_BARK )
				return ACT_MALAMUTE_BARKEXCITED;
		}
	}
	else
	{
		if( m_bSitting )
		{
			if( newActivity == ACT_TURN_LEFT )
				return ACT_MALAMUTE_SIT_IDLE;
			if( newActivity == ACT_TURN_RIGHT )
				return ACT_MALAMUTE_SIT_IDLE;
			if( newActivity == ACT_IDLE )
			{
				int rInt = RandomInt(0, 10);
				if( rInt == 10)
					return ACT_MALAMUTE_SIT_IDLE_B;
				else
					return ACT_MALAMUTE_SIT_IDLE;
			}
			if( newActivity == ACT_MALAMUTE_BARK )
				return ACT_MALAMUTE_SIT_BARK;
		}
	}

	return newActivity;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Malamute::Precache()
{
	PrecacheModel( MALAMUTE_MODEL );
	
	PrecacheScriptSound( "NPC_Citizen.FootstepLeft" );
	PrecacheScriptSound( "NPC_Citizen.FootstepRight" );

	PrecacheScriptSound( "NPC_Malamute.Bark" );
	PrecacheScriptSound( "NPC_Malamute.Pant" );
	PrecacheScriptSound( "NPC_Malamute.CallHome" );

	BaseClass::Precache();
}
 

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Malamute::Spawn()
{
	Precache();

	BaseClass::Spawn();

	SetModel( MALAMUTE_MODEL );

	SetHullType(HULL_WIDE_SHORT);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetBloodColor( BLOOD_COLOR_RED );

	SetDefaultEyeOffset(); // new
	SetNavType( NAV_GROUND ); // new

	m_iHealth			= 100;
	m_NPCState			= NPC_STATE_NONE; // new

	m_flFieldOfView = -0.4f;
	SetViewOffset( Vector(6, 0, 11) );		// Position of the eyes relative to NPC's origin.

	CapabilitiesAdd( bits_CAP_TURN_HEAD | bits_CAP_MOVE_GROUND );
	CapabilitiesAdd( bits_CAP_FRIENDLY_DMG_IMMUNE );

	// set a save position
	// we will use this to run back to when the player calls our 'return to origin' function
	m_vSpawnOrigin = GetAbsOrigin(); //m_vSpawnOrigin, m_vSavePosition

	if ( ( m_spawnflags & SF_MALAMUTE_SFP ) && AI_IsSinglePlayer() )
	{
		m_bExcited = true;
	}
	else
	{
		m_bExcited = false;
	}

	m_iPO_State = POS_STATIC;
	m_iPO_NextStatic = gpGlobals->curtime;
	
	m_bFirstWander = true;	
	m_bSwitchSit = false;
	m_iNextSit = gpGlobals->curtime;
	
	m_bSitting = !m_bStartPosture;

	// Fix floating at start glitch
	trace_t tr;
	UTIL_TraceHull( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -512 ), NAI_Hull::Mins( GetHullType() ), NAI_Hull::Maxs( GetHullType() ), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
	SetAbsOrigin( tr.endpos );

	m_nPoseFaceVert = LookupPoseParameter( "turnhead_vert" );
	m_nPoseFaceHoriz = LookupPoseParameter( "turnhead" );

	NPCInit();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_Malamute::PainSound( const CTakeDamageInfo &info )
{
	SpeakIfAllowed( TLK_WOUND );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_Malamute::DeathSound( const CTakeDamageInfo &info )
{
	// Sentences don't play on dead NPCs
	SentenceStop();

	Speak( TLK_DEATH );
}

//
void CNPC_Malamute::InputReturnToOrigin( inputdata_t &inputdata )
{
	SetCondition( COND_MALAMUTE_CALLED_HOME );
}

//-----------------------------------------------------------------------------
bool CNPC_Malamute::OverrideMove( float flInterval )
{
	// Update what we're looking at
	UpdateHead();

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flInterval - 
//-----------------------------------------------------------------------------
void CNPC_Malamute::UpdateHead( void )
{
	float yaw = GetPoseParameter( m_nPoseFaceHoriz );
	float pitch = GetPoseParameter( m_nPoseFaceVert );

	// If we should be watching our enemy, turn our head
	if ( ( GetTarget() != NULL ) && HasCondition( COND_IN_PVS ) )
	{
		Vector	enemyDir = GetTarget()->WorldSpaceCenter() - WorldSpaceCenter();

		// if target is player look up to their face.
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
		if ( pPlayer != NULL )
		{
			if( GetTarget() == pPlayer )
			{
				enemyDir = GetTarget()->EyePosition() - WorldSpaceCenter();
			}
		}

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
// Purpose: 
// Input  : pEvent - 
//-----------------------------------------------------------------------------
void CNPC_Malamute::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == AE_MALAMUTE_STANDING )
	{
		m_bSitting = false;
		return;
	}
	if ( pEvent->event == AE_MALAMUTE_SITTING )
	{
		m_bSitting = true;
		return;
	}
	if ( pEvent->event == AE_MALAMUTE_BARK )
	{
		EmitSound( "NPC_Malamute.Bark" );
		return;
	}
	if ( pEvent->event == AE_MALAMUTE_PANT )
	{
		EmitSound( "NPC_Malamute.Pant" );
		return;
	}
	// pick up / drop item
	if ( pEvent->event == AE_MALAMUTE_PICKUPITEM )
	{
		// pick up obect, add animation? AE will chage po-state
		m_iPO_State = POS_MALAMUTE_OCCUPIED;

		//
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
		if ( pPlayer != NULL )
		{
			if(GetTarget() != NULL && GetTarget() != pPlayer)
			{
				CBaseEntity *pObject = GetTarget();

				int attachment;

				//!!!PERF - These string lookups here aren't the swiftest, but
				// this doesn't get called very frequently unless a lot of NPCs
				// are using this code.
				attachment = this->LookupAttachment( "itempos" );
				// first move item into the mouth
				Vector vecItemMouthPos;
				if( GetAttachment( "itempos", vecItemMouthPos ) )
					pObject->SetAbsOrigin(vecItemMouthPos);
				// set the malamute attacment as a parent
				pObject->SetParent(this, attachment);

				// extra - set angles: to work with non ball shaped objects better?
				Vector vGunPos;
				QAngle angGunAngles;
				GetAttachment( attachment, vGunPos, angGunAngles );
				pObject->SetAbsAngles( angGunAngles );
			}
		}	
		return;
	}
	if( pEvent->event == AE_MALAMUTE_DROPITEM )
	{
		// drop obect
		m_iPO_State = POS_STATIC;

		CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
		if ( pPlayer != NULL )
		{
			if(GetTarget() != NULL && GetTarget() != pPlayer)
			{
				CBaseEntity *pObject = GetTarget();
				
				//!!!PERF - These string lookups here aren't the swiftest, but
				// this doesn't get called very frequently unless a lot of NPCs
				// are using this code.
				
				int attachment;
				attachment = this->LookupAttachment( "itempos" );
				Vector vecItemMouthPos;

				if( GetAttachment( "itempos", vecItemMouthPos ) )
				{
					pObject->SetParent( NULL );
					pObject->SetOwnerEntity( NULL );

					IPhysicsObject *pPhysObj = pObject->VPhysicsGetObject();
					if( pPhysObj )
					{
						Vector vGunPos;
						QAngle angGunAngles;
						GetAttachment( attachment, vGunPos, angGunAngles );
						// Make sure the pObject's awake
						pPhysObj->Wake();
						pPhysObj->RemoveShadowController();
						pPhysObj->SetPosition( vGunPos, angGunAngles, true );
					}
				}
			}
		}
		return;
	}

	// foot sounds and snow prints
	if ( pEvent->event == AE_MALAMUTE_FOOTPRINT_FRIGHT )
	{
		EmitSound( "NPC_Citizen.FootstepRight" );
		SnowFootPrint( false, true );
		return;
	}
	if ( pEvent->event == AE_MALAMUTE_FOOTPRINT_FLEFT )
	{
		EmitSound( "NPC_Citizen.FootstepLeft" );
		SnowFootPrint( true, true );
		return;
	}
	if ( pEvent->event == AE_MALAMUTE_FOOTPRINT_RIGHT )
	{
		EmitSound( "NPC_Citizen.FootstepRight" );
		SnowFootPrint( false, false );
		return;
	}
	if ( pEvent->event == AE_MALAMUTE_FOOTPRINT_LEFT )
	{
		EmitSound( "NPC_Citizen.FootstepLeft" );
		SnowFootPrint( true, false );
		return;
	}
}

//-------------------------------------
int CNPC_Malamute::SelectSchedule()
{
	if( HasCondition(COND_MALAMUTE_CALLED_HOME) )
	{
		m_bSitting = false;
		EmitSound( "NPC_Malamute.CallHome" );
		m_iPO_State = POS_STATIC;
		return SCHED_MALAMUTE_RETURN_TO_ORIGIN;
	}
	if( HasCondition(COND_MALAMUTE_WATCHOBJECT) )
	{
		m_iPO_State = POS_MALAMUTE_JUDGING;
		return SCHED_MALAMUTE_WATCHOBJECT;
	}
	if( HasCondition(COND_MALAMUTE_FETCH) )
	{
		m_bSitting = false;
		m_iPO_State = POS_MALAMUTE_FETCHING;
		return SCHED_MALAMUTE_FETCH;
	}
	if( HasCondition(COND_MALAMUTE_CHASE) )
	{
		m_bSitting = false;
		return SCHED_MALAMUTE_RUN_TO_GREET_PLAYER;
	}
	if( HasCondition(COND_MALAMUTE_SIT) && !m_bSitting )
	{
		m_iNextWander = gpGlobals->curtime + RandomInt( 3, 5);
		return SCHED_MALAMUTE_SIT;
	}
	if( HasCondition(COND_MALAMUTE_STAND) && m_bSitting )
		return SCHED_MALAMUTE_STAND;

	if ( HasCondition(COND_SEE_PLAYER) )
	{
		if( !m_bExcited )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
			if ( pPlayer != NULL )
			{
				if( GetTarget() != pPlayer )
					SetTarget( pPlayer );
			}
		}

		int testbark = RandomInt(0,3);
		if(testbark == 1)
		{
			return SCHED_MALAMUTE_BARK;
		}
		else
		{
			if(HasCondition(COND_MALAMUTE_FARPLAYER) && !m_bFirstWander)
			{
				m_bSitting = false;
				return SCHED_MALAMUTE_RUN_TO_GREET_PLAYER;
			}
			else if( HasCondition(COND_MALAMUTE_CLOSEPLAYER) )
			{
				if( HasCondition(COND_SEE_PLAYER) )
				{
					int testbark = RandomInt(0,3);
					if(testbark <= 1 )
					{
						return SCHED_MALAMUTE_BARK;
					}
					else if(testbark >= 2 )
						return SCHED_IDLE_STAND;
				}
				else
					return SCHED_MALAMUTE_BARK;
			}
		}
	}
	else
	{
		if( !m_bExcited )
		{
			if( GetTarget() != NULL )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
				if ( pPlayer != NULL )
				{
					if( GetTarget() == pPlayer )
						SetTarget( NULL );
				}
			}
		}
		if(HasCondition(COND_LOST_PLAYER))
		{
			//maybe set a timer to be called home;
			return SCHED_MALAMUTE_WANDER;
		}
	}


	if ( HasCondition(COND_IN_PVS) )
	{
		if(m_bFirstWander)
		{
			m_bFirstWander = false;
			m_iNextWander = gpGlobals->curtime + RandomInt(3, 6);
		}
	}

	if( m_iNextWander < gpGlobals->curtime && !m_bFirstWander )
	{
		if( m_bSitting )
			return SCHED_MALAMUTE_STAND;
		else
			return SCHED_MALAMUTE_WANDER;
	}

	if( HasCondition( COND_HEAR_DANGER ))
	{
		SpeakIfAllowed( TLK_DANGER );
		return SCHED_TAKE_COVER_FROM_BEST_SOUND;
	}


	return BaseClass::SelectSchedule();
}

//
//
void CNPC_Malamute::GatherConditions( void )
{
	BaseClass::GatherConditions();

	static int conditionsToClear[] = 
	{
			COND_MALAMUTE_CALLED_HOME,
			COND_MALAMUTE_SIT,
			COND_MALAMUTE_STAND,
			COND_MALAMUTE_CLOSEPLAYER,
			COND_MALAMUTE_FARPLAYER,
			COND_MALAMUTE_CHASE,
			COND_MALAMUTE_FETCH,
			COND_MALAMUTE_WATCHOBJECT,
	};

	ClearConditions( conditionsToClear, ARRAYSIZE( conditionsToClear ) );


	if ( ( m_spawnflags & SF_MALAMUTE_NCHWL ) && AI_IsSinglePlayer() )
	{
	}
	else
	{
		float m_flRunAwayDist;
		m_flRunAwayDist = (GetLocalOrigin() - m_vSpawnOrigin).Length();
		if(m_flRunAwayDist >= m_iMaxWander)
		{
			SetCondition(COND_MALAMUTE_CALLED_HOME);
		}
	}

	// How far are we from player?
	CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
	if ( pPlayer != NULL )
	{
		// How far are we from player?
		float m_flPlayerDistance;
		m_flPlayerDistance = (GetLocalOrigin() - pPlayer->GetAbsOrigin()).Length();
		if(m_flPlayerDistance >= 180)
			SetCondition(COND_MALAMUTE_FARPLAYER);
		else
			SetCondition(COND_MALAMUTE_CLOSEPLAYER);

		// Is the player carrying something?
		CBaseEntity *pObject = GetPlayerHeldEntity(pPlayer);

		if( !pObject )
		{
			pObject = PhysCannonGetHeldEntity( pPlayer->GetActiveWeapon() );
		}

		if( pObject )
		{
			if( HasCondition( COND_MALAMUTE_CLOSEPLAYER ) && (pObject->VPhysicsGetObject()->GetMass() == MALAMUTE_FETCH_WEIGHT)) // MALAMUTE_MIN_FETCH_WEIGHT
			{
				if( m_iPO_State == POS_STATIC && GetTarget() != pObject)
				{
					m_iPO_State = POS_PLAYER_OCCUPIED;

					SetTarget(pObject); // set as target so we dont loose it

						if( !m_bExcited )
							m_bExcited = true;
						if(m_bSitting)
							SetCondition( COND_MALAMUTE_STAND );
				}
			}
		}
		else
		{
			if( m_iPO_State == POS_PLAYER_OCCUPIED )
			{
				//player tossed object
				m_iPO_State = POS_TOSSED;
				m_iPO_NextStatic = gpGlobals->curtime + 2;
			}
			if( (m_iPO_State == POS_STATIC) && !m_bExcited )
			{
				if( HasCondition ( COND_MALAMUTE_CLOSEPLAYER ) )
				{
					if( HasCondition ( COND_SEE_PLAYER ) )
					{
						if( GetTarget() != pPlayer )
							SetTarget( pPlayer );
					}
					else
					{
						if( GetTarget() != NULL )
							SetTarget( NULL );
					}
				}
				else
				{
					if( GetTarget() != NULL )
							SetTarget( NULL );
				}
			}
		}

		if( m_iPO_State == POS_TOSSED )
		{
			if( m_iPO_NextStatic < gpGlobals->curtime )
			{
				m_iPO_State = POS_STATIC;
				SetTarget(pPlayer);
				TaskComplete();
			}
			else
				SetCondition(COND_MALAMUTE_WATCHOBJECT);
		}
		if( m_iPO_State == POS_FETCHABLE )
			SetCondition(COND_MALAMUTE_FETCH);
	}

	// If we are not sitting, run a couple of test to see if we should try to.
	if( !m_bSitting )
	{
		if( IsCurSchedule(SCHED_IDLE_STAND) )
		{
			if( HasCondition(COND_SEE_PLAYER) )
			{
				if( HasCondition(COND_MALAMUTE_CLOSEPLAYER)) // we do this a lot, maybe we can test against a timer to sit after a while?
				{
					if( m_bSwitchSit )
					{
						m_bSwitchSit = false;
						m_iNextSit = gpGlobals->curtime + RandomInt (3 , 8);
					}
					if((m_iPO_State == POS_STATIC) && !m_bSwitchSit && (m_iNextSit < gpGlobals->curtime))
						SetCondition(COND_MALAMUTE_SIT);
				}
				else
				{
					if( !m_bSwitchSit )
					{
						m_bSwitchSit = true;
					}
				}
			}
			else
			{
				// test a chase next Jason!
				if( HasCondition(COND_MALAMUTE_FARPLAYER) && !m_bFirstWander)
				{
					if(!HasCondition(COND_LOST_PLAYER))
					{
						if(m_iPO_State == POS_STATIC)
							SetCondition(COND_MALAMUTE_CHASE);
					}
				}
			}
			if( HasCondition(COND_LOST_PLAYER) )
			{
				if( HasCondition(COND_MALAMUTE_FARPLAYER) )
					SetCondition(COND_MALAMUTE_CALLED_HOME);
			}	
		}
	}
	else
	{
		if( HasCondition(COND_LOST_PLAYER) )
		{
			if( m_bExcited && (m_iPO_State == POS_STATIC) )
				SetCondition(COND_MALAMUTE_CALLED_HOME);
		}
	}
}

//-------------------------------------
//-------------------------------------
void CNPC_Malamute::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_MALAMUTE_BARK:
		SetIdealActivity( ACT_MALAMUTE_BARK );
		break;
	case TASK_MALAMUTE_SIT:
		SetIdealActivity( ACT_MALAMUTE_SIT );
		break;
	case TASK_MALAMUTE_STAND:
		SetIdealActivity( ACT_MALAMUTE_STAND );
		break;
	case TASK_MALAMUTE_PICKUPITEM:
		ResetActivity();
		SetActivity( ACT_MALAMUTE_PICKUPITEM );
		break;
	case TASK_MALAMUTE_DROPITEM:
		ResetActivity();
		SetActivity( ACT_MALAMUTE_DROPITEM );
		break;
	case TASK_MALAMUTE_SET_ITEM_GOAL:
		{
			// scan down from the item to find the floor
			trace_t tr;
			UTIL_TraceHull( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -2048 ), NAI_Hull::Mins( GetHullType() ), NAI_Hull::Maxs( GetHullType() ), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
			// check for ground below
			if( tr.fraction < 1.0 && tr.m_pEnt )
			{
				SetAbsOrigin( tr.endpos );
				SetAbsVelocity( vec3_origin );
				// set the goal location
				m_vecStoredPathGoal = tr.endpos;
				m_nStoredPathType	= GOALTYPE_LOCATION;
				m_fStoredPathFlags	= 0;
				m_hStoredPathTarget	= NULL; // GetEnemy(), NULL
				GetNavigator()->SetMovementActivity(ACT_RUN);
				TaskComplete();
			}
			else
				TaskComplete();
			break;
		}
	case TASK_MALAMUTE_SET_GOAL:
		{
			if(IsCurSchedule( SCHED_MALAMUTE_GRAB_RETURN ))
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
				if ( pPlayer != NULL )
				{
					m_vecStoredPathGoal = pPlayer->GetAbsOrigin();
				}
				else
				{
					m_vecStoredPathGoal = m_vSpawnOrigin;
				}
				//Setup our stored info
				m_vecStoredPathGoal = m_vSpawnOrigin;
				m_nStoredPathType	= GOALTYPE_LOCATION;
				m_fStoredPathFlags	= 0;
				m_hStoredPathTarget	= NULL; // GetEnemy(), NULL
				GetNavigator()->SetMovementActivity(ACT_RUN);
				TaskComplete();
			}
			else // spawn origin
			{
				//Setup our stored info
				m_vecStoredPathGoal = m_vSpawnOrigin;
				m_nStoredPathType	= GOALTYPE_LOCATION;
				m_fStoredPathFlags	= 0;
				m_hStoredPathTarget	= NULL; // GetEnemy(), NULL
				GetNavigator()->SetMovementActivity(ACT_RUN);
				TaskComplete();
			}
			break;
		}
	default:
		BaseClass::StartTask( pTask );
		break;
	}
}
//-------------------------------------
//-------------------------------------
void CNPC_Malamute::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_MALAMUTE_BARK:
		{
			if ( IsActivityFinished() )
			{
					TaskComplete();
					SetIdealActivity( ACT_IDLE );
			}
			break;
		}
	case TASK_MALAMUTE_SIT:
		{
			if ( IsActivityFinished() )
			{
					TaskComplete();
					SetIdealActivity( ACT_IDLE );
			}
			break;
		}
	case TASK_MALAMUTE_STAND:
		{
			if ( IsActivityFinished() )
			{
					TaskComplete();
					SetIdealActivity( ACT_IDLE );
			}
			break;
		}
	case TASK_MALAMUTE_PICKUPITEM:
		{
			if ( IsActivityFinished() )
				TaskComplete();
			break;
		}
	case TASK_MALAMUTE_DROPITEM:
		{
			if ( IsActivityFinished() )
				TaskComplete();
			break;
		}
	case TASK_WATCH_OBJECT:
		{
			if(m_iPO_State == POS_MALAMUTE_JUDGING)
			{
				if(m_iPO_NextStatic < gpGlobals->curtime )
				{
					m_iPO_State = POS_STATIC;
					
					CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
					if ( pPlayer != NULL )
					{
						SetTarget(pPlayer);
					}
					
					SetCondition( COND_MALAMUTE_CHASE ); //
					TaskComplete();
				}
				// if we has a target now that is not the player, 
				// and its a good distance away, 
				// set it up to be fetched
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
				if ( pPlayer != NULL )
				{
					if(GetTarget() != NULL && GetTarget() != pPlayer) // jason
					{
						CBaseEntity *pObjectnew = GetTarget(); // jason

						if( pObjectnew )
						{
							// How far is it from player?
							float m_flObjectDistance;
							m_flObjectDistance = (pObjectnew->GetAbsOrigin() - pPlayer->GetAbsOrigin()).Length();

							float f_curMass = pObjectnew->VPhysicsGetObject()->GetMass();
							if((m_flObjectDistance > BALL_MIN_TOSS_DISTANCE) && (f_curMass == MALAMUTE_FETCH_WEIGHT))
							{
								m_iPO_State = POS_FETCHABLE;
								SetCondition( COND_MALAMUTE_FETCH ); //
								TaskComplete();
							}
						}
					}
					else
					{
						m_iPO_State = POS_STATIC;
						TaskComplete();
					}
				}
			}
			else
			{
				m_iPO_State = POS_STATIC;
				TaskComplete();
			}
			break;
		}
	case TASK_WAIT_FOR_MOVEMENT:
		{
			if( IsCurSchedule( SCHED_MALAMUTE_FETCH ) )
			{
				TaskComplete();
				break;
			}
			if( IsCurSchedule( SCHED_MALAMUTE_GRAB_RETURN ) )
			{
				TaskComplete();
				break;
			}
			if( IsCurSchedule( SCHED_MALAMUTE_DROPOBJECT ) )
			{
				TaskComplete();
				break;
			}
			if( IsCurSchedule( SCHED_MALAMUTE_RETURN_TO_ORIGIN ) )
			{
				m_bExcited = false;
				SetIdealActivity( (Activity)ACT_MALAMUTE_SIT );
				m_iNextWander = gpGlobals->curtime + RandomInt(5,8);
				TaskComplete();
				break;
			}
			if(  IsCurSchedule( SCHED_MALAMUTE_RUN_TO_GREET_PLAYER ) )
			{
				TaskComplete();
				break;
			}
			if(IsCurSchedule(SCHED_MALAMUTE_BARK))
			{
				TaskComplete();
				break;
			}
			if ( IsWaitFinished() )
			{
				TaskComplete();
			}
			break;
		}
	default:
		BaseClass::RunTask( pTask );
		break;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Malamute::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

float CNPC_Malamute::MaxYawSpeed ( void )
{
	if(!m_bSitting)
	{
		if(m_bExcited)
		{
			switch ( GetActivity() )
			{
			case ACT_IDLE:			
				return 0;

			case ACT_RUN:
				return 18;
			case ACT_WALK:			
				return 25;

			case ACT_TURN_LEFT:
			case ACT_TURN_RIGHT:
				return 25;

			default:
				return 0;
			}
		}
		else
		{
			switch ( GetActivity() )
			{
			case ACT_IDLE:			
				return 0;

			case ACT_RUN:
				return 15;
			case ACT_WALK:			
				return 20;

			case ACT_TURN_LEFT:
			case ACT_TURN_RIGHT:
				return 20;

			default:
				return 0;
			}
		}
	}
	else
	{
		return 0;
	}

	return BaseClass::MaxYawSpeed();
}

//-----------------------------------------------------------------------------
// Purpose: Footprints left and right
// Output :
//-----------------------------------------------------------------------------
void CNPC_Malamute::SnowFootPrint( bool IsLeft, bool IsFront )
{
	trace_t tr;
	Vector traceStart;
	QAngle angles;

	int attachment;

	//!!!PERF - These string lookups here aren't the swiftest, but
	// this doesn't get called very frequently unless a lot of NPCs
	// are using this code.

	if(	IsFront )
	{
		if( IsLeft )
			attachment = this->LookupAttachment( "frontleftfoot" );
		else
			attachment = this->LookupAttachment( "frontrightfoot" );
	}
	else
	{
		if( IsLeft )
			attachment = this->LookupAttachment( "leftfoot" );
		else
			attachment = this->LookupAttachment( "rightfoot" );
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
				case 'J': // ICEMOD snow, defined in decals.h
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
						IsLeft ? -5 : 5, // 12 = half width
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
//
// CNPC_Malamute Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_malamute, CNPC_Malamute )

	DECLARE_ACTIVITY( ACT_MALAMUTE_HAPPYIDLE )
	DECLARE_ACTIVITY( ACT_MALAMUTE_HAPPYPAUSE )
	DECLARE_ACTIVITY( ACT_MALAMUTE_SIT )
	DECLARE_ACTIVITY( ACT_MALAMUTE_STAND )
	DECLARE_ACTIVITY( ACT_MALAMUTE_SIT_IDLE )
	DECLARE_ACTIVITY( ACT_MALAMUTE_SIT_IDLE_B )
	DECLARE_ACTIVITY( ACT_MALAMUTE_BARK )
	DECLARE_ACTIVITY( ACT_MALAMUTE_BARKEXCITED )
	DECLARE_ACTIVITY( ACT_MALAMUTE_SIT_BARK )
	DECLARE_ACTIVITY( ACT_MALAMUTE_SIT_PANT )
	DECLARE_ACTIVITY( ACT_MALAMUTE_PICKUPITEM )
	DECLARE_ACTIVITY( ACT_MALAMUTE_DROPITEM )

	DECLARE_TASK( TASK_MALAMUTE_SIT )
	DECLARE_TASK( TASK_MALAMUTE_STAND )
	DECLARE_TASK( TASK_MALAMUTE_SET_GOAL )
	DECLARE_TASK( TASK_MALAMUTE_BARK )
	DECLARE_TASK( TASK_WATCH_OBJECT )
	DECLARE_TASK( TASK_MALAMUTE_SET_ITEM_GOAL )
	DECLARE_TASK( TASK_MALAMUTE_PICKUPITEM )
	DECLARE_TASK( TASK_MALAMUTE_DROPITEM )

	DECLARE_ANIMEVENT( AE_MALAMUTE_STANDING )
	DECLARE_ANIMEVENT( AE_MALAMUTE_SITTING )
	DECLARE_ANIMEVENT( AE_MALAMUTE_BARK )
	DECLARE_ANIMEVENT( AE_MALAMUTE_FOOTPRINT_FRIGHT )
	DECLARE_ANIMEVENT( AE_MALAMUTE_FOOTPRINT_FLEFT )
	DECLARE_ANIMEVENT( AE_MALAMUTE_FOOTPRINT_RIGHT )
	DECLARE_ANIMEVENT( AE_MALAMUTE_FOOTPRINT_LEFT )
	DECLARE_ANIMEVENT( AE_MALAMUTE_PANT )
	DECLARE_ANIMEVENT( AE_MALAMUTE_PICKUPITEM )
	DECLARE_ANIMEVENT( AE_MALAMUTE_DROPITEM )

	DECLARE_CONDITION( COND_MALAMUTE_SIT )
	DECLARE_CONDITION( COND_MALAMUTE_STAND )
	DECLARE_CONDITION( COND_MALAMUTE_CALLED_HOME )
	DECLARE_CONDITION( COND_MALAMUTE_CHASE )
	DECLARE_CONDITION( COND_MALAMUTE_CLOSEPLAYER )
	DECLARE_CONDITION( COND_MALAMUTE_FARPLAYER )
	DECLARE_CONDITION( COND_MALAMUTE_WATCHOBJECT )
	DECLARE_CONDITION( COND_MALAMUTE_FETCH )

	DEFINE_SCHEDULE
	(
		SCHED_MALAMUTE_WANDER,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_WANDER						480240"		// 48 units to 240 units.
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_STOP_MOVING				0"
		"		TASK_WAIT						3"
		"		TASK_WAIT_RANDOM				3"
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_MALAMUTE_WANDER" // keep doing it
		"	"
		"	Interrupts"
		"		COND_SEE_PLAYER"
		"		COND_HEAR_PLAYER"
		"		COND_MALAMUTE_CALLED_HOME"
		"		COND_MALAMUTE_SIT"
		"		COND_MALAMUTE_WATCHOBJECT"
		"		COND_MALAMUTE_FETCH"
	)

	DEFINE_SCHEDULE
	(
		SCHED_MALAMUTE_RETURN_TO_ORIGIN,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_MALAMUTE_SET_GOAL			0"
		"		TASK_GET_PATH_TO_GOAL			0"
		"		TASK_RUN_PATH_WITHIN_DIST		32"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_MALAMUTE_SIT"
		""
		"	Interrupts"
	)
	DEFINE_SCHEDULE
	(
		SCHED_MALAMUTE_RUN_TO_GREET_PLAYER,

		"	Tasks"
		"		TASK_TARGET_PLAYER			0"
		"		TASK_GET_PATH_TO_PLAYER		0"
		"		TASK_RUN_PATH_WITHIN_DIST	180"
		"		TASK_FACE_PLAYER			0"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		""
		"	Interrupts"
		"		COND_MALAMUTE_CALLED_HOME"
		"		COND_LOST_PLAYER"
		"		COND_MALAMUTE_WATCHOBJECT"
		"		COND_MALAMUTE_FETCH"
	)
	
	DEFINE_SCHEDULE
	(
		SCHED_MALAMUTE_WATCHOBJECT,

		"	Tasks"
		"		TASK_WATCH_OBJECT					0"
		""
		"	Interrupts"
		"		COND_MALAMUTE_WATCHOBJECT"
		"		COND_MALAMUTE_FETCH"
	)

	DEFINE_SCHEDULE
	(
		SCHED_MALAMUTE_FETCH,

		"	Tasks"
		"		TASK_FACE_TARGET						0"
		"		TASK_GET_PATH_TO_TARGET					0"
		"		TASK_MOVE_TO_TARGET_RANGE				5" //15"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		"		TASK_SET_SCHEDULE						SCHEDULE:SCHED_MALAMUTE_GRAB_RETURN"
		""
		"	Interrupts"
	)
	DEFINE_SCHEDULE
	(
		SCHED_MALAMUTE_GRAB_RETURN,
		"	Tasks"
		"		TASK_MALAMUTE_PICKUPITEM				0"
		"		TASK_GET_PATH_TO_PLAYER					0"
		"		TASK_RUN_PATH_WITHIN_DIST				180"
		"		TASK_FACE_PLAYER						0"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		"		TASK_SET_SCHEDULE						SCHEDULE:SCHED_MALAMUTE_DROPOBJECT"
		""
		"	Interrupts"
	)
	DEFINE_SCHEDULE
	(
		SCHED_MALAMUTE_DROPOBJECT,

		"	Tasks"
		"		TASK_MALAMUTE_DROPITEM					0"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		"		TASK_TARGET_PLAYER						0"
		"		TASK_SET_ACTIVITY						ACTIVITY:ACT_MALAMUTE_BARK"
		""
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_MALAMUTE_SIT,

		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_MALAMUTE_SIT					0"
		""
		"	Interrupts"
		"		COND_MALAMUTE_FETCH"
	)
	DEFINE_SCHEDULE
	(
		SCHED_MALAMUTE_STAND,

		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_MALAMUTE_STAND					0"
		""
		"	Interrupts"
		"		COND_MALAMUTE_FETCH"
	)

	DEFINE_SCHEDULE
	(
		SCHED_MALAMUTE_BARK,

		"	Tasks"
		"		TASK_FACE_PLAYER					0"
		"		TASK_STOP_MOVING					0"
		"		TASK_MALAMUTE_BARK					0"
		""
		"	Interrupts"
		"		COND_MALAMUTE_FETCH"
	)
	
	DEFINE_SCHEDULE
	(
		SCHED_MALAMUTE_STAND_BARK,

		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_FACE_PLAYER					0"
		"		TASK_MALAMUTE_BARK					0"
		""
		"	Interrupts"
		"		COND_MALAMUTE_FETCH"
	)

AI_END_CUSTOM_NPC()
