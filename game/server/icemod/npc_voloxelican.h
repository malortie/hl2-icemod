//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef NPC_VOLOXELICAN_H
#define NPC_VOLOXELICAN_H
#ifdef _WIN32
#pragma once
#endif

// for footprints
#include "decals.h"

//
// Spawnflags.
//
#define SF_VOLOXELICAN_FLYING			16

#define VOLOXELICAN_TAKEOFF_SPEED		170
#define VOLOXELICAN_AIRSPEED			220 // FIXME: should be about 440, but I need to add acceleration

//
// Custom schedules.
//
enum
{
	SCHED_VOLOXELICAN_IDLE_WALK = LAST_SHARED_SCHEDULE,
	SCHED_VOLOXELICAN_IDLE_FLY,

	//
	// Various levels of wanting to get away from something, selected
	// by current value of m_nMorale.
	//
	SCHED_VOLOXELICAN_WALK_AWAY,
	SCHED_VOLOXELICAN_RUN_AWAY,
	SCHED_VOLOXELICAN_HOP_AWAY,
	SCHED_VOLOXELICAN_FLY_AWAY,

	SCHED_VOLOXELICAN_RUN_TO_ATTACK,
	SCHED_VOLOXELICAN_WALK_TO_ATTACK,
	SCHED_VOLOXELICAN_FLY_TO_ATTACK,

	SCHED_VOLOXELICAN_FLY,
	SCHED_VOLOXELICAN_FLY_FAIL,

	SCHED_VOLOXELICAN_GROUND_ATTACK,
	SCHED_VOLOXELICAN_POST_MELEE_WAIT,
	SCHED_VOLOXELICAN_AIR_ATTACK,
	SCHED_VOLOXELICAN_POST_AIR_WAIT,

	SCHED_VOLOXELICAN_BARNACLED,
};


//
// Custom tasks.
//
enum 
{
	TASK_VOLOXELICAN_FIND_FLYTO_NODE = LAST_SHARED_TASK,
	TASK_VOLOXELICAN_SET_FLY_ATTACK,
	TASK_VOLOXELICAN_WAIT_POST_MELEE,
	TASK_VOLOXELICAN_WAIT_POST_RANGE,
	TASK_VOLOXELICAN_TAKEOFF,
	TASK_VOLOXELICAN_FLY,
	TASK_VOLOXELICAN_PICK_RANDOM_GOAL,
	TASK_VOLOXELICAN_PICK_EVADE_GOAL,
	TASK_VOLOXELICAN_HOP,

	TASK_VOLOXELICAN_FALL_TO_GROUND,
	TASK_VOLOXELICAN_PREPARE_TO_FLY_RANDOM,

	TASK_VOLOXELICAN_WAIT_FOR_BARNACLE_KILL,
};


//
// Custom conditions.
//
enum
{
	COND_VOLOXELICAN_ENEMY_TOO_CLOSE = LAST_SHARED_CONDITION,
	COND_VOLOXELICAN_ENEMY_WAY_TOO_CLOSE,
	COND_VOLOXELICAN_ENEMY_GOOD_RANGE_DISTANCE,
	COND_VOLOXELICAN_LOCAL_MELEE_OBSTRUCTION,
	COND_VOLOXELICAN_FORCED_FLY,
	COND_VOLOXELICAN_BARNACLED,
};

enum FlyState_t
{
	FlyState_Walking = 0,
	FlyState_Flying,
	FlyState_Falling,
	FlyState_Landing,
};

enum InAirState_t
{
	InAirState_Idle = 0,
	InAirState_RangeAttacking,
	InAirState_MoveToLanding,
};
//-----------------------------------------------------------------------------
// The voloxelican class.
//-----------------------------------------------------------------------------
class CNPC_Voloxelican : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_Voloxelican, CAI_BaseNPC );

