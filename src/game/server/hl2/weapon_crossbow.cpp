//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "basehlcombatweapon_shared.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "IEffects.h"
#include "te_effect_dispatch.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "beam_shared.h"
#include "rumble_shared.h"
#include "gamestats.h"
#include "decals.h"

#ifdef PORTAL
	#include "portal_util_shared.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//#define BOLT_MODEL			"models/crossbow_bolt.mdl"
#define BOLT_MODEL	"models/weapons/w_missile_closed.mdl"

#define BOLT_AIR_VELOCITY	2500
#define BOLT_WATER_VELOCITY	1500

extern ConVar sk_plr_dmg_crossbow;
extern ConVar sk_npc_dmg_crossbow;

void TE_StickyBolt( IRecipientFilter& filter, float delay,	Vector vecDirection, const Vector *origin );

#define	BOLT_SKIN_NORMAL	0
#define BOLT_SKIN_GLOW		1

#if defined ( ICEMOD_DLL )
// ICE for secondary attack
//
#define BLUDGEON_HULL_DIM		16
static const Vector g_bludgeonMins(-BLUDGEON_HULL_DIM,-BLUDGEON_HULL_DIM,-BLUDGEON_HULL_DIM);
static const Vector g_bludgeonMaxs(BLUDGEON_HULL_DIM,BLUDGEON_HULL_DIM,BLUDGEON_HULL_DIM);
//
#endif // defined ( ICEMOD_DLL )

//-----------------------------------------------------------------------------
// Crossbow Bolt
//-----------------------------------------------------------------------------
class CCrossbowBolt : public CBaseCombatCharacter
{
	DECLARE_CLASS( CCrossbowBolt, CBaseCombatCharacter );

public:
	CCrossbowBolt() { };
	~CCrossbowBolt();

	Class_T Classify( void ) { return CLASS_NONE; }

public:
	void Spawn( void );
	void Precache( void );
	void BubbleThink( void );
	void BoltTouch( CBaseEntity *pOther );
	bool CreateVPhysics( void );
	unsigned int PhysicsSolidMaskForEntity() const;
	static CCrossbowBolt *BoltCreate( const Vector &vecOrigin, const QAngle &angAngles, CBasePlayer *pentOwner = NULL );

protected:

	bool	CreateSprites( void );

	CHandle<CSprite>		m_pGlowSprite;
	//CHandle<CSpriteTrail>	m_pGlowTrail;

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
};
LINK_ENTITY_TO_CLASS( crossbow_bolt, CCrossbowBolt );

BEGIN_DATADESC( CCrossbowBolt )
	// Function Pointers
	DEFINE_FUNCTION( BubbleThink ),
	DEFINE_FUNCTION( BoltTouch ),

	// These are recreated on reload, they don't need storage
	DEFINE_FIELD( m_pGlowSprite, FIELD_EHANDLE ),
	//DEFINE_FIELD( m_pGlowTrail, FIELD_EHANDLE ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CCrossbowBolt, DT_CrossbowBolt )
END_SEND_TABLE()

