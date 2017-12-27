//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Voloxelicans. Simple ambient birds that fly away when they hear gunfire or
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
#include "ai_hint.h"
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
#include "npc_voloxelican.h" //
#include "ai_moveprobe.h"

// had to add for attack obstructions
#include "props.h"
// had to add for phys gun holding stuff
#include "weapon_physcannon.h"
// for claw attack and vehicles
#include "vehicle_base.h"
// for spit attack
#include "voloxelican_spit.h"
// for giving eachother room
#include "ai_squad.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
// Custom activities.
//
static int ACT_VOLOXELICAN_TAKEOFF;
static int ACT_VOLOXELICAN_SOAR;
static int ACT_VOLOXELICAN_LAND;

//
// Animation events.
//
static int AE_VOLOXELICAN_TAKEOFF;
static int AE_VOLOXELICAN_LANDED;
static int AE_VOLOXELICAN_FLY;
static int AE_VOLOXELICAN_HOP;
static int AE_VOLOXELICAN_MELEE_KICK;
static int AE_VOLOXELICAN_SPIT;

static int AE_VOLOXELICAN_FOOTPRINT_RIGHT;
static int AE_VOLOXELICAN_FOOTPRINT_LEFT;

//
// Skill settings.
//
ConVar sk_voloxelican_health( "sk_voloxelican_health","0");
ConVar sk_voloxelican_melee_dmg( "sk_voloxelican_melee_dmg","0");

LINK_ENTITY_TO_CLASS( npc_voloxelican, CNPC_Voloxelican );

BEGIN_DATADESC( CNPC_Voloxelican )

	DEFINE_FIELD( m_flGroundIdleMoveTime, FIELD_TIME ),
	DEFINE_FIELD( m_flEnemyDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_bReachedMoveGoal, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flHopStartZ, FIELD_FLOAT ),
	DEFINE_FIELD( m_vDesiredTarget, FIELD_VECTOR ),
	DEFINE_FIELD( m_vCurrentTarget, FIELD_VECTOR ),
	DEFINE_FIELD( m_bPlayedLoopingSound, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vLastStoredOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flLastStuckCheck, FIELD_TIME ),
	DEFINE_KEYFIELD( m_bStartAsFlying, FIELD_BOOLEAN, "flying" ),
	DEFINE_KEYFIELD( m_bMyAIType, FIELD_BOOLEAN, "myaitype" ),
	DEFINE_FIELD( m_nUniqueSpitDistanceClose, FIELD_INTEGER ),
	DEFINE_FIELD( m_nUniqueSpitDistanceFar, FIELD_INTEGER ),
	DEFINE_FIELD( m_nCurrentInAirState, FIELD_INTEGER ),
	DEFINE_FIELD( m_flGetTiredTime, FIELD_TIME ),
	DEFINE_FIELD( m_flNextCanFlyTime, FIELD_TIME ),
	DEFINE_FIELD( m_bLanding, FIELD_BOOLEAN ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_STRING, "FlyAway", InputFlyAway ),

END_DATADESC()

static ConVar birds_debug( "birds_debug", "0" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::Spawn( void )
{
	BaseClass::Spawn();

	char *szModel = (char *)STRING( GetModelName() );
	if (!szModel || !*szModel)
	{
		szModel = "models/voloxelican/voloxelican.mdl";
		SetModelName( AllocPooledString(szModel) );
	}

	Precache();
	SetModel( szModel );

	m_iHealth = sk_voloxelican_health.GetFloat();

	SetHullType(HULL_MEDIUM);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_STEP );

	m_flFieldOfView = VIEW_FIELD_FULL;
	SetViewOffset( Vector(6, 0, 11) );		// Position of the eyes relative to NPC's origin.

	m_flGroundIdleMoveTime = gpGlobals->curtime + random->RandomFloat( 0.0f, 5.0f );

	SetBloodColor( BLOOD_COLOR_YELLOW );
	m_NPCState = NPC_STATE_NONE;

	SetCollisionGroup( HL2COLLISION_GROUP_ANTLION );

	CapabilitiesClear();

	// Start as a flyer?
	SetFlyingState( m_bStartAsFlying ? FlyState_Flying : FlyState_Walking );
	m_nCurrentInAirState = InAirState_Idle;
	SetUniqueSpitDistanceClose(RandomInt( 115, 320));
	SetUniqueSpitDistanceFar(UniqueSpitDistanceClose() + 130);

	m_bLanding = false;

	// We don't mind zombies/penguins so much.
	AddClassRelationship( CLASS_ZOMBIE, D_NU, 0 );

	NPCInit();

	m_vLastStoredOrigin = vec3_origin;
	m_flLastStuckCheck = gpGlobals->curtime;

	SetGoalEnt( NULL );

	m_nPoseFaceVert = LookupPoseParameter( "turnhead_vert" );
	m_nPoseFaceHoriz = LookupPoseParameter( "turnhead" );
}


