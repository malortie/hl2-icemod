//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Grey, the aliens who do weird hand gestures to hurt you.
//
//=============================================================================//

#include "cbase.h"
#include "simtimer.h"

#include "npc_BaseZombie.h"

#include "ai_hull.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "npcevent.h" // Jason - needed for event types in HandleAnimEvent

// handglow
#include "Sprite.h"
#include "SpriteTrail.h"

#include "gib.h"
#include "soundenvelope.h"
#include "engine/IEngineSound.h"
#include "ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ACT_FLINCH_PHYSICS

ConVar	sk_grey_health( "sk_grey_health","0");

//
// Controls how soon grey's do their attack
//
#define GREY_ATTACK_MIN_DELAY	.3f		// min seconds before first attack
#define GREY_ATTACK_MAX_DELAY	.9f	// max seconds before first attack

envelopePoint_t envGreyMoanVolumeFast[] =
{
	{	7.0f, 7.0f,
		0.1f, 0.1f,
	},
	{	0.0f, 0.0f,
		0.2f, 0.3f,
	},
};

envelopePoint_t envGreyMoanVolume[] =
{
	{	1.0f, 1.0f,
		0.1f, 0.1f,
	},
	{	1.0f, 1.0f,
		0.2f, 0.2f,
	},
	{	0.0f, 0.0f,
		0.3f, 0.4f,
	},
};

envelopePoint_t envGreyMoanVolumeLong[] =
{
	{	1.0f, 1.0f,
		0.3f, 0.5f,
	},
	{	1.0f, 1.0f,
		0.6f, 1.0f,
	},
	{	0.0f, 0.0f,
		0.3f, 0.4f,
	},
};

envelopePoint_t envGreyMoanIgnited[] =
{
	{	1.0f, 1.0f,
		0.5f, 1.0f,
	},
	{	1.0f, 1.0f,
		30.0f, 30.0f,
	},
	{	0.0f, 0.0f,
		0.5f, 1.0f,
	},
};

// Jason - animation events
int AE_GREY_HANDS_EFFECT;

//=============================================================================
//=============================================================================

class CGrey : public CAI_BlendingHost<CNPC_BaseZombie>
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CGrey, CAI_BlendingHost<CNPC_BaseZombie> );

public:
	CGrey()
	{
	}

	void Spawn( void );
	void Precache( void );

	void SetZombieModel( void );
	void MoanSound(envelopePoint_t *pEnvelope, int iEnvelopeSize) {}
	bool ShouldBecomeTorso( const CTakeDamageInfo &info, float flDamageThreshold ) { return false; }
	bool CanBecomeLiveTorso() { return false; }

	void GatherConditions( void );

	int SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	int TranslateSchedule( int scheduleType );

	virtual int RangeAttack1Conditions( float flDot, float flDist );

	//
	// CBaseAnimating implementation.
	//
	virtual void HandleAnimEvent( animevent_t *pEvent );

	Activity NPC_TranslateActivity( Activity newActivity );

	void OnStateChange( NPC_STATE OldState, NPC_STATE NewState );

	void StartTask( const Task_t *pTask );
	void RunTask( const Task_t *pTask );

	virtual const char *GetLegsModel( void );
	virtual const char *GetTorsoModel( void );
	virtual const char *GetHeadcrabClassname( void );
	virtual const char *GetHeadcrabModel( void );

	virtual bool OnObstructingDoor( AILocalMoveGoal_t *pMoveGoal, 
								 CBaseDoor *pDoor,
								 float distClear, 
								 AIMoveResult_t *pResult );

	int OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo );
	void BuildScheduleTestBits( void );

	void PrescheduleThink( void );
	int SelectSchedule ( void );
	void NPCThink( void );

	void PainSound(const CTakeDamageInfo &info) {}
	void DeathSound(const CTakeDamageInfo &info) {}
	void AlertSound(void) {}
	void IdleSound(void) {}
	void AttackSound(void) {}
	void AttackHitSound(void) {}
	void AttackMissSound(void) {}
	void FootstepSound(bool fRightFoot) {}
	void FootscuffSound(bool fRightFoot) {}

	const char *GetMoanSound(int nSound) { return NULL; }
	