CCrossbowBolt *CCrossbowBolt::BoltCreate( const Vector &vecOrigin, const QAngle &angAngles, CBasePlayer *pentOwner )
{
	// Create a new entity with CCrossbowBolt private data
	CCrossbowBolt *pBolt = (CCrossbowBolt *)CreateEntityByName( "crossbow_bolt" );
	UTIL_SetOrigin( pBolt, vecOrigin );
	pBolt->SetAbsAngles( angAngles );
	pBolt->Spawn();
	pBolt->SetOwnerEntity( pentOwner );

	return pBolt;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCrossbowBolt::~CCrossbowBolt( void )
{
	if ( m_pGlowSprite )
	{
		UTIL_Remove( m_pGlowSprite );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CCrossbowBolt::CreateVPhysics( void )
{
	// Create the object in the physics system
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
unsigned int CCrossbowBolt::PhysicsSolidMaskForEntity() const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CCrossbowBolt::CreateSprites( void )
{
	// Start up the eye glow
	m_pGlowSprite = CSprite::SpriteCreate( "sprites/light_glow02_noz.vmt", GetLocalOrigin(), false );

	if ( m_pGlowSprite != NULL )
	{
		m_pGlowSprite->FollowEntity( this );
		m_pGlowSprite->SetTransparency( kRenderGlow, 255, 255, 255, 128, kRenderFxNoDissipation );
		m_pGlowSprite->SetScale( 0.2f );
		m_pGlowSprite->TurnOff();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCrossbowBolt::Spawn( void )
{
	Precache( );

	SetModel( "models/crossbow_bolt.mdl" );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	UTIL_SetSize( this, -Vector(0.3f,0.3f,0.3f), Vector(0.3f,0.3f,0.3f) );
	SetSolid( SOLID_BBOX );
#if defined ( ICEMOD_DLL )
	SetGravity( 1.5f );
#else
	SetGravity( 0.05f );
#endif // defined ( ICEMOD_DLL )
	
	// Make sure we're updated if we're underwater
	UpdateWaterState();

	SetTouch( &CCrossbowBolt::BoltTouch );

	SetThink( &CCrossbowBolt::BubbleThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
	
#if !defined ( ICEMOD_DLL )
	CreateSprites();

	// Make us glow until we've hit the wall
	m_nSkin = BOLT_SKIN_GLOW;
#endif // !defined ( ICEMOD_DLL )
}


void CCrossbowBolt::Precache( void )
{
	PrecacheModel( BOLT_MODEL );

	// This is used by C_TEStickyBolt, despte being different from above!!!
	PrecacheModel( "models/crossbow_bolt.mdl" );

#if !defined ( ICEMOD_DLL )
	PrecacheModel( "sprites/light_glow02_noz.vmt" );
#endif // !defined ( ICEMOD_DLL )
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCrossbowBolt::BoltTouch( CBaseEntity *pOther )
{
	if ( pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER) )
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ( ( pOther->m_takedamage == DAMAGE_NO ) || ( pOther->m_takedamage == DAMAGE_EVENTS_ONLY ) )
			return;
	}

	if ( pOther->m_takedamage != DAMAGE_NO )
	{
		trace_t	tr, tr2;
		tr = BaseClass::GetTouchTrace();
		Vector	vecNormalizedVel = GetAbsVelocity();

		ClearMultiDamage();
		VectorNormalize( vecNormalizedVel );

#if defined(HL2_EPISODIC)
		//!!!HACKHACK - specific hack for ep2_outland_10 to allow crossbow bolts to pass through her bounding box when she's crouched in front of the player
		// (the player thinks they have clear line of sight because Alyx is crouching, but her BBOx is still full-height and blocks crossbow bolts.
		if( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() && pOther->Classify() == CLASS_PLAYER_ALLY_VITAL && FStrEq(STRING(gpGlobals->mapname), "ep2_outland_10") )
		{
			// Change the owner to stop further collisions with Alyx. We do this by making her the owner.
			// The player won't get credit for this kill but at least the bolt won't magically disappear!
			SetOwnerEntity( pOther );
			return;
		}
#endif//HL2_EPISODIC

		if( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() && pOther->IsNPC() )
		{
			CTakeDamageInfo	dmgInfo( this, GetOwnerEntity(), sk_plr_dmg_crossbow.GetFloat(), DMG_NEVERGIB );
			dmgInfo.AdjustPlayerDamageInflictedForSkillLevel();
			CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, tr.endpos, 0.7f );
			dmgInfo.SetDamagePosition( tr.endpos );
			pOther->DispatchTraceAttack( dmgInfo, vecNormalizedVel, &tr );

			CBasePlayer *pPlayer = ToBasePlayer( GetOwnerEntity() );
			if ( pPlayer )
			{
				gamestats->Event_WeaponHit( pPlayer, true, "weapon_crossbow", dmgInfo );
			}
			
#if defined ( ICEMOD_DLL )
			// I was testing rope attached to the harpoon
			/*
			if ( !pOther->IsAlive() ) // only apply test if the thing is dead
			{
				// TEST CODE
				IPhysicsObject *pObject = pOther->VPhysicsGetObject();
				Vector vecPosition, force;
				GetAttachment( -1, vecPosition, &force );
				force *= pObject->GetMass() * 90.0f;
				pObject->ApplyForceOffset( -force, vec3_origin );
			}*/
#endif // defined ( ICEMOD_DLL )

		}
		else
		{
			CTakeDamageInfo	dmgInfo( this, GetOwnerEntity(), sk_plr_dmg_crossbow.GetFloat(), DMG_BULLET | DMG_NEVERGIB );
			CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, tr.endpos, 0.7f );
			dmgInfo.SetDamagePosition( tr.endpos );
			pOther->DispatchTraceAttack( dmgInfo, vecNormalizedVel, &tr );
			
#if defined ( ICEMOD_DLL )
			// Test code
			// I was testing rope attached to the harpoon
			/*
			IPhysicsObject *pObject = pOther->VPhysicsGetObject();
			Vector vecPosition, force;
			GetAttachment( -1, vecPosition, &force );
			force *= pObject->GetMass() * 90.0f;
			pObject->ApplyForceOffset( -force, vec3_origin );
			*/
#endif // defined ( ICEMOD_DLL )
		}

		ApplyMultiDamage();

		//Adrian: keep going through the glass.
		if ( pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS )
			 return;

		if ( !pOther->IsAlive() )
		{
			// We killed it! 
			const surfacedata_t *pdata = physprops->GetSurfaceData( tr.surface.surfaceProps );
			if ( pdata->game.material == CHAR_TEX_GLASS )
			{
				return;
			}
		}

		SetAbsVelocity( Vector( 0, 0, 0 ) );

		// play body "thwack" sound
		EmitSound( "Weapon_Crossbow.BoltHitBody" );

		Vector vForward;

		AngleVectors( GetAbsAngles(), &vForward );
		VectorNormalize ( vForward );

		UTIL_TraceLine( GetAbsOrigin(),	GetAbsOrigin() + vForward * 128, MASK_BLOCKLOS, pOther, COLLISION_GROUP_NONE, &tr2 );

		if ( tr2.fraction != 1.0f )
		{
//			NDebugOverlay::Box( tr2.endpos, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 255, 0, 0, 10 );
//			NDebugOverlay::Box( GetAbsOrigin(), Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 0, 255, 0, 10 );

			if ( tr2.m_pEnt == NULL || ( tr2.m_pEnt && tr2.m_pEnt->GetMoveType() == MOVETYPE_NONE ) )
			{
				CEffectData	data;

				data.m_vOrigin = tr2.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = tr2.fraction != 1.0f;
			
				DispatchEffect( "BoltImpact", data );
			}
		}
		
		SetTouch( NULL );
		SetThink( NULL );

		if ( !g_pGameRules->IsMultiplayer() )
		{
			UTIL_Remove( this );
		}
	}
	else
	{
		trace_t	tr;
		tr = BaseClass::GetTouchTrace();

		// See if we struck the world
		if ( pOther->GetMoveType() == MOVETYPE_NONE && !( tr.surface.flags & SURF_SKY ) )
		{
			EmitSound( "Weapon_Crossbow.BoltHitWorld" );

#if !defined ( ICEMOD_DLL )
			// if what we hit is static architecture, can stay around for a while.
			Vector vecDir = GetAbsVelocity();
			float speed = VectorNormalize( vecDir );

			// See if we should reflect off this surface
			float hitDot = DotProduct( tr.plane.normal, -vecDir );
			
			if ( ( hitDot < 0.5f ) && ( speed > 100 ) )
			{
				Vector vReflection = 2.0f * tr.plane.normal * hitDot + vecDir;
				
				QAngle reflectAngles;

				VectorAngles( vReflection, reflectAngles );

				SetLocalAngles( reflectAngles );

				SetAbsVelocity( vReflection * speed * 0.75f );

				// Start to sink faster
				SetGravity( 1.0f );
			}
			else
			{
#endif // !defined ( ICEMOD_DLL )
				SetThink( &CCrossbowBolt::SUB_Remove );
				SetNextThink( gpGlobals->curtime + 2.0f );
				
				//FIXME: We actually want to stick (with hierarchy) to what we've hit
				SetMoveType( MOVETYPE_NONE );
			
				Vector vForward;

				AngleVectors( GetAbsAngles(), &vForward );
				VectorNormalize ( vForward );

				CEffectData	data;

				data.m_vOrigin = tr.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = 0;
			
				DispatchEffect( "BoltImpact", data );
				
				UTIL_ImpactTrace( &tr, DMG_BULLET );

				AddEffects( EF_NODRAW );
				SetTouch( NULL );
				SetThink( &CCrossbowBolt::SUB_Remove );
				SetNextThink( gpGlobals->curtime + 2.0f );

#if !defined ( ICEMOD_DLL )
				if ( m_pGlowSprite != NULL )
				{
					m_pGlowSprite->TurnOn();
					m_pGlowSprite->FadeAndDie( 3.0f );
				}
#endif // !defined ( ICEMOD_DLL )
#if !defined ( ICEMOD_DLL )
			}
#endif // !defined ( ICEMOD_DLL )
			
			// Shoot some sparks
			if ( UTIL_PointContents( GetAbsOrigin() ) != CONTENTS_WATER)
			{
				g_pEffects->Sparks( GetAbsOrigin() );
			}
		}
		else
		{
			// Put a mark unless we've hit the sky
			if ( ( tr.surface.flags & SURF_SKY ) == false )
			{
				UTIL_ImpactTrace( &tr, DMG_BULLET );
			}

			UTIL_Remove( this );
		}
	}

	if ( g_pGameRules->IsMultiplayer() )
	{
//		SetThink( &CCrossbowBolt::ExplodeThink );
//		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCrossbowBolt::BubbleThink( void )
{
	QAngle angNewAngles;

	VectorAngles( GetAbsVelocity(), angNewAngles );
	SetAbsAngles( angNewAngles );

	SetNextThink( gpGlobals->curtime + 0.1f );

	// Make danger sounds out in front of me, to scare snipers back into their hole
	CSoundEnt::InsertSound( SOUND_DANGER_SNIPERONLY, GetAbsOrigin() + GetAbsVelocity() * 0.2, 120.0f, 0.5f, this, SOUNDENT_CHANNEL_REPEATED_DANGER );

	if ( GetWaterLevel()  == 0 )
		return;

	UTIL_BubbleTrail( GetAbsOrigin() - GetAbsVelocity() * 0.1f, GetAbsOrigin(), 5 );
}


//-----------------------------------------------------------------------------
// CWeaponCrossbow
//-----------------------------------------------------------------------------

class CWeaponCrossbow : public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CWeaponCrossbow, CBaseHLCombatWeapon );
public:

	CWeaponCrossbow( void );
	
	virtual void	Precache( void );
	virtual void	PrimaryAttack( void );
	virtual void	SecondaryAttack( void );
	virtual bool	Deploy( void );
	virtual void	Drop( const Vector &vecVelocity );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual bool	Reload( void );
	virtual void	ItemPostFrame( void );
	virtual void	ItemBusyFrame( void );
	virtual void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	virtual bool	SendWeaponAnim( int iActivity );
	virtual bool	IsWeaponZoomed() { return m_bInZoom; }
	
	bool	ShouldDisplayHUDHint() { return true; }
	
#if defined ( ICEMOD_DLL )
	// TEST CODE
	//EHANDLE m_hRope;

	// ICE required for melee secondary
	
	// virtual float	GetRange( void )								{	return	32.0f;	}
	virtual float	GetRange( void )								{	return	64.0f;	}
	virtual	float	GetDamageForActivity( Activity hitActivity )	{	return	26.0f;	}

	virtual int		CapabilitiesGet( void );
	virtual	int		WeaponMeleeAttack1Condition( float flDot, float flDist );

	virtual	void	ImpactEffect( trace_t &trace );

	bool			ImpactWater( const Vector &start, const Vector &end );
	void			Swing( int bIsSecondary );
	void			Hit( trace_t &traceHit, Activity nHitActivity, bool bIsSecondary );
	Activity		ChooseIntersectionPointAndActivity( trace_t &hitTrace, const Vector &mins, const Vector &maxs, CBasePlayer *pOwner );

	//Functions to select animation sequences 
	virtual Activity	GetSecondaryAttackActivity( void )	{	return	ACT_VM_HITCENTER;	}
	//

	//end ICE
#endif // defined ( ICEMOD_DLL )


	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
	
	void	StopEffects( void );
	void	SetSkin( int skinNum );
#if !defined ( ICEMOD_DLL )
	void	CheckZoomToggle( void );
#endif // !defined ( ICEMOD_DLL )
	void	FireBolt( void );
#if !defined ( ICEMOD_DLL )
	void	ToggleZoom( void );
#endif // !defined ( ICEMOD_DLL )
	
	// Various states for the crossbow's charger
	enum ChargerState_t
	{
		CHARGER_STATE_START_LOAD,
		CHARGER_STATE_START_CHARGE,
		CHARGER_STATE_READY,
		CHARGER_STATE_DISCHARGE,
		CHARGER_STATE_OFF,
	};

	void	CreateChargerEffects( void );
	void	SetChargerState( ChargerState_t state );
	void	DoLoadEffect( void );

private:
	
	// Charger effects
	ChargerState_t		m_nChargeState;
	CHandle<CSprite>	m_hChargerSprite;

	bool				m_bInZoom;
	bool				m_bMustReload;
};

LINK_ENTITY_TO_CLASS( weapon_crossbow, CWeaponCrossbow );

PRECACHE_WEAPON_REGISTER( weapon_crossbow );

IMPLEMENT_SERVERCLASS_ST( CWeaponCrossbow, DT_WeaponCrossbow )
END_SEND_TABLE()

BEGIN_DATADESC( CWeaponCrossbow )

	DEFINE_FIELD( m_bInZoom,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMustReload,	FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nChargeState,	FIELD_INTEGER ),
	DEFINE_FIELD( m_hChargerSprite,	FIELD_EHANDLE ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponCrossbow::CWeaponCrossbow( void )
{
	m_bReloadsSingly	= true;
	m_bFiresUnderwater	= true;
	m_bAltFiresUnderwater = true;
	m_bInZoom			= false;
	m_bMustReload		= false;
	
#if defined ( ICEMOD_DLL )
	// ICE this would normally go into precache, but yeh (might need this)
	/*

	m_fMinRange1	= 0;
	m_fMinRange2	= 0;
	m_fMaxRange1	= 64;
	m_fMaxRange2	= 64;

	*/
#endif // defined ( ICEMOD_DLL )
}

#define	CROSSBOW_GLOW_SPRITE	"sprites/light_glow02_noz.vmt"
#define	CROSSBOW_GLOW_SPRITE2	"sprites/blueflare1.vmt"

#if defined ( ICEMOD_DLL )
// ICE additon for secondary Part 1
//

int CWeaponCrossbow::CapabilitiesGet()
{ 
	return bits_CAP_WEAPON_MELEE_ATTACK1; 
}


int CWeaponCrossbow::WeaponMeleeAttack1Condition( float flDot, float flDist )
{
	//if (flDist > 64)
	if (flDist > 90)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.7)
	{
		return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_MELEE_ATTACK1;
}

//
#endif // defined ( ICEMOD_DLL )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::Precache( void )
{
	UTIL_PrecacheOther( "crossbow_bolt" );

	PrecacheScriptSound( "Weapon_Crossbow.BoltHitBody" );
	PrecacheScriptSound( "Weapon_Crossbow.BoltHitWorld" );
	PrecacheScriptSound( "Weapon_Crossbow.BoltSkewer" );

	PrecacheModel( CROSSBOW_GLOW_SPRITE );
	PrecacheModel( CROSSBOW_GLOW_SPRITE2 );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponCrossbow::PrimaryAttack( void )
{
	if ( m_bInZoom && g_pGameRules->IsMultiplayer() )
	{
//		FireSniperBolt();
		FireBolt();
	}
	else
	{
		FireBolt();
	}

	// Signal a reload
	m_bMustReload = true;

	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration( ACT_VM_PRIMARYATTACK ) );

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer )
	{
		m_iPrimaryAttacks++;
		gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponCrossbow::SecondaryAttack( void )
{
	//NOTENOTE: The zooming is handled by the post/busy frames
	
#if defined ( ICEMOD_DLL )
	// ICE secondary
	//

	Swing( false );

	//
#endif // defined ( ICEMOD_DLL )
}

#if defined ( ICEMOD_DLL )
// ICE for secondary part 2
//

void CWeaponCrossbow::Hit( trace_t &traceHit, Activity nHitActivity, bool bIsSecondary )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	//Do view kick
	AddViewKick();

	//Make sound for the AI
	CSoundEnt::InsertSound( SOUND_BULLET_IMPACT, traceHit.endpos, 400, 0.2f, pPlayer );

	// This isn't great, but it's something for when the crowbar hits.
	pPlayer->RumbleEffect( RUMBLE_AR2, 0, RUMBLE_FLAG_RESTART );

	CBaseEntity	*pHitEntity = traceHit.m_pEnt;

	//Apply damage to a hit target
	if ( pHitEntity != NULL )
	{
		Vector hitDirection;
		pPlayer->EyeVectors( &hitDirection, NULL, NULL );
		VectorNormalize( hitDirection );

		CTakeDamageInfo info( GetOwner(), GetOwner(), GetDamageForActivity( nHitActivity ), DMG_SLASH );

		if( pPlayer && pHitEntity->IsNPC() )
		{
			// If bonking an NPC, adjust damage.
			info.AdjustPlayerDamageInflictedForSkillLevel();

			/* ICE Try this to sticky the NPC
			if ( !pHitEntity->IsEFlagSet( EFL_NO_MEGAPHYSCANNON_RAGDOLL ) )
			{
				Assert( pObject->MyNPCPointer()->CanBecomeRagdoll() );
				CTakeDamageInfo info( GetOwner(), GetOwner(), 1.0f, DMG_GENERIC );
				// try this next

				//get the enemy
				CBaseAnimating *pAnimating = dynamic_cast<CBaseAnimating*>( GetEnemy() );

				// Get the current bone matrix
				//matrix3x4_t pBoneToWorld[MAXSTUDIOBONES];
				//pAnimating->SetupBones( pBoneToWorld, BONE_USED_BY_ANYTHING );

				//look up attach enemy bone
				int m_iGrabbedBoneIndex = pAnimating->LookupBone( "ValveBiped.Bip01_Head1" );

				// get the weapon so we can find the bone to attach to, on the weapon
				CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
				int m_hTongueTip = pAnimating->LookupBone( "Ammo" );

				//get physics object of the npc
				//IPhysicsObject *pPhysics = pHitEntity->VPhysicsGetObject();
				IPhysicsObject *pPhysics = pWeapon->VPhysicsGetObject();

				//CreateServerRagdollAttached( CBaseAnimating *pAnimating, const Vector &vecForce, int forceBone, int collisionGroup, IPhysicsObject *pAttached, CBaseAnimating *pParentEntity, int boneAttach, const Vector &originAttached, int parentBoneAttach, const Vector &boneOrigin )
				//CRagdollProp *pRagdoll = CreateServerRagdollAttached( GetBaseAnimating(), vec3_origin, -1, COLLISION_GROUP_NONE, pPhysics, pWeapon, 0, vec3_origin, m_iGrabbedBoneIndex, vec3_origin );
				CRagdollProp *pRagdoll = CreateServerRagdollAttached( GetBaseAnimating(), vec3_origin, -1, COLLISION_GROUP_NONE, pPhysics, pWeapon, 0, vec3_origin, m_iGrabbedBoneIndex, vec3_origin );
				
				//dont collide with this ragdoll
				PhysSetEntityGameFlags( pRagdoll, FVPHYSICS_NO_SELF_COLLISIONS );
				pRagdoll->SetCollisionBounds( pHitEntity->CollisionProp()->OBBMins(), pHitEntity->CollisionProp()->OBBMaxs() );

				// Necessary to cause it to do the appropriate death cleanup
				CTakeDamageInfo ragdollInfo( GetOwner(), GetOwner(), 10000.0, DMG_PHYSGUN | DMG_REMOVENORAGDOLL );
				pHitEntity->TakeDamage( ragdollInfo );

				// Now we act on the ragdoll for the remainder of the time
				pHitEntity = pRagdoll;
				//bKilledByGrab = true;

				// then you might need to call detach ragdoll
			}
			*/

		}

		CalculateMeleeDamageForce( &info, hitDirection, traceHit.endpos );

		pHitEntity->DispatchTraceAttack( info, hitDirection, &traceHit ); 
		ApplyMultiDamage();

		// Now hit all triggers along the ray that... 
		TraceAttackToTriggers( info, traceHit.startpos, traceHit.endpos, hitDirection );

		if ( ToBaseCombatCharacter( pHitEntity ) )
		{
			gamestats->Event_WeaponHit( pPlayer, !bIsSecondary, GetClassname(), info );
		}
	}

	// Apply an impact effect
	ImpactEffect( traceHit );
}

Activity CWeaponCrossbow::ChooseIntersectionPointAndActivity( trace_t &hitTrace, const Vector &mins, const Vector &maxs, CBasePlayer *pOwner )
{
	int			i, j, k;
	float		distance;
	const float	*minmaxs[2] = {mins.Base(), maxs.Base()};
	trace_t		tmpTrace;
	Vector		vecHullEnd = hitTrace.endpos;
	Vector		vecEnd;

	distance = 1e6f;
	Vector vecSrc = hitTrace.startpos;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &tmpTrace );
	if ( tmpTrace.fraction == 1.0 )
	{
		for ( i = 0; i < 2; i++ )
		{
			for ( j = 0; j < 2; j++ )
			{
				for ( k = 0; k < 2; k++ )
				{
					vecEnd.x = vecHullEnd.x + minmaxs[i][0];
					vecEnd.y = vecHullEnd.y + minmaxs[j][1];
					vecEnd.z = vecHullEnd.z + minmaxs[k][2];

					UTIL_TraceLine( vecSrc, vecEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &tmpTrace );
					if ( tmpTrace.fraction < 1.0 )
					{
						float thisDistance = (tmpTrace.endpos - vecSrc).Length();
						if ( thisDistance < distance )
						{
							hitTrace = tmpTrace;
							distance = thisDistance;
						}
					}
				}
			}
		}
	}
	else
	{
		hitTrace = tmpTrace;
	}


	return ACT_VM_HITCENTER;
}

bool CWeaponCrossbow::ImpactWater( const Vector &start, const Vector &end )
{
	//FIXME: This doesn't handle the case of trying to splash while being underwater, but that's not going to look good
	//		 right now anyway...
	
	// We must start outside the water
	if ( UTIL_PointContents( start ) & (CONTENTS_WATER|CONTENTS_SLIME))
		return false;

	// We must end inside of water
	if ( !(UTIL_PointContents( end ) & (CONTENTS_WATER|CONTENTS_SLIME)))
		return false;

	trace_t	waterTrace;

	UTIL_TraceLine( start, end, (CONTENTS_WATER|CONTENTS_SLIME), GetOwner(), COLLISION_GROUP_NONE, &waterTrace );

	if ( waterTrace.fraction < 1.0f )
	{
		CEffectData	data;

		data.m_fFlags  = 0;
		data.m_vOrigin = waterTrace.endpos;
		data.m_vNormal = waterTrace.plane.normal;
		data.m_flScale = 8.0f;

		// See if we hit slime
		if ( waterTrace.contents & CONTENTS_SLIME )
		{
			data.m_fFlags |= FX_WATER_IN_SLIME;
		}

		DispatchEffect( "watersplash", data );			
	}

	return true;
}

void CWeaponCrossbow::ImpactEffect( trace_t &traceHit )
{
	// See if we hit water (we don't do the other impact effects in this case)
	if ( ImpactWater( traceHit.startpos, traceHit.endpos ) )
		return;

	//FIXME: need new decals
	UTIL_ImpactTrace( &traceHit, DMG_SLASH );
}

void CWeaponCrossbow::Swing( int bIsSecondary )
{
	trace_t traceHit;

	// Try a ray
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	pOwner->RumbleEffect( RUMBLE_CROWBAR_SWING, 0, RUMBLE_FLAG_RESTART );

	Vector swingStart = pOwner->Weapon_ShootPosition( );
	Vector forward;

	forward = pOwner->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT, GetRange() );

	Vector swingEnd = swingStart + forward * GetRange();
	UTIL_TraceLine( swingStart, swingEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &traceHit );
	Activity nHitActivity = ACT_VM_HITCENTER;

	// Like bullets, bludgeon traces have to trace against triggers.
	CTakeDamageInfo triggerInfo( GetOwner(), GetOwner(), GetDamageForActivity( nHitActivity ), DMG_SLASH );
	triggerInfo.SetDamagePosition( traceHit.startpos );
	triggerInfo.SetDamageForce( forward );
	TraceAttackToTriggers( triggerInfo, traceHit.startpos, traceHit.endpos, forward );

	if ( traceHit.fraction == 1.0 )
	{
		float bludgeonHullRadius = 1.732f * BLUDGEON_HULL_DIM;  // hull is +/- 16, so use cuberoot of 2 to determine how big the hull is from center to the corner point

		// Back off by hull "radius"
		swingEnd -= forward * bludgeonHullRadius;

		UTIL_TraceHull( swingStart, swingEnd, g_bludgeonMins, g_bludgeonMaxs, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &traceHit );
		if ( traceHit.fraction < 1.0 && traceHit.m_pEnt )
		{
			Vector vecToTarget = traceHit.m_pEnt->GetAbsOrigin() - swingStart;
			VectorNormalize( vecToTarget );

			float dot = vecToTarget.Dot( forward );

			// YWB:  Make sure they are sort of facing the guy at least...
			if ( dot < 0.70721f )
			{
				// Force amiss
				traceHit.fraction = 1.0f;
			}
			else
			{
				nHitActivity = ChooseIntersectionPointAndActivity( traceHit, g_bludgeonMins, g_bludgeonMaxs, pOwner );
			}
		}
	}

	// ICE this had primary, we are secondary here
	m_iSecondaryAttacks++;

	gamestats->Event_WeaponFired( pOwner, !bIsSecondary, GetClassname() );

	// -------------------------
	//	Miss
	// -------------------------
	if ( traceHit.fraction == 1.0f )
	{
		nHitActivity = bIsSecondary ? ACT_VM_MISSCENTER2 : ACT_VM_MISSCENTER;

		// We want to test the first swing again
		Vector testEnd = swingStart + forward * GetRange();
		
		// See if we happened to hit water
		ImpactWater( swingStart, testEnd );
	}
	else
	{
		Hit( traceHit, nHitActivity, bIsSecondary ? true : false );
	}

	// Send the anim
	SendWeaponAnim( nHitActivity );

	//Setup our next attack times
	//m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();

	//Play swing sound
	
	WeaponSound( MELEE_MISS );
	//WeaponSound( SINGLE );
}

#endif // defined ( ICEMOD_DLL )

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::Reload( void )
{
	if ( BaseClass::Reload() )
	{
		m_bMustReload = false;
		return true;
	}

	return false;
}

#if !defined ( ICEMOD_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::CheckZoomToggle( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	if ( pPlayer->m_afButtonPressed & IN_ATTACK2 )
	{
			ToggleZoom();
	}
}
#endif // !defined ( ICEMOD_DLL )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::ItemBusyFrame( void )
{
#if !defined ( ICEMOD_DLL )
	// Allow zoom toggling even when we're reloading
	CheckZoomToggle();
#endif // !defined ( ICEMOD_DLL )
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::ItemPostFrame( void )
{
#if !defined ( ICEMOD_DLL )
	// Allow zoom toggling
	CheckZoomToggle();
#endif // !defined ( ICEMOD_DLL )

#if defined ( ICEMOD_DLL )
	// ICE part 3 (might need this
	/*

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	if ( (pOwner->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
	{
		PrimaryAttack();
	} 
	else if ( (pOwner->m_nButtons & IN_ATTACK2) && (m_flNextSecondaryAttack <= gpGlobals->curtime) )
	{
		SecondaryAttack();
	}

	*/
#endif // defined ( ICEMOD_DLL )

	if ( m_bMustReload && HasWeaponIdleTimeElapsed() )
	{
		Reload();
	}

	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::FireBolt( void )
{
	if ( m_iClip1 <= 0 )
	{
		if ( !m_bFireOnEmpty )
		{
			Reload();
		}
		else
		{
			WeaponSound( EMPTY );
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	pOwner->RumbleEffect( RUMBLE_357, 0, RUMBLE_FLAG_RESTART );

	Vector vecAiming	= pOwner->GetAutoaimVector( 0 );
	Vector vecSrc		= pOwner->Weapon_ShootPosition();

	QAngle angAiming;
	VectorAngles( vecAiming, angAiming );

#if defined(HL2_EPISODIC)
	// !!!HACK - the other piece of the Alyx crossbow bolt hack for Outland_10 (see ::BoltTouch() for more detail)
	if( FStrEq(STRING(gpGlobals->mapname), "ep2_outland_10") )
	{
		trace_t tr;
		UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 24.0f, MASK_SOLID, pOwner, COLLISION_GROUP_NONE, &tr );

		if( tr.m_pEnt != NULL && tr.m_pEnt->Classify() == CLASS_PLAYER_ALLY_VITAL )
		{
			// If Alyx is right in front of the player, make sure the bolt starts outside of the player's BBOX, or the bolt
			// will instantly collide with the player after the owner of the bolt is switched to Alyx in ::BoltTouch(). We 
			// avoid this altogether by making it impossible for the bolt to collide with the player.
			vecSrc += vecAiming * 24.0f;
		}
	}
#endif

	CCrossbowBolt *pBolt = CCrossbowBolt::BoltCreate( vecSrc, angAiming, pOwner );
	
#if defined ( ICEMOD_DLL )
	// TEST CODE
	// I was testing rope attached to the harpoon
			/*
	// Decent enough default in case something happens to our owner!
	float flDist = 384;

	//if( GetOwnerEntity() )
	//{
		//flDist = fabs( GetOwnerEntity()->GetAbsOrigin().z - GetAbsOrigin().z );
	//}

	m_hRope = CRopeKeyframe::CreateWithSecondPointDetached( pBolt, -1, flDist, 1, "cable/cable.vmt", 5, true ); // ropewidth 1

	//m_hRope =  CRopeKeyframe::Create( pBolt, this, -1, 1, 1, "cable/cable.vmt", 10);


	ASSERT( m_hRope != NULL );
	// TEST CODE END
	*/
#endif // defined ( ICEMOD_DLL )

	if ( pOwner->GetWaterLevel() == 3 )
	{
		pBolt->SetAbsVelocity( vecAiming * BOLT_WATER_VELOCITY );
	}
	else
	{
		pBolt->SetAbsVelocity( vecAiming * BOLT_AIR_VELOCITY );
	}

	m_iClip1--;

	pOwner->ViewPunch( QAngle( -2, 0, 0 ) );

	WeaponSound( SINGLE );
	WeaponSound( SPECIAL2 );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 200, 0.2 );

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	if ( !m_iClip1 && pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		pOwner->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack	= gpGlobals->curtime + 0.75;

	DoLoadEffect();
	SetChargerState( CHARGER_STATE_DISCHARGE );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::Deploy( void )
{
	if ( m_iClip1 <= 0 )
	{
		return DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), ACT_CROSSBOW_DRAW_UNLOADED, (char*)GetAnimPrefix() );
	}

	SetSkin( BOLT_SKIN_GLOW );

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSwitchingTo - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#if defined ( ICEMOD_DLL )
	//UTIL_Remove( m_hRope );	// TEST CODE
#endif // defined ( ICEMOD_DLL )
	StopEffects();
	return BaseClass::Holster( pSwitchingTo );
}

#if !defined ( ICEMOD_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::ToggleZoom( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	if ( m_bInZoom )
	{
		if ( pPlayer->SetFOV( this, 0, 0.2f ) )
		{
			m_bInZoom = false;
		}
	}
	else
	{
		if ( pPlayer->SetFOV( this, 20, 0.1f ) )
		{
			m_bInZoom = true;
		}
	}
}
#endif // !defined ( ICEMOD_DLL )

#define	BOLT_TIP_ATTACHMENT	2

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::CreateChargerEffects( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( m_hChargerSprite != NULL )
		return;

	m_hChargerSprite = CSprite::SpriteCreate( CROSSBOW_GLOW_SPRITE, GetAbsOrigin(), false );

	if ( m_hChargerSprite )
	{
		m_hChargerSprite->SetAttachment( pOwner->GetViewModel(), BOLT_TIP_ATTACHMENT );
		m_hChargerSprite->SetTransparency( kRenderTransAdd, 255, 128, 0, 255, kRenderFxNoDissipation );
		m_hChargerSprite->SetBrightness( 0 );
		m_hChargerSprite->SetScale( 0.1f );
		m_hChargerSprite->TurnOff();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : skinNum - 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::SetSkin( int skinNum )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	CBaseViewModel *pViewModel = pOwner->GetViewModel();

	if ( pViewModel == NULL )
		return;

	pViewModel->m_nSkin = skinNum;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::DoLoadEffect( void )
{
	SetSkin( BOLT_SKIN_GLOW );

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	CBaseViewModel *pViewModel = pOwner->GetViewModel();

	if ( pViewModel == NULL )
		return;

	CEffectData	data;

	data.m_nEntIndex = pViewModel->entindex();
	data.m_nAttachmentIndex = 1;

	DispatchEffect( "CrossbowLoad", data );

	CSprite *pBlast = CSprite::SpriteCreate( CROSSBOW_GLOW_SPRITE2, GetAbsOrigin(), false );

	if ( pBlast )
	{
		pBlast->SetAttachment( pOwner->GetViewModel(), 1 );
		pBlast->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
		pBlast->SetBrightness( 128 );
		pBlast->SetScale( 0.2f );
		pBlast->FadeOutFromSpawn();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::SetChargerState( ChargerState_t state )
{
	// Make sure we're setup
	CreateChargerEffects();

	// Don't do this twice
	if ( state == m_nChargeState )
		return;

	m_nChargeState = state;

	switch( m_nChargeState )
	{
	case CHARGER_STATE_START_LOAD:
	
		WeaponSound( SPECIAL1 );
		
		// Shoot some sparks and draw a beam between the two outer points
		DoLoadEffect();
		
		break;

	case CHARGER_STATE_START_CHARGE:
		{
			if ( m_hChargerSprite == NULL )
				break;
			
			m_hChargerSprite->SetBrightness( 32, 0.5f );
			m_hChargerSprite->SetScale( 0.025f, 0.5f );
			m_hChargerSprite->TurnOn();
		}

		break;

	case CHARGER_STATE_READY:
		{
			// Get fully charged
			if ( m_hChargerSprite == NULL )
				break;
			
			m_hChargerSprite->SetBrightness( 80, 1.0f );
			m_hChargerSprite->SetScale( 0.1f, 0.5f );
			m_hChargerSprite->TurnOn();
		}

		break;

	case CHARGER_STATE_DISCHARGE:
		{
			SetSkin( BOLT_SKIN_NORMAL );
			
			if ( m_hChargerSprite == NULL )
				break;
			
			m_hChargerSprite->SetBrightness( 0 );
			m_hChargerSprite->TurnOff();
		}

		break;

	case CHARGER_STATE_OFF:
		{
			SetSkin( BOLT_SKIN_NORMAL );

			if ( m_hChargerSprite == NULL )
				break;
			
			m_hChargerSprite->SetBrightness( 0 );
			m_hChargerSprite->TurnOff();
		}
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
	case EVENT_WEAPON_THROW:
		SetChargerState( CHARGER_STATE_START_LOAD );
		break;

	case EVENT_WEAPON_THROW2:
		SetChargerState( CHARGER_STATE_START_CHARGE );
		break;
	
	case EVENT_WEAPON_THROW3:
		SetChargerState( CHARGER_STATE_READY );
		break;

	default:
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the desired activity for the weapon and its viewmodel counterpart
// Input  : iActivity - activity to play
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::SendWeaponAnim( int iActivity )
{
	int newActivity = iActivity;

	// The last shot needs a non-loaded activity
	if ( ( newActivity == ACT_VM_IDLE ) && ( m_iClip1 <= 0 ) )
	{
		newActivity = ACT_VM_FIDGET;
	}

	//For now, just set the ideal activity and be done with it
	return BaseClass::SendWeaponAnim( newActivity );
}

//-----------------------------------------------------------------------------
// Purpose: Stop all zooming and special effects on the viewmodel
//-----------------------------------------------------------------------------
void CWeaponCrossbow::StopEffects( void )
{
#if !defined ( ICEMOD_DLL )
	// Stop zooming
	if ( m_bInZoom )
	{
		ToggleZoom();
	}
#endif // !defined ( ICEMOD_DLL )

	// Turn off our sprites
	SetChargerState( CHARGER_STATE_OFF );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::Drop( const Vector &vecVelocity )
{
	StopEffects();
	BaseClass::Drop( vecVelocity );
}