//-----------------------------------------------------------------------------
// Purpose: Returns this monster's classification in the relationship table.
//-----------------------------------------------------------------------------
Class_T	CNPC_Voloxelican::Classify( void )
{
	return( CLASS_ANTLION ); // was CLASS_EARTH_FAUNA, using CLASS_ANTLION for now 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEnemy - 
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	ClearCondition( COND_VOLOXELICAN_LOCAL_MELEE_OBSTRUCTION );

	m_flEnemyDist = (GetLocalOrigin() - pEnemy->GetLocalOrigin()).Length();

	if(IsFlying())
	{
		if( gpGlobals->curtime >= m_flNextAttack )
		{
			if( (m_flEnemyDist > (UniqueSpitDistanceClose() - 100)) &&
				(m_flEnemyDist < (UniqueSpitDistanceFar() + 100)) )
			{
				// trace from mouth, to see if we'll hit an ally

				Vector vecMouthPos;

				if( GetAttachment( "mouth", vecMouthPos ) )
				{
					trace_t tr;

					AI_TraceLine ( vecMouthPos, (pEnemy->GetAbsOrigin() - Vector(150)), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
					
					if ( tr.fraction != 1.0f)
					{
						if(GetEnemy()->IsPlayer())
						{
							// Less chance of attacking the player, if the player is looking away from you.
							CBasePlayer *pPlayer = ToBasePlayer( GetEnemy() );
							Vector vLookDir = pPlayer->EyeDirection3D();
							Vector vTargetDir = GetAbsOrigin() - pPlayer->EyePosition();
							VectorNormalize( vTargetDir );

							float fDotPr = DotProduct( vLookDir,vTargetDir );
							if ( fDotPr > 0 )
								SetCondition( COND_VOLOXELICAN_ENEMY_GOOD_RANGE_DISTANCE );
							else
							{
								// player isnt looking, chance I wont attack
								if(RandomInt(0 , 5) == 3)
									SetCondition( COND_VOLOXELICAN_ENEMY_GOOD_RANGE_DISTANCE );
							}
						}
						else
						{
							SetCondition( COND_VOLOXELICAN_ENEMY_GOOD_RANGE_DISTANCE );
						}
					}
				}
				m_flNextAttack = gpGlobals->curtime + RandomFloat( 1 , 3 );
			}
		}
	}
	else
	{

		if ( m_flEnemyDist < 512 )
			SetCondition( COND_VOLOXELICAN_ENEMY_WAY_TOO_CLOSE );

		if ( m_flEnemyDist < 1024 )
			SetCondition( COND_VOLOXELICAN_ENEMY_TOO_CLOSE );
	}

	BaseClass::GatherEnemyConditions(pEnemy);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : posSrc - 
// Output : Vector
//-----------------------------------------------------------------------------
Vector CNPC_Voloxelican::BodyTarget( const Vector &posSrc, bool bNoisy ) 
{ 
	Vector vecResult;
	vecResult = GetAbsOrigin();
	vecResult.z += 6;
	return vecResult;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::StopLoopingSounds( void )
{
	//
	// Stop whatever flap sound might be playing.
	//
	if ( m_bPlayedLoopingSound )
		StopSound( "NPC_Voloxelican.Flap" );

	BaseClass::StopLoopingSounds();
}


//-----------------------------------------------------------------------------
// Purpose: Catches the monster-specific messages that occur when tagged
//			animation frames are played.
// Input  : pEvent - 
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::HandleAnimEvent( animevent_t *pEvent )
{
	// Footprints in da snow
	if ( pEvent->event == AE_VOLOXELICAN_FOOTPRINT_LEFT )
	{
		SnowFootPrint( true );
		return;
	}
	if ( pEvent->event == AE_VOLOXELICAN_FOOTPRINT_RIGHT )
	{
		SnowFootPrint( false );
		return;
	}
	// attacks
	if ( pEvent->event == AE_VOLOXELICAN_MELEE_KICK )
	{
		Vector right, forward;
		AngleVectors( GetLocalAngles(), &forward, &right, NULL );
		right = right * 100;
		forward = forward * 200;

		ClawAttack( 55, sk_voloxelican_melee_dmg.GetFloat(), QAngle( -15, -20, -10 ), right + forward );
		return;
	}
	if ( pEvent->event == AE_VOLOXELICAN_SPIT )
	{
		// before shooting the player should be looking at this enemy... or the enemy can fire at you less while
		// you're paying less attention to it?
		EmitSound( "NPC_FastHeadcrab.Attack" );
		SpitAttack(5); // spit at this velocity 5 bullseye. 
		return;
	}
	if ( pEvent->event == AE_VOLOXELICAN_TAKEOFF )
	{
		if ( GetEnemy() != NULL)
		{
			Vector vEnemyPos = GetEnemy()->GetAbsOrigin();
			Takeoff( vEnemyPos );
		}
		else if ( GetNavigator()->GetPath()->GetCurWaypoint() )
			Takeoff( GetNavigator()->GetCurWaypointPos() );
		return;
	}
	if ( pEvent->event == AE_VOLOXELICAN_LANDED )
	{
		SetFlyingState( FlyState_Walking );
		return;
	}

	if( pEvent->event == AE_VOLOXELICAN_HOP )
	{
		SetGroundEntity( NULL );

		//
		// Take him off ground so engine doesn't instantly reset FL_ONGROUND.
		//
		UTIL_SetOrigin( this, GetLocalOrigin() + Vector( 0 , 0 , 1 ));

		//
		// How fast does the voloxelican need to travel to reach the hop goal given gravity?
		//
		float flHopDistance = ( m_vSavePosition - GetLocalOrigin() ).Length();
		float gravity = sv_gravity.GetFloat();
		if ( gravity <= 1 )
		{
			gravity = 1;
		}

		float height = 0.25 * flHopDistance;
		float speed = sqrt( 2 * gravity * height );
		float time = speed / gravity;

		//
		// Scale the sideways velocity to get there at the right time
		//
		Vector vecJumpDir = m_vSavePosition - GetLocalOrigin();
		vecJumpDir = vecJumpDir / time;

		//
		// Speed to offset gravity at the desired height.
		//
		vecJumpDir.z = speed;

		//
		// Don't jump too far/fast.
		//
		float distance = vecJumpDir.Length();
		if ( distance > 650 )
		{
			vecJumpDir = vecJumpDir * ( 650.0 / distance );
		}

		// Play a hop flap sound.
		EmitSound( "NPC_Voloxelican.Hop" );

		SetAbsVelocity( vecJumpDir );
		return;
	}

	if( pEvent->event == AE_VOLOXELICAN_FLY )
	{
		//
		// Start flying.
		//
		SetActivity( ACT_FLY );
		return;
	}

	CAI_BaseNPC::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eNewActivity - 
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::OnChangeActivity( Activity eNewActivity )
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
// Purpose: Input handler that makes the voloxelican fly away.
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::InputFlyAway( inputdata_t &inputdata )
{
	string_t sTarget = MAKE_STRING( inputdata.value.String() );

	if ( sTarget != NULL_STRING )// this npc has a target
	{
		CBaseEntity *pEnt = gEntList.FindEntityByName( NULL, sTarget );

		if ( pEnt )
		{
			trace_t tr;
			AI_TraceLine ( EyePosition(), pEnt->GetAbsOrigin(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );

			if ( tr.fraction != 1.0f )
				 return;

			// Find the npc's initial target entity, stash it
			SetGoalEnt( pEnt );
		}
	}
	else
		SetGoalEnt( NULL );

	SetCondition( COND_VOLOXELICAN_FORCED_FLY );
	SetCondition( COND_PROVOKED );

}

void CNPC_Voloxelican::UpdateEfficiency( bool bInPVS )	
{
	if ( IsFlying() )
	{
		SetEfficiency( ( GetSleepState() != AISS_AWAKE ) ? AIE_DORMANT : AIE_NORMAL ); 
		SetMoveEfficiency( AIME_NORMAL ); 
		return;
	}

	BaseClass::UpdateEfficiency( bInPVS );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
inline CBaseEntity *CNPC_Voloxelican::EntityToWatch( void )
{
	return ( GetTarget() != NULL ) ? GetTarget() : GetEnemy();	// Okay if NULL
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flInterval - 
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::UpdateHead( void )
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
bool CNPC_Voloxelican::OverrideMove( float flInterval )
{
	if ( GetNavigator()->GetPath()->CurWaypointNavType() == NAV_FLY && GetNavigator()->GetNavType() != NAV_FLY )
	{
		SetNavType( NAV_FLY );
	}

	// Update what we're looking at
	UpdateHead();

	// Flying
	if ( IsFlying() && !m_bLanding)
	{
		// see if were tired and want to land first
		if( !m_bMyAIType )
		{
			if ( gpGlobals->curtime > m_flGetTiredTime )
			{
				if(m_nCurrentInAirState != InAirState_MoveToLanding )
				{
					//m_flGetTiredTime = gpGlobals->curtime + RandomInt(5, 10); // Jason remove
					m_nCurrentInAirState = InAirState_MoveToLanding; // switch back  
				}
			}
		}
		// Jason - testing out this new code
		Vector vMoveTargetPos(0,0,0);
		CBaseEntity *pMoveTarget = NULL;

		if ( !GetNavigator()->IsGoalActive() || ( GetNavigator()->GetCurWaypointFlags() | bits_WP_TO_PATHCORNER ) )
		{
			// Select move target 
			if ( GetTarget() != NULL )
				pMoveTarget = GetTarget();
			else if ( GetEnemy() != NULL )
				pMoveTarget = GetEnemy();

			// Select move target position 
			if ( GetEnemy() != NULL )
			{
				vMoveTargetPos = GetEnemy()->GetAbsOrigin();

				if( m_nCurrentInAirState != InAirState_MoveToLanding )
					m_nCurrentInAirState = InAirState_RangeAttacking;
			}
			else
			{
				if( m_nCurrentInAirState != InAirState_MoveToLanding )
					m_nCurrentInAirState = InAirState_Idle;
			}
		}
		else
			vMoveTargetPos = GetNavigator()->GetCurWaypointPos();

		// Jason - testing out this new code *END*


		if ( GetNavigator()->GetPath()->GetCurWaypoint() )
		{
			if ( m_flLastStuckCheck <= gpGlobals->curtime )
			{
				if ( m_vLastStoredOrigin == GetAbsOrigin() )
				{
					if ( GetAbsVelocity() == vec3_origin )
					{
						float flDamage = m_iHealth;
						
						CTakeDamageInfo	dmgInfo( this, this, flDamage, DMG_GENERIC );
						GuessDamageForce( &dmgInfo, vec3_origin - Vector( 0, 0, 0.1 ), GetAbsOrigin() );
						TakeDamage( dmgInfo );

						return false;
					}
					else
					{
						m_vLastStoredOrigin = GetAbsOrigin();
					}
				}
				else
				{
					m_vLastStoredOrigin = GetAbsOrigin();
				}
				
				m_flLastStuckCheck = gpGlobals->curtime + 1.0f;
			}

			if (m_bReachedMoveGoal)
			{
				SetIdealActivity( (Activity)ACT_VOLOXELICAN_LAND );
				SetFlyingState( FlyState_Walking );
				TaskMovementComplete();
			}
			else
				SetIdealActivity ( ACT_FLY );
		}
		else if ( !GetTask() || GetTask()->iTask == TASK_WAIT_FOR_MOVEMENT )
		{
			SetFlyingState( FlyState_Flying );
			SetIdealActivity ( ACT_FLY );
		}
		MoveVoloxelicanFly( flInterval );
		return true;
	}
	
	return false;
}

Activity CNPC_Voloxelican::NPC_TranslateActivity( Activity eNewActivity )
{
	if ( IsFlying() && eNewActivity == ACT_IDLE ){
		return ACT_FLY;
	}

	if ( eNewActivity == ACT_FLY ){
		return ACT_FLY;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vOut - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Voloxelican::GetGoalDirection( Vector *vOut )
{
	CBaseEntity *pTarget = GetTarget();

	if ( pTarget == NULL )
		return false;

	if ( FClassnameIs( pTarget, "info_hint_air" ) || FClassnameIs( pTarget, "info_target" ) )
	{
		AngleVectors( pTarget->GetAbsAngles(), vOut );
		return true;
	}

	return false;
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &goalPos - 
//			&startPos - 
//			idealRange - 
//			idealHeight - 
// Output : Vector
//-----------------------------------------------------------------------------
Vector CNPC_Voloxelican::IdealGoalForMovement( const Vector &goalPos, const Vector &startPos, float idealRange, float idealHeightDiff )
{
	Vector	vMoveDir;

	if ( GetGoalDirection( &vMoveDir ) == false )
	{
		vMoveDir = ( goalPos - startPos );
		vMoveDir.z = 0;
		VectorNormalize( vMoveDir );
	}

	// Move up from the position by the desired amount
	Vector vIdealPos;
	if( m_nCurrentInAirState == InAirState_MoveToLanding )
	{
		vIdealPos = goalPos + Vector( vMoveDir[0] * -idealRange, vMoveDir[1] * -idealRange, 0 );
		vIdealPos.z = 0;
	}
	else
	{
		vIdealPos = goalPos + Vector( 0, 0, idealHeightDiff ) + ( vMoveDir * -idealRange );
	}

	// Trace down and make sure we can fit here
	trace_t	tr;
	AI_TraceHull( vIdealPos, vIdealPos - Vector( 0, 0, 64 ), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );

	// Move up otherwise if not tired
	if( m_nCurrentInAirState != InAirState_MoveToLanding )
	{
		if ( tr.fraction < 1.0f )
			vIdealPos.z += ( 64 * ( 1.0f - tr.fraction ) );
	}

	return vIdealPos;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Vector CNPC_Voloxelican::VelocityToEvade(CBaseCombatCharacter *pEnemy)
{
	if (pEnemy)
	{
		// -----------------------------------------
		//  Keep out of enemy's shooting position
		// -----------------------------------------
		Vector vEnemyFacing = pEnemy->BodyDirection2D( );
		Vector	vEnemyDir   = pEnemy->EyePosition() - GetLocalOrigin();
		VectorNormalize(vEnemyDir);
		float  fDotPr		= DotProduct(vEnemyFacing,vEnemyDir);

		if (fDotPr < -0.9)
		{
			Vector vDirUp(0,0,1);
			Vector vDir;
			CrossProduct( vEnemyFacing, vDirUp, vDir);

			Vector crossProduct;
			CrossProduct(vEnemyFacing, vEnemyDir, crossProduct);
			if (crossProduct.y < 0)
			{
				vDir = vDir * -1;
			}
			return (vDir);
		}
		else if (fDotPr < -0.85)
		{
			Vector vDirUp(0,0,1);
			Vector vDir;
			CrossProduct( vEnemyFacing, vDirUp, vDir);

			Vector crossProduct;
			CrossProduct(vEnemyFacing, vEnemyDir, crossProduct);
			if (random->RandomInt(0,1))
			{
				vDir = vDir * -1;
			}
			return (vDir);
		}
	}
	return vec3_origin;
}
//-----------------------------------------------------------------------------
// Purpose: Handles all flight movement.
// Input  : flInterval - Seconds to simulate.
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::MoveVoloxelicanFly( float flInterval )
{
	//
	// Bound interval so we don't get ludicrous motion when debugging
	// or when framerate drops catastrophically.  
	//
	if (flInterval > 1.0)
	{
		flInterval = 1.0;
	}

	if( m_nCurrentInAirState == InAirState_RangeAttacking)
	{
		if (GetEnemy() == NULL)
			return;

		if ( flInterval <= 0 )
			return;

		Vector vTargetPos = GetEnemyLKP();

		float goalDist = ( UniqueSpitDistanceClose() + ( ( UniqueSpitDistanceFar() - UniqueSpitDistanceClose() ) / 2 ) );
		Vector idealPos = IdealGoalForMovement( vTargetPos, GetAbsOrigin(), goalDist, UniqueSpitDistanceClose() );

		MoveToTarget( flInterval, idealPos );

		// Face target
		Vector dir = GetEnemy()->GetAbsOrigin() - GetAbsOrigin();
		VectorNormalize(dir);

		GetMotor()->SetIdealYawAndUpdate( dir, AI_KEEP_YAW_SPEED );

		//
		// ------------------------------------------------
		//  Also keep my distance from other squad members
		//  
		// ------------------------------------------------

		Vector vFlyDirection = GetAbsVelocity();
		VectorNormalize( vFlyDirection );

		if ( m_pSquad )
		{
			CBaseEntity*	pNearest	= m_pSquad->NearestSquadMember(this);
			if (pNearest)
			{
				Vector			vNearestDir = (pNearest->GetLocalOrigin() - GetLocalOrigin());
				if (vNearestDir.Length() < 80) // 80 = SCANNER_SQUAD_FLY_DIST
				{
					vNearestDir		= pNearest->GetLocalOrigin() - GetLocalOrigin();
					VectorNormalize(vNearestDir);
					vFlyDirection  -= 0.5*vNearestDir;
					SetAbsVelocity(vFlyDirection);
				}
			}
		}

		// ---------------------------------------------------------
		//  Add evasion if I have taken damage recently
		// ---------------------------------------------------------
		if ((m_flLastDamageTime + 2) > gpGlobals->curtime)
		{
			vFlyDirection = vFlyDirection + VelocityToEvade(GetEnemyCombatCharacterPointer());
			SetAbsVelocity(vFlyDirection);
		}

	}
	else if ( m_nCurrentInAirState == InAirState_MoveToLanding )
	{
		if ( flInterval <= 0 )
			return;

		if(	!m_bLanding	)
		{
			Vector vTargetPos = GetEnemyLKP();
			Vector idealPos = IdealGoalForMovement( vTargetPos, GetAbsOrigin(), UniqueSpitDistanceClose(), 0 );

			MoveToTarget( flInterval, idealPos );
		}
		if(CheckLanding())
		{
			SetFlyingState(FlyState_Walking);
			TaskMovementComplete();
		}
	}
	else
	{
		//
		// Determine the goal of our movement.
		//
		Vector vecMoveGoal = GetAbsOrigin();
	
		if ( GetNavigator()->IsGoalActive() )
		{
			vecMoveGoal = GetNavigator()->GetCurWaypointPos();

			if ( GetNavigator()->CurWaypointIsGoal() == false  )
			{
  				AI_ProgressFlyPathParams_t params( MASK_NPCSOLID );
				params.bTrySimplify = false;

				GetNavigator()->ProgressFlyPath( params ); // ignore result, voloxelican handles completion directly

				// Fly towards the hint.
				if ( GetNavigator()->GetPath()->GetCurWaypoint() )
					vecMoveGoal = GetNavigator()->GetCurWaypointPos();
			}
		}
		else
		{
			// No movement goal.
			vecMoveGoal = GetAbsOrigin(); // tace down to see if I can land here
			SetAbsVelocity( vec3_origin );
			return;
		}

		Vector vecMoveDir = ( vecMoveGoal - GetAbsOrigin() );
		Vector vForward;
		AngleVectors( GetAbsAngles(), &vForward );
		//
		// Fly towards the movement goal.
		//
		float flDistance = ( vecMoveGoal - GetAbsOrigin() ).Length();

		if ( vecMoveGoal != m_vDesiredTarget )
			m_vDesiredTarget = vecMoveGoal;
		else
		{
			m_vCurrentTarget = ( m_vDesiredTarget - GetAbsOrigin() );
			VectorNormalize( m_vCurrentTarget );
		}

		float flLerpMod = 0.25f;

		if ( flDistance <= 256.0f )
			flLerpMod = 1.0f - ( flDistance / 256.0f );

		VectorLerp( vForward, m_vCurrentTarget, flLerpMod, vForward );

		if ( flDistance < VOLOXELICAN_AIRSPEED * flInterval )
		{
			if ( GetNavigator()->IsGoalActive() )
			{
				if ( GetNavigator()->CurWaypointIsGoal() )
					m_bReachedMoveGoal = true;
				else
					GetNavigator()->AdvancePath();
			}
			else
				m_bReachedMoveGoal = true;
		}

		if ( GetHintNode() )
		{
			AIMoveTrace_t moveTrace;
			GetMoveProbe()->MoveLimit( NAV_FLY, GetAbsOrigin(), GetNavigator()->GetCurWaypointPos(), MASK_NPCSOLID, GetNavTargetEntity(), &moveTrace );

			//See if it succeeded
			if ( IsMoveBlocked( moveTrace.fStatus ) )
			{
				Vector vNodePos = vecMoveGoal;
				GetHintNode()->GetPosition(this, &vNodePos);
			
				GetNavigator()->SetGoal( vNodePos );
			}
		}

		//
		// Look to see if we are going to hit anything.
		//
		VectorNormalize( vForward );
		Vector vecDeflect;
		if ( Probe( vForward, VOLOXELICAN_AIRSPEED * flInterval, vecDeflect ) )
		{
			vForward = vecDeflect;
			VectorNormalize( vForward );
		}

		SetAbsVelocity( vForward * VOLOXELICAN_AIRSPEED );

		if ( GetAbsVelocity().Length() > 0 && GetNavigator()->CurWaypointIsGoal() && flDistance < VOLOXELICAN_AIRSPEED )
			SetIdealActivity( (Activity)ACT_VOLOXELICAN_LAND );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Monitor the antlion's jump to play the proper landing sequence
//-----------------------------------------------------------------------------
bool CNPC_Voloxelican::CheckLanding( void )
{
	trace_t	tr;
	Vector	testPos;

	//Amount of time to predict forward
	const float	timeStep = 0.1f;

	//Roughly looks one second into the future
	testPos = GetAbsOrigin() + ( GetAbsVelocity() * timeStep );
	testPos[2] -= ( 0.5 * sv_gravity.GetFloat() * GetGravity() * timeStep * timeStep);

	// Look below
	AI_TraceHull( GetAbsOrigin(), testPos, NAI_Hull::Mins( GetHullType() ), NAI_Hull::Maxs( GetHullType() ), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );

	if ( tr.DidHitWorld() )
	{
		//See if we're about to contact, or have already contacted the ground
		if ( ( GetFlags() & FL_ONGROUND ) ) //( tr.fraction != 1.0f ) ||
		{
			//might be landing on another voloxelican!
			int	sequence = SelectWeightedSequence( (Activity)ACT_VOLOXELICAN_LAND );

			if ( GetSequence() != sequence )
			{
				VacateStrategySlot();
				SetIdealActivity( (Activity) ACT_VOLOXELICAN_LAND );

				m_bLanding = true;
				SetFlyingState( FlyState_Falling );
				return false;
			}
			VacateStrategySlot();

			return IsActivityFinished();
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Accelerates toward a given position.
// Input  : flInterval - Time interval over which to move.
//			vecMoveTarget - Position to move toward.
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::MoveToTarget( float flInterval, const Vector &vecMoveTarget )
{
	// -------------------------------------
	// Move towards our target
	// -------------------------------------
	float myAccel;
	float myZAccel = 400.0f; //400
	float myDecay  = 0.15f;

	// Get the relationship between my current velocity and the way I want to be going.
	Vector vecCurrentDir = GetAbsVelocity();
	VectorNormalize( vecCurrentDir );

	Vector targetDir = vecMoveTarget - GetAbsOrigin();
	float flDist = VectorNormalize(targetDir);

	float flDot;
	flDot = DotProduct( targetDir, vecCurrentDir );

	if( flDot > 0.25 )
	{
		// If my target is in front of me, my flight model is a bit more accurate.
		myAccel = 380; //250
	}
	else
	{
		// Have a harder time correcting my course if I'm currently flying away from my target.
		myAccel = 250; //128
	}

	if ( myAccel > flDist / flInterval )
		myAccel = flDist / flInterval;

	if ( myZAccel > flDist / flInterval )
		myZAccel = flDist / flInterval;

	MoveInDirection( flInterval, targetDir, myAccel, myZAccel, myDecay );
}

//-----------------------------------------------------------------------------
// Purpose: Looks ahead to see if we are going to hit something. If we are, a
//			recommended avoidance path is returned.
// Input  : vecMoveDir - 
//			flSpeed - 
//			vecDeflect - 
// Output : Returns true if we hit something and need to deflect our course,
//			false if all is well.
//-----------------------------------------------------------------------------
bool CNPC_Voloxelican::Probe( const Vector &vecMoveDir, float flSpeed, Vector &vecDeflect )
{
	//
	// Look 1/2 second ahead.
	//
	trace_t tr;
	AI_TraceHull( GetAbsOrigin(), GetAbsOrigin() + vecMoveDir * flSpeed, GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, HL2COLLISION_GROUP_ANTLION, &tr );
	if ( tr.fraction < 1.0f )
	{
		//
		// If we hit something, deflect flight path parallel to surface hit.
		//
		Vector vecUp;
		CrossProduct( vecMoveDir, tr.plane.normal, vecUp );
		CrossProduct( tr.plane.normal - Vector(90), vecUp, vecDeflect ); //tr.plane.normal
		VectorNormalize( vecDeflect );
		return true;
	}

	vecDeflect = vec3_origin;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Switches between flying mode and ground mode.
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::SetFlyingState( FlyState_t eState )
{
	if ( eState == FlyState_Flying )
	{
		// Flying
		SetGroundEntity( NULL );
		AddFlag( FL_FLY );
		SetNavType( NAV_FLY );
		CapabilitiesRemove( bits_CAP_MOVE_GROUND );
		CapabilitiesAdd( bits_CAP_MOVE_FLY );
		CapabilitiesAdd( bits_CAP_INNATE_RANGE_ATTACK1 );
		SetMoveType( MOVETYPE_STEP );
		m_vLastStoredOrigin = GetAbsOrigin();
		m_flLastStuckCheck = gpGlobals->curtime + 3.0f;
		m_flGroundIdleMoveTime = gpGlobals->curtime + random->RandomFloat( 5.0f, 10.0f );

		// when will I get tired of being in the air?
		if ( GetEnemy() != NULL )
			m_nCurrentInAirState = InAirState_RangeAttacking;
		else
			m_nCurrentInAirState = InAirState_Idle;

		m_flGetTiredTime = gpGlobals->curtime + random->RandomInt( 5, 8 );
		m_bLanding = false;
	}
	else if ( eState == FlyState_Walking )
	{
		// Walking
		QAngle angles = GetAbsAngles();
		angles[PITCH] = 0.0f;
		angles[ROLL] = 0.0f;
		SetAbsAngles( angles );

		RemoveFlag( FL_FLY );
		SetNavType( NAV_GROUND );
		CapabilitiesRemove( bits_CAP_MOVE_FLY );
		CapabilitiesRemove( bits_CAP_INNATE_RANGE_ATTACK1 );
		CapabilitiesAdd( bits_CAP_MOVE_GROUND );
		SetMoveType( MOVETYPE_STEP );
		m_vLastStoredOrigin = vec3_origin;
		m_flGroundIdleMoveTime = gpGlobals->curtime + random->RandomFloat( 5.0f, 10.0f );

		// when will I get tired of being on the ground?
		m_flNextCanFlyTime = gpGlobals->curtime + random->RandomFloat( 5.0f, 10.0f );
		m_bLanding = false;
	}
	else
	{
		// Falling / Landing
		QAngle angles = GetAbsAngles();
		angles[PITCH] = 0.0f;
		angles[ROLL] = 0.0f;
		SetAbsAngles( angles );

		RemoveFlag( FL_FLY );
		SetNavType( NAV_GROUND );
		CapabilitiesRemove( bits_CAP_MOVE_FLY );
		CapabilitiesRemove( bits_CAP_INNATE_RANGE_ATTACK1 );
		CapabilitiesAdd( bits_CAP_MOVE_GROUND );
		SetMoveType( MOVETYPE_STEP );
		m_flGroundIdleMoveTime = gpGlobals->curtime + random->RandomFloat( 5.0f, 10.0f );
		SetAbsVelocity(vec3_origin);
		SetLocalVelocity(vec3_origin);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Performs a takeoff. Called via an animation event at the moment
//			our feet leave the ground.
// Input  : pGoalEnt - The entity that we are going to fly toward.
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::Takeoff( const Vector &vGoal )
{
	if ( vGoal != vec3_origin )
	{
		//
		// Lift us off ground so engine doesn't instantly reset FL_ONGROUND.
		//
		UTIL_SetOrigin( this, GetAbsOrigin() + Vector( 0 , 0 , 1 ));

		//
		// Fly straight at the goal entity at our maximum airspeed.
		//
		Vector vecMoveDir = vGoal - GetAbsOrigin();
		VectorNormalize( vecMoveDir );
		
		// FIXME: pitch over time

		SetFlyingState( FlyState_Flying );

		QAngle angles;
		VectorAngles( vecMoveDir, angles );
		SetAbsAngles( angles );

		SetAbsVelocity( vecMoveDir * VOLOXELICAN_TAKEOFF_SPEED );
	}
}

void CNPC_Voloxelican::TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator)
{
	CTakeDamageInfo	newInfo = info;

	if ( info.GetDamageType() & DMG_PHYSGUN )
	{
		Vector	puntDir = ( info.GetDamageForce() * 5000.0f );

		newInfo.SetDamage( m_iMaxHealth );

		PainSound( newInfo );
		newInfo.SetDamageForce( puntDir );
	}

	BaseClass::TraceAttack( newInfo, vecDir, ptr, pAccumulator );
}


void CNPC_Voloxelican::StartTargetHandling( CBaseEntity *pTargetEnt )
{
	AI_NavGoal_t goal( GOALTYPE_PATHCORNER, pTargetEnt->GetAbsOrigin(),
					   ACT_FLY,
					   AIN_DEF_TOLERANCE, AIN_YAW_TO_DEST);

	if ( !GetNavigator()->SetGoal( goal ) )
	{
		DevWarning( 2, "Can't Create Route!\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTask - 
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_VOLOXELICAN_SET_FLY_ATTACK:
		{
			TaskComplete();
			break;
		}
	case TASK_VOLOXELICAN_WAIT_POST_RANGE:
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
		case TASK_VOLOXELICAN_WAIT_POST_MELEE:
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
		case TASK_VOLOXELICAN_TAKEOFF:
		{
			if ( random->RandomInt( 1, 4 ) == 1 )
				AlertSound();

			FlapSound();

			SetIdealActivity( ( Activity )ACT_VOLOXELICAN_TAKEOFF );

			break;
		}

		case TASK_VOLOXELICAN_PICK_EVADE_GOAL:
		{
			if ( GetEnemy() != NULL )
			{
				//
				// Get our enemy's position in x/y.
				//
				Vector vecEnemyOrigin = GetEnemy()->GetAbsOrigin();
				vecEnemyOrigin.z = GetAbsOrigin().z;

				//
				// Pick a hop goal a random distance along a vector away from our enemy.
				//
				m_vSavePosition = GetAbsOrigin() - vecEnemyOrigin;
				VectorNormalize( m_vSavePosition );
				m_vSavePosition = GetAbsOrigin() + m_vSavePosition * ( 32 + random->RandomInt( 0, 32 ) );

				GetMotor()->SetIdealYawToTarget( m_vSavePosition );
				TaskComplete();
			}
			else
			{
				TaskFail( "No enemy" );
			}
			break;
		}

		case TASK_VOLOXELICAN_FALL_TO_GROUND:
		{
			SetFlyingState( FlyState_Falling );
			break;
		}

		case TASK_FIND_HINTNODE:
		{
			if ( GetGoalEnt() )
			{
				TaskComplete();
				return;
			}
			// Overloaded because we search over a greater distance.
			if ( !GetHintNode() )
			{
				SetHintNode(CAI_HintManager::FindHint( this, HINT_CROW_FLYTO_POINT, bits_HINT_NODE_NEAREST | bits_HINT_NODE_USE_GROUP, 10000 ));
			}

			if ( GetHintNode() )
				TaskComplete();
			else
				TaskFail( FAIL_NO_HINT_NODE );
			break;
		}

		case TASK_GET_PATH_TO_HINTNODE:
		{
			//How did this happen?!
			if ( GetGoalEnt() == this )
			{
				SetGoalEnt( NULL );
			}

			if ( GetGoalEnt() )
			{
				SetFlyingState( FlyState_Flying );
				StartTargetHandling( GetGoalEnt() );
			
				m_bReachedMoveGoal = false;
				TaskComplete();
				SetHintNode( NULL );
				return;
			}

			if ( GetHintNode() )
			{
				Vector vHintPos;
				GetHintNode()->GetPosition(this, &vHintPos);
		
				SetNavType( NAV_FLY );
				CapabilitiesAdd( bits_CAP_MOVE_FLY );
				// @HACKHACK: Force allow triangulation. Too many HL2 maps were relying on this feature WRT fly nodes (toml 8/1/2007)
				NPC_STATE state = GetState();
				m_NPCState = NPC_STATE_SCRIPT;
				bool bFoundPath = GetNavigator()->SetGoal( vHintPos );
				m_NPCState = state;
				if ( !bFoundPath )
				{
					GetHintNode()->DisableForSeconds( .3 );
					SetHintNode(NULL);
				}
				CapabilitiesRemove( bits_CAP_MOVE_FLY );
			}

			if ( GetHintNode() )
			{
				m_bReachedMoveGoal = false;
				TaskComplete();
			}
			else
				TaskFail( FAIL_NO_ROUTE );

			break;
		}

		//
		// We have failed to fly normally. Pick a random "up" direction and fly that way.
		//
		case TASK_VOLOXELICAN_FLY:
		{
			float flYaw = UTIL_AngleMod( random->RandomInt( -180, 180 ) );

			Vector vecNewVelocity( cos( DEG2RAD( flYaw ) ), sin( DEG2RAD( flYaw ) ), random->RandomFloat( 0.1f, 0.5f ) );
			vecNewVelocity *= VOLOXELICAN_AIRSPEED;
			SetAbsVelocity( vecNewVelocity );

			SetIdealActivity( ACT_FLY );
			break;
		}

		case TASK_VOLOXELICAN_PICK_RANDOM_GOAL:
		{
			m_vSavePosition = GetLocalOrigin() + Vector( random->RandomFloat( -48.0f, 48.0f ), random->RandomFloat( -48.0f, 48.0f ), 0 );
			TaskComplete();
			break;
		}

		case TASK_VOLOXELICAN_HOP:
		{
			SetIdealActivity( ACT_HOP );
			m_flHopStartZ = GetLocalOrigin().z;
			break;
		}

		case TASK_VOLOXELICAN_WAIT_FOR_BARNACLE_KILL:
			break;

		default:
		{
			BaseClass::StartTask( pTask );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTask - 
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_VOLOXELICAN_WAIT_POST_RANGE:
		{
			if ( IsWaitFinished() )
				TaskComplete();
		}
		break;
		case TASK_VOLOXELICAN_WAIT_POST_MELEE:
		{
			if ( IsWaitFinished() )
				TaskComplete();
		}
		break;
		case TASK_VOLOXELICAN_TAKEOFF:
		{
			if ( GetNavigator()->IsGoalActive() )
				GetMotor()->SetIdealYawToTargetAndUpdate( GetAbsOrigin() + GetNavigator()->GetCurWaypointPos(), AI_KEEP_YAW_SPEED );
			else
				TaskFail( FAIL_NO_ROUTE );

			if ( IsActivityFinished() )
			{
				TaskComplete();
				SetIdealActivity( ACT_FLY );
			}
			
			break;
		}

		case TASK_VOLOXELICAN_HOP:
		{
			if ( IsActivityFinished() )
			{
				TaskComplete();
				SetIdealActivity( ACT_IDLE );
			}

			if ( ( GetAbsOrigin().z < m_flHopStartZ ) && ( !( GetFlags() & FL_ONGROUND ) ) )
			{
				//
				// We've hopped off of something! See if we're going to fall very far.
				//
				trace_t tr;
				AI_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -32 ), MASK_SOLID, this, HL2COLLISION_GROUP_ANTLION, &tr );
				if ( tr.fraction == 1.0f )
				{
					//
					// We're falling! Better fly away. SelectSchedule will check ONGROUND and do the right thing.
					//
					TaskComplete();
				}
				else
				{
					//
					// We'll be okay. Don't check again unless what we're hopping onto moves
					// out from under us.
					//
					m_flHopStartZ = GetAbsOrigin().z - ( 32 * tr.fraction );
				}
			}

			break;
		}

		//
		// Face the direction we are flying.
		//
		case TASK_VOLOXELICAN_FLY:
		{
			GetMotor()->SetIdealYawToTargetAndUpdate( GetAbsOrigin() + GetAbsVelocity(), AI_KEEP_YAW_SPEED );

			break;
		}

		case TASK_VOLOXELICAN_FALL_TO_GROUND:
		{
			if ( GetFlags() & FL_ONGROUND )
			{
				SetFlyingState( FlyState_Walking );
				TaskComplete();
			}
			break;
		}

		case TASK_VOLOXELICAN_WAIT_FOR_BARNACLE_KILL:
		{
			if ( m_flNextFlinchTime < gpGlobals->curtime )
			{
				m_flNextFlinchTime = gpGlobals->curtime + random->RandomFloat( 0.5f, 2.0f );
				EmitSound( "NPC_Voloxelican.Squawk" );
			}
			break;
		}

		default:
			CAI_BaseNPC::RunTask( pTask );
			break;
	}
}


//------------------------------------------------------------------------------
// Purpose: Override to do voloxelican specific gibs.
// Output : Returns true to gib, false to not gib.
//-----------------------------------------------------------------------------
bool CNPC_Voloxelican::CorpseGib( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_Voloxelican.Gib" );
	return true;
}

//-----------------------------------------------------------------------------
// Don't allow ridiculous forces to be applied to the voloxelican. It only weighs
// 1.5kg, so extreme forces will give it ridiculous velocity.
//-----------------------------------------------------------------------------
#define VOLOXELICAN_RAGDOLL_SPEED_LIMIT	1000.0f  // Voloxelican ragdoll speed limit in inches per second.
bool CNPC_Voloxelican::BecomeRagdollOnClient( const Vector &force )
{
	Vector newForce = force;
	
	if( VPhysicsGetObject() )
	{
		float flMass = VPhysicsGetObject()->GetMass();
		float speed = VectorNormalize( newForce );
		speed = min( speed, (VOLOXELICAN_RAGDOLL_SPEED_LIMIT * flMass) );
		newForce *= speed;
	}

	return BaseClass::BecomeRagdollOnClient( newForce );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Voloxelican::FValidateHintType( CAI_Hint *pHint )
{
	return( pHint->HintType() == HINT_CROW_FLYTO_POINT );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the activity for the given hint type.
// Input  : sHintType - 
//-----------------------------------------------------------------------------
Activity CNPC_Voloxelican::GetHintActivity( short sHintType, Activity HintsActivity )
{
	if ( sHintType == HINT_CROW_FLYTO_POINT )
	{
		return ACT_FLY;
	}

	return BaseClass::GetHintActivity( sHintType, HintsActivity );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pevInflictor - 
//			pevAttacker - 
//			flDamage - 
//			bitsDamageType - 
//-----------------------------------------------------------------------------
int CNPC_Voloxelican::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// TODO: spew a feather or two
	return BaseClass::OnTakeDamage_Alive( info );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the best new schedule for this NPC based on current conditions.
//-----------------------------------------------------------------------------
int CNPC_Voloxelican::SelectSchedule( void )
{
	//
	// If we're flying, see if we can attack or just fly around
	//
	if ( IsFlying() )
	{
		if (m_nCurrentInAirState == InAirState_RangeAttacking)
		{
			if ( HasCondition( COND_VOLOXELICAN_ENEMY_GOOD_RANGE_DISTANCE ) )
			{
				ClearCondition( COND_VOLOXELICAN_ENEMY_GOOD_RANGE_DISTANCE );

				return SCHED_VOLOXELICAN_AIR_ATTACK;
			}
			else
			{
				return SCHED_VOLOXELICAN_IDLE_FLY;
			}
		}
		else
			return SCHED_VOLOXELICAN_IDLE_FLY;

	}
	else if( !m_bMyAIType )
	{
		if( HasCondition( COND_PROVOKED ) ||  HasCondition( COND_VOLOXELICAN_ENEMY_TOO_CLOSE ) ||  HasCondition( COND_HEAVY_DAMAGE ) ||
			HasCondition( COND_LIGHT_DAMAGE ) || HasCondition( COND_HEAR_DANGER ) || HasCondition( COND_HEAR_COMBAT ) || HasCondition( COND_NEW_ENEMY ))
		{
			if(m_flNextCanFlyTime < gpGlobals->curtime)
			{
				if( RandomInt( 0 , 10 ) < 5 )
					return SCHED_VOLOXELICAN_FLY_AWAY;
				else
					m_flNextCanFlyTime = gpGlobals->curtime + RandomInt(2, 4);
			}
		}
	}
	else if( m_bLanding )
	{
		if( GetEnemy() != NULL )
			return SCHED_VOLOXELICAN_RUN_TO_ATTACK;
		else
			return SCHED_IDLE_STAND;
	}


	if ( HasCondition( COND_VOLOXELICAN_BARNACLED ) )
	{
		// Caught by a barnacle!
		return SCHED_VOLOXELICAN_BARNACLED;
	}

	//
	// If we were told to fly away via our FlyAway input, do so ASAP.
	//
	if ( HasCondition( COND_VOLOXELICAN_FORCED_FLY ) )
	{
		ClearCondition( COND_VOLOXELICAN_FORCED_FLY );
		return SCHED_VOLOXELICAN_FLY_AWAY;
	}

	//
	// If we're not flying but we're not on the ground, start flying.
	// Maybe we hopped off of something? Don't do this immediately upon
	// because we may be falling to the ground on spawn.
	//
	if ( !( GetFlags() & FL_ONGROUND ) && ( gpGlobals->curtime > 2.0 ) == false )
		return SCHED_VOLOXELICAN_FLY_AWAY;

	//
	// If someone we hate is getting WAY too close for comfort, fly away.
	//
	if ( HasCondition( COND_VOLOXELICAN_ENEMY_WAY_TOO_CLOSE ) )
	{
		ClearCondition( COND_VOLOXELICAN_ENEMY_WAY_TOO_CLOSE );
		if( gpGlobals->curtime > m_flNextAttack )
			return SCHED_VOLOXELICAN_GROUND_ATTACK;
	}

	//
	// If someone we hate is getting a little too close for comfort, avoid them.
	//
	if ( HasCondition( COND_VOLOXELICAN_ENEMY_TOO_CLOSE ) )
	{
		ClearCondition( COND_VOLOXELICAN_ENEMY_TOO_CLOSE );

		if ( m_flEnemyDist > 600 )
			return SCHED_VOLOXELICAN_RUN_TO_ATTACK; // walk away, now move in ... SCHED_VOLOXELICAN_WALK_TO_ATTACK
		else if ( m_flEnemyDist > 190 )
			return SCHED_VOLOXELICAN_RUN_TO_ATTACK; //SCHED_VOLOXELICAN_WALK_TO_ATTACK
	}

	switch ( m_NPCState )
	{
		case NPC_STATE_IDLE:
		case NPC_STATE_ALERT:
		case NPC_STATE_COMBAT:
		{
			if ( !IsFlying() )
			{
				//
				// If we are hanging out on the ground, see if it is time to pick a new place to walk to.
				//
				if ( gpGlobals->curtime > m_flGroundIdleMoveTime )
				{
					m_flGroundIdleMoveTime = gpGlobals->curtime + random->RandomFloat( 10.0f, 20.0f );
					return SCHED_VOLOXELICAN_IDLE_WALK;
				}

				return SCHED_IDLE_STAND;
			}
			else
			{
				//
				// If we are just flying around, see if it is time to pick a new place to fly to.
				//
				if ( gpGlobals->curtime > m_flGroundIdleMoveTime )
				{
					m_flGroundIdleMoveTime = gpGlobals->curtime + random->RandomFloat( 10.0f, 20.0f );

					if( m_nCurrentInAirState == InAirState_Idle)
						return SCHED_VOLOXELICAN_IDLE_FLY;
					else if( m_nCurrentInAirState == InAirState_RangeAttacking)
						return SCHED_VOLOXELICAN_FLY_TO_ATTACK;
					else
						return SCHED_VOLOXELICAN_IDLE_FLY;
				}

				// Back up, we're too near an enemy or can't see them
				if ( HasCondition( COND_TOO_CLOSE_TO_ATTACK ) || HasCondition( COND_ENEMY_OCCLUDED ) )
					return SCHED_ESTABLISH_LINE_OF_FIRE;
			}
		}
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Voloxelican::RangeAttack1Conditions( float flDot, float flDist )
{
	if ( GetNextAttack() > gpGlobals->curtime )
		return COND_NOT_FACING_ATTACK;

	if ( flDot < DOT_10DEGREE )
		return COND_NOT_FACING_ATTACK;
	
	if ( flDist > UniqueSpitDistanceFar() )
		return COND_TOO_FAR_TO_ATTACK;

	if ( flDist < UniqueSpitDistanceClose() )
		return COND_TOO_CLOSE_TO_ATTACK;

	return COND_CAN_RANGE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: For innate melee attack
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Voloxelican::MeleeAttack1Conditions ( float flDot, float flDist )
{
	float range = 55; // Jason 55 hardcoded for now, this is the zombie reach

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

					if( flDist <= 55 ) // Jason 55 hardcoded for now, this is the zombie reach
						return COND_CAN_MELEE_ATTACK1;
				}
			}
		}
		return COND_TOO_FAR_TO_ATTACK;
	}

	if (flDot < 0.7)
		return COND_NOT_FACING_ATTACK;

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
	AI_TraceHull( WorldSpaceCenter(), WorldSpaceCenter() + forward * 55, vecMins, vecMaxs, MASK_NPCSOLID, &traceFilter, &tr );

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
		if ( lenTraceSq < Square( 55 * 0.75f ) ) // Jason 55 hardcoded for now, this is the zombie reach
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
			return COND_VOLOXELICAN_LOCAL_MELEE_OBSTRUCTION;
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

void CNPC_Voloxelican::SpitAttack(float flVelocity)
{
	Vector vecMouthPos;

	if( GetAttachment( "mouth", vecMouthPos ) )
	{
		// Get the direction of the target
		Vector vecShootDir;
		//vecShootDir = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
		vecShootDir = (GetEnemy()->EyePosition() - vecMouthPos);

		// get a angle for the model to be drawn
		QAngle angles;
		VectorAngles( vecShootDir, angles );

		CVoloxelicanSpit *pGrenade = (CVoloxelicanSpit*) CreateEntityByName( "voloxelican_spit" );
		pGrenade->SetAbsOrigin( vecMouthPos );
		pGrenade->SetAbsAngles( angles ); //vec3angle
		DispatchSpawn( pGrenade );
		pGrenade->SetThrower( this );
		pGrenade->SetOwnerEntity( this );
		pGrenade->SetAbsVelocity( vecShootDir * flVelocity ); //pGrenade->SetAbsVelocity( vecToss * flVelocity );

		EmitSound( "Zombie.AttackMiss" ); // NPC_Antlion.RunOverByVehicle, 
	}
	m_flNextAttack = gpGlobals->curtime + random->RandomFloat( 3, 5 ); //temporary?
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
CBaseEntity *CNPC_Voloxelican::ClawAttack( float flDist, int iDamage, QAngle &qaViewPunch, Vector &vecVelocityPunch  )
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

				if( GetAttachment( "leftfoot", vecBloodPos ) )
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
// Purpose: Footprints left and right
// Output :
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::SnowFootPrint( bool IsLeft )
{
	trace_t tr;
	Vector traceStart;
	QAngle angles;

	int attachment;

	//!!!PERF - These string lookups here aren't the swiftest, but
	// this doesn't get called very frequently unless a lot of NPCs
	// are using this code.

	if( IsLeft )
		attachment = this->LookupAttachment( "LeftFoot" );
	else
		attachment = this->LookupAttachment( "RightFoot" );

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
						IsLeft ? -7 : 7, // 8 = half width
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
void CNPC_Voloxelican::Precache( void )
{
	BaseClass::Precache();
	
	PrecacheModel( "models/voloxelican/voloxelican.mdl" );
	PrecacheModel( "models/voloxelican_spit.mdl" ); 

	//Voloxelican
	PrecacheScriptSound( "NPC_Crow.Hop" );
	PrecacheScriptSound( "NPC_Stalker.Scream" );
	PrecacheScriptSound( "NPC_FastHeadcrab.Attack" );
	PrecacheScriptSound( "NPC_Crow.Gib" );
	PrecacheScriptSound( "NPC_Stalker.Ambient01" );
	PrecacheScriptSound( "NPC_Stalker.Alert" );
	PrecacheScriptSound( "NPC_Stalker.Die" );
	PrecacheScriptSound( "NPC_Stalker.Pain" );
	PrecacheScriptSound( "NPC_Crow.Flap" );

	// attack sounds
	PrecacheScriptSound( "Zombie.AttackHit" );
	PrecacheScriptSound( "Zombie.AttackMiss" );
	PrecacheScriptSound( "NPC_Antlion.RunOverByVehicle" );
}


//-----------------------------------------------------------------------------
// Purpose: Sounds.
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::IdleSound( void )
{
	EmitSound( "NPC_Stalker.Ambient01" );
}


void CNPC_Voloxelican::AlertSound( void )
{
	EmitSound( "NPC_Stalker.Alert" );
}


void CNPC_Voloxelican::PainSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_Stalker.Pain" );
}


void CNPC_Voloxelican::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_Stalker.Die" );
}

void CNPC_Voloxelican::FlapSound( void )
{
	EmitSound( "NPC_Crow.Flap" );
	m_bPlayedLoopingSound = true;
}

// attack sounds
//-----------------------------------------------------------------------------
// Purpose: Play a random attack hit sound
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::AttackHitSound( void )
{
	EmitSound( "Zombie.AttackHit" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack miss sound
//-----------------------------------------------------------------------------
void CNPC_Voloxelican::AttackMissSound( void )
{
	// Play a random attack miss sound
	EmitSound( "Zombie.AttackMiss" );
}

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  Constant for the type of interaction
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CNPC_Voloxelican::HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt )
{
	if ( interactionType == g_interactionBarnacleVictimDangle )
	{
		// Die instantly
		return false;
	}
	else if ( interactionType == g_interactionBarnacleVictimGrab )
	{
		if ( GetFlags() & FL_ONGROUND )
		{
			SetGroundEntity( NULL );
		}

		// return ideal grab position
		if (data)
		{
			// FIXME: need a good way to ensure this contract
			*((Vector *)data) = GetAbsOrigin() + Vector( 0, 0, 5 );
		}

		StopLoopingSounds();

		SetThink( NULL );
		return true;
	}

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//---------------------------------------------------------
//---------------------------------------------------------
int CNPC_Voloxelican::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
		case SCHED_CHASE_ENEMY:
			if ( HasCondition( COND_VOLOXELICAN_LOCAL_MELEE_OBSTRUCTION ) && !HasCondition(COND_TASK_FAILED) && (IsCurSchedule( SCHED_VOLOXELICAN_WALK_TO_ATTACK || SCHED_VOLOXELICAN_RUN_TO_ATTACK ), false ) )
			{
				return SCHED_COMBAT_PATROL;
			}
			return SCHED_VOLOXELICAN_RUN_TO_ATTACK;
			break;
		case SCHED_MELEE_ATTACK1:
			return SCHED_VOLOXELICAN_GROUND_ATTACK;
		case SCHED_RANGE_ATTACK1:
					return SCHED_VOLOXELICAN_AIR_ATTACK;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Voloxelican::DrawDebugTextOverlays( void )
{
	int nOffset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];
		//Q_snprintf( tempstr, sizeof( tempstr ), "morale: %d", m_nMorale );
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
// Purpose: Determines which sounds the voloxelican cares about.
//-----------------------------------------------------------------------------
int CNPC_Voloxelican::GetSoundInterests( void )
{
	return	SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_DANGER;
}


//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_voloxelican, CNPC_Voloxelican )

	DECLARE_TASK( TASK_VOLOXELICAN_FIND_FLYTO_NODE )
	DECLARE_TASK( TASK_VOLOXELICAN_SET_FLY_ATTACK )
	DECLARE_TASK( TASK_VOLOXELICAN_WAIT_POST_MELEE )
	DECLARE_TASK( TASK_VOLOXELICAN_WAIT_POST_RANGE )
	DECLARE_TASK( TASK_VOLOXELICAN_TAKEOFF )
	DECLARE_TASK( TASK_VOLOXELICAN_FLY )
	DECLARE_TASK( TASK_VOLOXELICAN_PICK_RANDOM_GOAL )
	DECLARE_TASK( TASK_VOLOXELICAN_HOP )
	DECLARE_TASK( TASK_VOLOXELICAN_PICK_EVADE_GOAL )
	DECLARE_TASK( TASK_VOLOXELICAN_WAIT_FOR_BARNACLE_KILL )

	// experiment
	DECLARE_TASK( TASK_VOLOXELICAN_FALL_TO_GROUND )
	DECLARE_TASK( TASK_VOLOXELICAN_PREPARE_TO_FLY_RANDOM )

	DECLARE_ACTIVITY( ACT_VOLOXELICAN_TAKEOFF )
	DECLARE_ACTIVITY( ACT_VOLOXELICAN_SOAR )
	DECLARE_ACTIVITY( ACT_VOLOXELICAN_LAND )

	DECLARE_ANIMEVENT( AE_VOLOXELICAN_HOP )
	DECLARE_ANIMEVENT( AE_VOLOXELICAN_FLY )
	DECLARE_ANIMEVENT( AE_VOLOXELICAN_TAKEOFF )
	DECLARE_ANIMEVENT( AE_VOLOXELICAN_LANDED )
	DECLARE_ANIMEVENT( AE_VOLOXELICAN_MELEE_KICK )
	DECLARE_ANIMEVENT( AE_VOLOXELICAN_SPIT )
	DECLARE_ANIMEVENT( AE_VOLOXELICAN_FOOTPRINT_RIGHT )
	DECLARE_ANIMEVENT( AE_VOLOXELICAN_FOOTPRINT_LEFT )

	DECLARE_CONDITION( COND_VOLOXELICAN_ENEMY_TOO_CLOSE )
	DECLARE_CONDITION( COND_VOLOXELICAN_ENEMY_WAY_TOO_CLOSE )
	DECLARE_CONDITION( COND_VOLOXELICAN_ENEMY_GOOD_RANGE_DISTANCE )

	DECLARE_CONDITION( COND_VOLOXELICAN_LOCAL_MELEE_OBSTRUCTION )
	DECLARE_CONDITION( COND_VOLOXELICAN_FORCED_FLY )
	DECLARE_CONDITION( COND_VOLOXELICAN_BARNACLED )

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_IDLE_WALK,
		
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_IDLE_STAND"
		"		TASK_VOLOXELICAN_PICK_RANDOM_GOAL		0"
		"		TASK_GET_PATH_TO_SAVEPOSITION	0"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_WAIT_PVS					0"
		"		"
		"	Interrupts"
		"		COND_VOLOXELICAN_FORCED_FLY"
		"		COND_PROVOKED"
		"		COND_VOLOXELICAN_ENEMY_TOO_CLOSE"
		"		COND_NEW_ENEMY"
		"		COND_HEAVY_DAMAGE"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
	)

	// to replace the above
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_WALK_TO_ATTACK,
		
		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
		"		 TASK_SET_TOLERANCE_DISTANCE	24"
		"		 TASK_GET_CHASE_PATH_TO_ENEMY	600"
		"		 TASK_WALK_PATH					0"
		"		 TASK_WAIT_FOR_MOVEMENT			0"
		"		 TASK_FACE_ENEMY				0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_TASK_FAILED"
		"		COND_HEAVY_DAMAGE"
	)
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_RUN_TO_ATTACK,
		
		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
		"		 TASK_SET_TOLERANCE_DISTANCE	24"
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
		"		COND_TASK_FAILED"
		"		COND_HEAVY_DAMAGE"
	)
	
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_FLY_TO_ATTACK,

		"	Tasks"
		"		TASK_VOLOXELICAN_SET_FLY_ATTACK		0"
		"		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE"
		"		TASK_WAIT							0.1"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_HOP_AWAY,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_FAILED" //SCHED_VOLOXELICAN_FLY_AWAY
		"		TASK_STOP_MOVING				0"
		"		TASK_VOLOXELICAN_PICK_EVADE_GOAL		0"
		"		TASK_FACE_IDEAL					0"
		"		TASK_VOLOXELICAN_HOP					0"
		"	"
		"	Interrupts"
		"		COND_VOLOXELICAN_FORCED_FLY"
		"		COND_HEAVY_DAMAGE"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_IDLE_FLY,
		
		"	Tasks"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_FLY_AWAY,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_GET_PATH_TO_RANDOM_NODE		2000"
		"		TASK_VOLOXELICAN_TAKEOFF				0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
	)
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_GROUND_ATTACK,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		0" 
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_VOLOXELICAN_POST_MELEE_WAIT"
		""
		"	Interrupts"
	)
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_POST_MELEE_WAIT,

		"	Tasks"
		"		TASK_VOLOXELICAN_WAIT_POST_MELEE		0"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_AIR_ATTACK,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"
		"		TASK_RANGE_ATTACK1		0"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_VOLOXELICAN_POST_AIR_WAIT"
		""
		"	Interrupts"
	)
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_POST_AIR_WAIT,

		"	Tasks"
		"		TASK_VOLOXELICAN_WAIT_POST_RANGE		0"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_FLY,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_VOLOXELICAN_TAKEOFF				0"
		"		TASK_VOLOXELICAN_FLY					0"
		"	"
		"	Interrupts"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_FLY_FAIL,

		"	Tasks"
		"		TASK_VOLOXELICAN_FALL_TO_GROUND		0"
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_VOLOXELICAN_IDLE_WALK"
		"	"
		"	Interrupts"
	)

	//=========================================================
	// Voloxelican is in the clutches of a barnacle
	DEFINE_SCHEDULE
	(
		SCHED_VOLOXELICAN_BARNACLED,

		"	Tasks"
		"		TASK_STOP_MOVING						0"
		"		TASK_SET_ACTIVITY						ACTIVITY:ACT_HOP"
		"		TASK_VOLOXELICAN_WAIT_FOR_BARNACLE_KILL		0"

		"	Interrupts"
	)


AI_END_CUSTOM_NPC()