public:
	DEFINE_CUSTOM_AI;

protected:
	static const char *pMoanSounds[];
	
	CHandle<CSprite>		m_pMainGlow;
	CHandle<CSprite>		m_pMainGlow2;
	CHandle<CSpriteTrail>	m_pGlowTrail;
	CHandle<CSpriteTrail>	m_pGlowTrail2;

private:
	Vector				 m_vPositionCharged;

	float m_flAttackTime;	// The soonest we can attack from spotting an enemy.
	
	// hand glow effects
	void CreateEffects( void );
	void RemoveEffects( void );

	// allowed to get hurt (used to kill itself)
	bool m_bAllowDamage;
	// fading away
	bool m_bAmIFading;

};

LINK_ENTITY_TO_CLASS( npc_grey, CGrey );

//---------------------------------------------------------
//---------------------------------------------------------
const char *CGrey::pMoanSounds[] =
{
	 "NPC_BaseGrey.Moan1",
	 "NPC_BaseGrey.Moan2",
	 "NPC_BaseGrey.Moan3",
	 "NPC_BaseGrey.Moan4",
};

//=========================================================
// Conditions
//=========================================================
enum
{
	COND_BLOCKED_BY_DOOR = LAST_BASE_ZOMBIE_CONDITION,
	COND_DOOR_OPENED,
	COND_GREY_CHARGE_TARGET_MOVED,
};

//=========================================================
// Schedules
//=========================================================
enum
{
	SCHED_GREY_BASH_DOOR = LAST_BASE_ZOMBIE_SCHEDULE,
	SCHED_GREY_WANDER_ANGRILY,
	SCHED_GREY_CHARGE_ENEMY,
	SCHED_GREY_FAIL,
	SCHED_GREY_RANGE_ATTACK1,
};

//=========================================================
// Tasks
//=========================================================
enum
{
	TASK_GREY_EXPRESS_ANGER = LAST_BASE_ZOMBIE_TASK,
	TASK_GREY_YAW_TO_DOOR,
	TASK_GREY_ATTACK_DOOR,
	TASK_GREY_CHARGE_ENEMY,
	TASK_GREY_FADE_AWAY,
};

//-----------------------------------------------------------------------------

int ACT_GREY_TANTRUM;
int ACT_GREY_WALLPOUND;
int ACT_GREY_ALARMED;

BEGIN_DATADESC( CGrey )

	DEFINE_FIELD( m_vPositionCharged, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flAttackTime, FIELD_TIME ),
	DEFINE_FIELD( m_pMainGlow, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pMainGlow2, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pGlowTrail, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pGlowTrail2, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bAllowDamage, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bAmIFading, FIELD_BOOLEAN ),

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrey::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( "models/alien/grey.mdl" );
	PrecacheModel( "sprites/greenglow1.vmt" );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CGrey::Spawn( void )
{
	Precache();

	SetBloodColor( DONT_BLEED ); // ICE, ZOMBIE BLOOD_COLOR_RED

	m_iHealth			= sk_grey_health.GetFloat();
	m_flFieldOfView		= 0.2;
	m_bAllowDamage		= false;
	m_bAmIFading		= false;
	m_fIsHeadless		= true;

	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 );

	BaseClass::Spawn();

	m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 1.0, 4.0 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CGrey::PrescheduleThink( void )
{
  	if( gpGlobals->curtime > m_flNextMoanSound )
  	{
  		if( CanPlayMoanSound() )
  		{
			// Classic guy idles instead of moans.
			IdleSound();

  			m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 2.0, 5.0 );
  		}
  		else
 		{
  			m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 1.0, 2.0 );
  		}
  	}

	if ( HasCondition( COND_NEW_ENEMY ) )
	{
		m_flAttackTime = gpGlobals->curtime + random->RandomFloat( GREY_ATTACK_MIN_DELAY, GREY_ATTACK_MAX_DELAY );
	}

	BaseClass::PrescheduleThink();
}

