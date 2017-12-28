//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef NPC_DRAGON_H
#define NPC_DRAGON_H
#ifdef _WIN32
#pragma once
#endif

// for footprints
#include "decals.h"

//
// Custom schedules.
//
enum
{
	SCHED_DRAGON_IDLE_WALK = LAST_SHARED_SCHEDULE,
	SCHED_DRAGON_RUN_TO_ATTACK,
	SCHED_DRAGON_MELEE_ATTACK,
	SCHED_DRAGON_POST_MELEE_WAIT,
	SCHED_DRAGON_RANGE_ATTACK,
	SCHED_DRAGON_POST_RANGE_WAIT,
	SCHED_DRAGON_IVESTIGATE_SOUND,
};


//
// Custom tasks.
//
enum 
{
	TASK_DRAGON_PICK_RANDOM_GOAL = LAST_SHARED_TASK,
	TASK_DRAGON_WAIT_POST_MELEE,
	TASK_DRAGON_WAIT_POST_RANGE,
};

//
// Custom conditions.
//
enum
{
	COND_DRAGON_ENEMY_TOO_FAR = LAST_SHARED_CONDITION,
	COND_DRAGON_ENEMY_GOOD_RANGE_DISTANCE,
	COND_DRAGON_ENEMY_GOOD_MELEE_DISTANCE,
	COND_DRAGON_LOCAL_MELEE_OBSTRUCTION,
};

//-----------------------------------------------------------------------------
// The dragon class.
//-----------------------------------------------------------------------------
class CNPC_Dragon : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_Dragon, CAI_BaseNPC );

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
	virtual CBaseEntity *TorchTrace( void );

	//
	// footprints:
	//
	void SnowFootPrint( bool IsLeft, bool IsFront );
	int FootPrintDecal( void ){ int fpdid = decalsystem->GetDecalIndexForName( "FootPrintDragon" ); return fpdid; };

	//
	// CBaseCombatCharacter:
	//
	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual bool CorpseGib( const CTakeDamageInfo &info );
	bool	BecomeRagdollOnClient( const Vector &force );

	//
	// CAI_BaseNPC:
	//
	virtual Class_T Classify( void );
	virtual void GatherEnemyConditions( CBaseEntity *pEnemy );

	virtual void HandleAnimEvent( animevent_t *pEvent );
	virtual int GetSoundInterests( void );

	int SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode ); //
	virtual int SelectSchedule( void );
	virtual int TranslateSchedule( int scheduleType );
	virtual void StartTask( const Task_t *pTask );
	virtual void RunTask( const Task_t *pTask );

	virtual void OnChangeActivity( Activity eNewActivity );

	virtual bool OverrideMove( float flInterval );
	virtual float MaxYawSpeed ( void );

	virtual void Event_Killed( const CTakeDamageInfo &info );

	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void IdleSound( void );
	virtual void AlertSound( void );
	virtual void StopLoopingSounds( void );

	//attack sounds
	virtual void AttackHitSound( void );
	virtual void AttackMissSound( void );
	
	void TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator);

	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

protected:
	void UpdateHead( void );
	inline CBaseEntity *EntityToWatch( void );

protected:
	// Pose parameters
	int					m_nPoseFaceVert;
	int					m_nPoseFaceHoriz;

	float m_flGroundIdleMoveTime;

	float m_flNextTorchTrace;
	float m_flNextTorchAttack;

	float m_flEnemyDist;		// Distance to GetEnemy(), cached in GatherEnemyConditions.
	int m_nMorale;				// Used to determine which avoidance schedule to pick. Degrades as I pick avoidance schedules.
	
	bool		m_bPlayedLoopingSound; // use for torch

	bool		m_bIsAlert; // used to darken/brighten color intensity depending if the dragon has a target.

private:

	Activity NPC_TranslateActivity( Activity eNewActivity );

	Vector				m_vLastStoredOrigin;
	float				m_flLastStuckCheck;
	
	float				m_flDangerSoundTime;

	Vector				m_vDesiredTarget;
	Vector				m_vCurrentTarget;

	bool				m_bParticle;

protected:
	void SetSkinIllume( float illume ) { m_flSkinIllume = illume; }
	void SetTorchBool( bool torchbool ) { m_bIsTorching = torchbool; }
private:
	CNetworkVar( float, m_flSkinIllume );
	CNetworkVar( bool, m_bIsTorching );
	CNetworkVar( int, m_iSkinColor );

};

#endif // NPC_DRAGON_H