public:

	//
	// CBaseEntity:
	//
	virtual void Spawn( void );
	virtual void Precache( void );

	virtual Vector BodyTarget( const Vector &posSrc, bool bNoisy = true );

	virtual int DrawDebugTextOverlays( void );

	int RangeAttack1Conditions( float flDot, float flDist );
	// Enemy melee on ground attack
	int MeleeAttack1Conditions ( float flDot, float flDist );
	//virtual float GetClawAttackRange() const { return ZOMBIE_MELEE_REACH; }
	virtual CBaseEntity *ClawAttack( float flDist, int iDamage, QAngle &qaViewPunch, Vector &vecVelocityPunch );
	void SpitAttack(float flVelocity);

	//
	// footprints:
	//
	void SnowFootPrint( bool IsLeft );
	int FootPrintDecal( void ){ int fpdid = decalsystem->GetDecalIndexForName( "FootPrintVoloxelican" ); return fpdid; };

	//
	// CBaseCombatCharacter:
	//
	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual bool CorpseGib( const CTakeDamageInfo &info );
	bool	BecomeRagdollOnClient( const Vector &force );

	//
	// CAI_BaseNPC:
	//
	virtual float MaxYawSpeed( void ) { return 120.0f; }
	
	virtual Class_T Classify( void );
	virtual void GatherEnemyConditions( CBaseEntity *pEnemy );

	virtual void HandleAnimEvent( animevent_t *pEvent );
	virtual int GetSoundInterests( void );
	virtual int SelectSchedule( void );
	virtual int TranslateSchedule( int scheduleType );
	virtual void StartTask( const Task_t *pTask );
	virtual void RunTask( const Task_t *pTask );

	virtual bool HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt );

	virtual void OnChangeActivity( Activity eNewActivity );

	virtual bool OverrideMove( float flInterval );

	virtual bool FValidateHintType( CAI_Hint *pHint );
	virtual Activity GetHintActivity( short sHintType, Activity HintsActivity );

	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void IdleSound( void );
	virtual void AlertSound( void );
	virtual void StopLoopingSounds( void );
	virtual void UpdateEfficiency( bool bInPVS );

	void SetUniqueSpitDistanceFar( int newFarDistance ){ m_nUniqueSpitDistanceFar = newFarDistance; }
	int UniqueSpitDistanceFar( void ){ return m_nUniqueSpitDistanceFar; }
	void SetUniqueSpitDistanceClose( int newCloseDistance ){ m_nUniqueSpitDistanceClose = newCloseDistance; }
	int UniqueSpitDistanceClose( void ){ return m_nUniqueSpitDistanceClose; }

	//attack sounds
	virtual void AttackHitSound( void );
	virtual void AttackMissSound( void );

	void InputFlyAway( inputdata_t &inputdata );
	
	void TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator);
	void StartTargetHandling( CBaseEntity *pTargetEnt );

	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();

protected:
	void	SetFlyingState( FlyState_t eState );

	Vector	IdealGoalForMovement( const Vector &goalPos, const Vector &startPos, float idealRange, float idealHeightDiff );
	Vector	VelocityToEvade(CBaseCombatCharacter *pEnemy);
	bool	GetGoalDirection( Vector *vOut );

	void	MoveToTarget( float flInterval, const Vector &vecMoveTarget );
	void	MoveInDirection( float flInterval, const Vector &targetDir, 
						 float accelXY, float accelZ, float decay)
	{
		decay = ExponentialDecay( decay, 1.0, flInterval );
		accelXY *= flInterval;
		accelZ  *= flInterval;

		Vector m_vCurrentVelocity = GetAbsVelocity();
		m_vCurrentVelocity.x = ( decay * m_vCurrentVelocity.x + accelXY * targetDir.x );
		m_vCurrentVelocity.y = ( decay * m_vCurrentVelocity.y + accelXY * targetDir.y );
		m_vCurrentVelocity.z = ( decay * m_vCurrentVelocity.z + accelZ  * targetDir.z );

		SetAbsVelocity(m_vCurrentVelocity);
	}
	int		m_nCurrentInAirState;

	inline bool IsFlying( void ) const { return GetNavType() == NAV_FLY; }

	void Takeoff( const Vector &vGoal );
	void FlapSound( void );

	void MoveVoloxelicanFly( float flInterval );
	bool Probe( const Vector &vecMoveDir, float flSpeed, Vector &vecDeflect );

	bool CheckLanding( void );

	void UpdateHead( void );
	inline CBaseEntity *EntityToWatch( void );

protected:
	// Pose parameters
	int					m_nPoseFaceVert;
	int					m_nPoseFaceHoriz;

	float m_flGroundIdleMoveTime;

	float m_flEnemyDist;		// Distance to GetEnemy(), cached in GatherEnemyConditions.
	int m_nMorale;				// Used to determine which avoidance schedule to pick. Degrades as I pick avoidance schedules.
	
	bool m_bReachedMoveGoal;

	float m_flHopStartZ;		// Our Z coordinate when we started a hop. Used to check for accidentally hopping off things.

	bool		m_bPlayedLoopingSound;

private:

	Activity NPC_TranslateActivity( Activity eNewActivity );

	Vector				m_vLastStoredOrigin;
	float				m_flLastStuckCheck;
	
	float				m_flDangerSoundTime;

	Vector				m_vDesiredTarget;
	Vector				m_vCurrentTarget;

	bool				m_bStartAsFlying;
	bool				m_bMyAIType;
	int 				m_nUniqueSpitDistanceClose;
	int 				m_nUniqueSpitDistanceFar;
	float				m_flGetTiredTime;
	float				m_flNextCanFlyTime;
	bool				m_bLanding;

};

#endif // NPC_VOLOXELICAN_H