void CGrey::NPCThink( void )
{
	if(m_bAmIFading)
	{
		color32 color = GetRenderColor();
		if( color.a >= 17 )
		{
			color.a -= 17;
			
			if( color.r > 17 )
			{
				color.r-=17;
			}
			if( color.b > 17 )
			{
				color.b-=17;
			}
			if( color.g > 17 )
			{
				color.g-=17;
			}
			SetRenderColor( color.r, color.g, color.b, color.a );
			
			if(m_pMainGlow != NULL)
			{
				color32 MGcolor = m_pMainGlow->GetRenderColor();
				if( MGcolor.a >= 17 )
				{
					MGcolor.a -= 17;
				}
				m_pMainGlow->SetTransparency( kRenderGlow, 255, 255, 255, MGcolor.a, kRenderFxNoDissipation );
				if(m_pMainGlow2 != NULL)
				{
					m_pMainGlow2->SetTransparency( kRenderGlow, 255, 255, 255, MGcolor.a, kRenderFxNoDissipation );
				}
			}

		}
		else
		{
			// remove the hand effects
			RemoveEffects();

			// dissappear
			AddEffects( EF_NODRAW );

			// kill
			m_bAllowDamage = true;
			m_iHealth = 5;
			OnTakeDamage( CTakeDamageInfo( this, this, m_iHealth * 2, DMG_GENERIC | DMG_REMOVENORAGDOLL ) );
		}
	}
	BaseClass::NPCThink();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CGrey::SelectSchedule ( void )
{
	if( HasCondition( COND_PHYSICS_DAMAGE ) && !m_ActBusyBehavior.IsActive() )
	{
		return SCHED_FLINCH_PHYSICS;
	}

	return BaseClass::SelectSchedule();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the classname (ie "npc_headcrab") to spawn when our headcrab bails.
//-----------------------------------------------------------------------------
const char *CGrey::GetHeadcrabClassname( void )
{
	return "npc_headcrab";
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CGrey::GetHeadcrabModel( void )
{
	return "models/headcrabclassic.mdl";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CGrey::GetLegsModel( void )
{
	return "models/zombie/classic_legs.mdl";
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CGrey::GetTorsoModel( void )
{
	return "models/zombie/classic_torso.mdl";
}


//---------------------------------------------------------
//---------------------------------------------------------
void CGrey::SetZombieModel( void )
{
	Hull_t lastHull = GetHullType();

	SetModel( "models/alien/grey.mdl" );

	SetHullType( HULL_MEDIUM );

	SetHullSizeNormal( true );
	SetDefaultEyeOffset();
	SetActivity( ACT_IDLE );

	// hull changed size, notify vphysics
	// UNDONE: Solve this generally, systematically so other
	// NPCs can change size
	if ( lastHull != GetHullType() )
	{
		if ( VPhysicsGetObject() )
		{
			SetupVPhysicsHull();
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CGrey::GatherConditions( void )
{
	BaseClass::GatherConditions();

	static int conditionsToClear[] = 
	{
		COND_GREY_CHARGE_TARGET_MOVED,
	};

	ClearConditions( conditionsToClear, ARRAYSIZE( conditionsToClear ) );


	if ( ConditionInterruptsCurSchedule( COND_GREY_CHARGE_TARGET_MOVED ) )
	{
		if ( GetNavigator()->IsGoalActive() )
		{
			const float CHARGE_RESET_TOLERANCE = 60.0;
			if ( !GetEnemy() ||
				 ( m_vPositionCharged - GetEnemyLKP()  ).Length() > CHARGE_RESET_TOLERANCE )
			{
				SetCondition( COND_GREY_CHARGE_TARGET_MOVED );
			}
				 
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

int CGrey::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( failedSchedule != SCHED_GREY_CHARGE_ENEMY && 
		 IsPathTaskFailure( taskFailCode ) &&
		 random->RandomInt( 1, 100 ) < 50 )
	{
		return SCHED_GREY_CHARGE_ENEMY;
	}

	if ( failedSchedule != SCHED_GREY_WANDER_ANGRILY &&
		 ( failedSchedule == SCHED_TAKE_COVER_FROM_ENEMY || 
		   failedSchedule == SCHED_CHASE_ENEMY_FAILED ) )
	{
		return SCHED_GREY_WANDER_ANGRILY;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//---------------------------------------------------------
//---------------------------------------------------------

int CGrey::TranslateSchedule( int scheduleType )
{
	if ( scheduleType == SCHED_RANGE_ATTACK1 )
		return SCHED_GREY_RANGE_ATTACK1;

	if ( scheduleType == SCHED_COMBAT_FACE && IsUnreachable( GetEnemy() ) )
		return SCHED_TAKE_COVER_FROM_ENEMY;

	if ( scheduleType == SCHED_FAIL ) // not a torso
		return SCHED_GREY_FAIL;

	return BaseClass::TranslateSchedule( scheduleType );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CGrey::RangeAttack1Conditions( float flDot, float flDist )
{
	if ( m_flAttackTime > gpGlobals->curtime )
	{
		return 0;
	}

	return COND_CAN_RANGE_ATTACK1;
}
//---------------------------------------------------------

void CGrey::HandleAnimEvent( animevent_t *pEvent )
{
	
	if ( pEvent->event == AE_GREY_HANDS_EFFECT )
	{
		// do something amazing!
		CreateEffects();
		// shake the world
		UTIL_ScreenShake( GetAbsOrigin(), 7, 100.0f, 1.5f, 2000, SHAKE_START, false );
		// slow player movement
		if ( GetEnemy() != NULL )
		{
			if( GetEnemy()->IsPlayer() )
			{
				CBasePlayer *pPlayer = ToBasePlayer( GetEnemy() );
				if (!pPlayer)
				{
					return;
				}
				else
				{
					// slow down the player
					if( pPlayer->IsSuitEquipped() ){
						pPlayer->SetMaxSpeed( 100 );
					} else {
						pPlayer->SetMaxSpeed( 90 );
					}
				}
			}
		}

		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

Activity CGrey::NPC_TranslateActivity( Activity newActivity )
{
	newActivity = BaseClass::NPC_TranslateActivity( newActivity );

	if ( newActivity == ACT_RUN )
		return ACT_WALK;

	return newActivity;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CGrey::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
	BaseClass::OnStateChange( OldState, NewState );
}

//---------------------------------------------------------
//---------------------------------------------------------

void CGrey::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_GREY_EXPRESS_ANGER:
		{
			if ( random->RandomInt( 1, 4 ) == 2 )
			{
				SetIdealActivity( (Activity)ACT_GREY_TANTRUM );
			}
			else
			{
				TaskComplete();
			}

			break;
		}

	case TASK_GREY_CHARGE_ENEMY:
		{
			if ( !GetEnemy() )
				TaskFail( FAIL_NO_ENEMY );
			else if ( GetNavigator()->SetVectorGoalFromTarget( GetEnemy()->GetLocalOrigin() ) )
			{
				m_vPositionCharged = GetEnemy()->GetLocalOrigin();
				TaskComplete();
			}
			else
				TaskFail( FAIL_NO_ROUTE );
			break;
		}
	case TASK_RANGE_ATTACK1:
		{
			SetActivity( ACT_RANGE_ATTACK1 );
			if ( IsActivityFinished() )
			{
				TaskComplete();
			}
			break;
		}
	case TASK_GREY_FADE_AWAY:
		{
			SetActivity( ACT_FLY );
			if ( IsActivityFinished() )
			{
				TaskComplete();
			}
			break;
		}
	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

void CGrey::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_GREY_CHARGE_ENEMY:
		{
			break;
		}

	case TASK_GREY_EXPRESS_ANGER:
		{
			if ( IsActivityFinished() )
			{
				TaskComplete();
			}
			break;
		}
	case TASK_RANGE_ATTACK1:
		{
			if ( IsActivityFinished() )
			{
				if ( GetEnemy() != NULL )
				{
					if( GetEnemy()->IsPlayer() )
					{
						CBasePlayer *pPlayer = ToBasePlayer( GetEnemy() );
						if (!pPlayer)
						{
							return;
						}
						else
						{
							// return player speeds to normal
							if( pPlayer->IsSuitEquipped() ){
								pPlayer->SetMaxSpeed( 190 );
							} else {
								pPlayer->SetMaxSpeed( 150 );
							}
							// Only do attack effects if player is looking in my direction
							Vector vLookDir = pPlayer->EyeDirection3D();
							Vector vTargetDir = GetAbsOrigin() - pPlayer->EyePosition();
							VectorNormalize( vTargetDir );

							float fDotPr = DotProduct( vLookDir,vTargetDir );
							if ( fDotPr > 0 )
							{
								// hurt player
								RadiusDamage( CTakeDamageInfo( this, GetOwnerEntity(), 60, DMG_SONIC ), GetAbsOrigin(), 1700, CLASS_ZOMBIE, NULL );

								//remove shadow
								AddEffects( EF_NOSHADOW );

								// Throw the player away
								Vector up, forward;
								GetVectors( &forward, NULL, &up );
								AngleVectors( pPlayer->GetAbsAngles(), &forward, NULL, &up );
								forward = -(forward * 200);
								pPlayer->VelocityPunch( forward );
							}
							else
							{
								// player isnt looking, just dissappear
								RemoveEffects();
								AddEffects( EF_NODRAW );
								m_bAllowDamage = true;
								m_iHealth = 5;
								OnTakeDamage( CTakeDamageInfo( this, this, m_iHealth * 2, DMG_GENERIC | DMG_REMOVENORAGDOLL ) );
							}
						}
					}
				}
				TaskComplete();
			}
			break;
		}
	case TASK_GREY_FADE_AWAY:
		{
			if(!m_bAmIFading)
			{
				m_bAmIFading = true;
				m_nSkin = 1;
				// remove the hand effects
				RemoveEffects();
				TaskComplete();
			}
		}
	default:
		BaseClass::RunTask( pTask );
		break;
	}
}


//---------------------------------------------------------
//---------------------------------------------------------
int CGrey::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	if(m_bAllowDamage)
		return BaseClass::OnTakeDamage_Alive( inputInfo );
	else
		return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrey::CreateEffects( void )
{
	// Start up the eye glow
	m_pMainGlow = CSprite::SpriteCreate( "sprites/greenglow1.vmt", GetLocalOrigin(), false );
	m_pMainGlow2 = CSprite::SpriteCreate( "sprites/greenglow1.vmt", GetLocalOrigin(), false );

	int	nAttachment = LookupAttachment( "lefthand" );
	int	nAttachment2 = LookupAttachment( "righthand" );

	if ( m_pMainGlow != NULL )
	{
		m_pMainGlow->FollowEntity( this );
		m_pMainGlow->SetAttachment( this, nAttachment );
		m_pMainGlow->SetTransparency( kRenderGlow, 255, 255, 255, 200, kRenderFxNoDissipation );
		m_pMainGlow->SetScale( 0.6f ); //.2
		m_pMainGlow->SetGlowProxySize( 4.0f );
	}
	if ( m_pMainGlow2 != NULL )
	{
		m_pMainGlow2->FollowEntity( this );
		m_pMainGlow2->SetAttachment( this, nAttachment2 );
		m_pMainGlow2->SetTransparency( kRenderGlow, 255, 255, 255, 200, kRenderFxNoDissipation );
		m_pMainGlow2->SetScale( 0.6f );
		m_pMainGlow2->SetGlowProxySize( 4.0f );
	}

	// Start up the eye trail
	m_pGlowTrail	= CSpriteTrail::SpriteTrailCreate( "sprites/bluelaser1.vmt", GetLocalOrigin(), false );
	m_pGlowTrail2	= CSpriteTrail::SpriteTrailCreate( "sprites/bluelaser1.vmt", GetLocalOrigin(), false );

	if ( m_pGlowTrail != NULL )
	{
		m_pGlowTrail->FollowEntity( this );
		m_pGlowTrail->SetAttachment( this, nAttachment );
		m_pGlowTrail->SetTransparency( kRenderTransAdd, 255, 0, 0, 255, kRenderFxNone );
		m_pGlowTrail->SetStartWidth( 8.0f );
		m_pGlowTrail->SetEndWidth( 1.0f );
		m_pGlowTrail->SetLifeTime( 0.5f );
	}
		if ( m_pGlowTrail2 != NULL )
	{
		m_pGlowTrail2->FollowEntity( this );
		m_pGlowTrail2->SetAttachment( this, nAttachment2 );
		m_pGlowTrail2->SetTransparency( kRenderTransAdd, 255, 0, 0, 255, kRenderFxNone );
		m_pGlowTrail2->SetStartWidth( 8.0f );
		m_pGlowTrail2->SetEndWidth( 1.0f );
		m_pGlowTrail2->SetLifeTime( 0.5f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrey::RemoveEffects( void )
{
	if ( m_pMainGlow != NULL )
	{
		m_pMainGlow->SetTransparency( kRenderTransAdd, 0, 0, 0, 0, kRenderFxNone );
		m_pMainGlow = NULL;
	}
	if ( m_pMainGlow2 != NULL )
	{
		m_pMainGlow2->SetTransparency( kRenderTransAdd, 0, 0, 0, 0, kRenderFxNone );
		m_pMainGlow2 = NULL;
	}
	if ( m_pGlowTrail != NULL )
	{
		m_pGlowTrail->SetTransparency( kRenderTransAdd, 0, 0, 0, 0, kRenderFxNone );
		m_pGlowTrail = NULL;
	}
	if ( m_pGlowTrail2 != NULL )
	{
		m_pGlowTrail2->SetTransparency( kRenderTransAdd, 0, 0, 0, 0, kRenderFxNone );
		m_pGlowTrail2 = NULL;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

bool CGrey::OnObstructingDoor( AILocalMoveGoal_t *pMoveGoal, CBaseDoor *pDoor, 
							  float distClear, AIMoveResult_t *pResult )
{
	return false;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CGrey::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	if( !IsCurSchedule( SCHED_FLINCH_PHYSICS ) && !m_ActBusyBehavior.IsActive() )
	{
		SetCustomInterruptCondition( COND_PHYSICS_DAMAGE );
	}
}

	
//=============================================================================

AI_BEGIN_CUSTOM_NPC( npc_grey, CGrey )

	DECLARE_CONDITION( COND_GREY_CHARGE_TARGET_MOVED )

	DECLARE_TASK( TASK_GREY_EXPRESS_ANGER )
	DECLARE_TASK( TASK_GREY_CHARGE_ENEMY )
	DECLARE_TASK( TASK_GREY_FADE_AWAY )
	
	DECLARE_ACTIVITY( ACT_GREY_ALARMED );

	DECLARE_ANIMEVENT( AE_GREY_HANDS_EFFECT ); // event in animation that does the attack effects


	DEFINE_SCHEDULE
	(
		SCHED_GREY_WANDER_ANGRILY,

		"	Tasks"
		"		TASK_WANDER						480240" // 48 units to 240 units.
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			4"
		""
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
	)

	DEFINE_SCHEDULE
	(
		SCHED_GREY_CHARGE_ENEMY,


		"	Tasks"
		"		TASK_GREY_CHARGE_ENEMY		0"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_GREY_ALARMED" /* placeholder until frustration/rage/fence shake animation available */
		""
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_GREY_CHARGE_TARGET_MOVED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_GREY_FAIL,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_GREY_ALARMED"
		"		TASK_WAIT				1"
		"		TASK_WAIT_PVS			0"
		""
		"	Interrupts"
		"		COND_CAN_RANGE_ATTACK1 "
		"		COND_CAN_MELEE_ATTACK1 "
		"		COND_GIVE_WAY"
	)

	DEFINE_SCHEDULE
	(
		SCHED_GREY_RANGE_ATTACK1,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_IDEAL							0"
		"		TASK_PLAY_PRIVATE_SEQUENCE_FACE_ENEMY	ACTIVITY:ACT_GREY_ALARMED"
		"		TASK_FACE_IDEAL							0"
		"		TASK_RANGE_ATTACK1		0"
		"		TASK_GREY_FADE_AWAY		0"
		"		TASK_PLAY_PRIVATE_SEQUENCE_FACE_ENEMY	ACTIVITY:ACT_FLY"
		""
		"	Interrupts"
	)

AI_END_CUSTOM_NPC()

//=============================================================================
