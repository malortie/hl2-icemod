//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_npc_dragon.h"
#include "proxyentity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "KeyValues.h"

// for Dlight
#include "dlight.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Save/restore
//-----------------------------------------------------------------------------
BEGIN_DATADESC( C_Npc_Dragon )
END_DATADESC()


IMPLEMENT_CLIENTCLASS_DT( C_Npc_Dragon, DT_Npc_Dragon, CNPC_Dragon )
	RecvPropFloat( RECVINFO( m_flSkinIllume ) ), 
	RecvPropBool( RECVINFO( m_bIsTorching ) ), 
	RecvPropInt( RECVINFO( m_iSkinColor ) ), 
END_RECV_TABLE()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_Npc_Dragon::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}


void C_Npc_Dragon::ClientThink()
{
	// update the dlight. (always done because clienthink only exists for cavernguard)
	if (!m_dlight)
	{
		m_dlight = effects->CL_AllocDlight( index );
		m_dlight->color.r = 240;
		m_dlight->color.g = 220;
		m_dlight->color.b = 48;
		m_dlight->radius  = 225;
		m_dlight->minlight = 128.0 / 256.0f; //128.0 / 256.0f;
		m_dlight->flags = DLIGHT_NO_MODEL_ILLUMINATION;
		m_dlight->die = FLT_MAX;

		m_cCurrentColor.r = 255;
		m_cCurrentColor.b = 255;
		m_cCurrentColor.g = 255;
		m_iTargetRadius = 225;

		m_cFlameColor.r = 255;
		m_cFlameColor.b = 92;
		m_cFlameColor.g = 195;
		/*
		very orange
		m_cFlameColor.r = 255;
		m_cFlameColor.b = 0;
		m_cFlameColor.g = 174;
		*/

		if( DragonSkinColor() == 0 )
		{
			m_cSkinColor.r = m_cCurrentColor.r = 255;
			m_cSkinColor.b = m_cCurrentColor.b = 0;
			m_cSkinColor.g = m_cCurrentColor.g = 255;
		}
		else if (DragonSkinColor() == 1 )
		{
			m_cSkinColor.r = m_cCurrentColor.r = 0;
			m_cSkinColor.b = m_cCurrentColor.b = 0;
			m_cSkinColor.g = m_cCurrentColor.g = 255;
		}
		else
		{
			m_cSkinColor.r = m_cCurrentColor.r = 255;
			m_cSkinColor.b = m_cCurrentColor.b = 0;
			m_cSkinColor.g = m_cCurrentColor.g = 0;
		}
	}

	// update dlight origin first to make my crappy code more readable
	Vector newOrigin;
	if(!IsTorching())
	{
		newOrigin = GetAbsOrigin();
		newOrigin.z += 64;
	}
	else
	{
		Vector	forward;
		AngleVectors( GetAbsAngles(), &forward );
		//forward.Negate();
		newOrigin = GetAbsOrigin() + (forward * 100);
	}
	m_dlight->origin	= newOrigin;

	// update dlight
	if(IsAlive())
	{
		if(IsTorching())
		{
			if(m_cCurrentColor != m_cFlameColor)
			{
				if(m_cCurrentColor.r != m_cFlameColor.r)
				{
					if(m_cCurrentColor.r > m_cFlameColor.r)
						m_cCurrentColor.r--;
					if(m_cCurrentColor.r < m_cFlameColor.r)
						m_cCurrentColor.r++;
				}
				if(m_cCurrentColor.b != m_cFlameColor.b)
				{
					if(m_cCurrentColor.b > m_cFlameColor.b)
						m_cCurrentColor.b--;
					if(m_cCurrentColor.b < m_cFlameColor.b)
						m_cCurrentColor.b++;
				}
				if(m_cCurrentColor.g != m_cFlameColor.g)
				{
					if(m_cCurrentColor.g > m_cFlameColor.g)
						m_cCurrentColor.g--;
					if(m_cCurrentColor.g < m_cFlameColor.g)
						m_cCurrentColor.g++;
				}
			}

			if( m_iTargetRadius < 225)
				m_iTargetRadius += .5f;

			m_dlight->radius  = (int)(m_iTargetRadius);
			m_dlight->color.r = m_cCurrentColor.r;
			m_dlight->color.b = m_cCurrentColor.b;
			m_dlight->color.g = m_cCurrentColor.g;

			int newMinlight = RemapVal( (sin( gpGlobals->curtime * SkinIllume() )), 0, 1, 48, 86 );
			m_dlight->minlight = newMinlight / 256.0f; //128.0 / 256.0f;
		}
		else
		{
			if(m_cCurrentColor != m_cSkinColor)
			{
				if(m_cCurrentColor.r != m_cSkinColor.r)
				{
					if(m_cCurrentColor.r > m_cSkinColor.r)
						m_cCurrentColor.r--;
					if(m_cCurrentColor.r < m_cSkinColor.r)
						m_cCurrentColor.r++;
				}
				if(m_cCurrentColor.b != m_cSkinColor.b)
				{
					if(m_cCurrentColor.b > m_cSkinColor.b)
						m_cCurrentColor.b--;
					if(m_cCurrentColor.b < m_cSkinColor.b)
						m_cCurrentColor.b++;
				}
				if(m_cCurrentColor.g != m_cSkinColor.g)
				{
					if(m_cCurrentColor.g > m_cSkinColor.g)
						m_cCurrentColor.g--;
					if(m_cCurrentColor.g < m_cSkinColor.g)
						m_cCurrentColor.g++;
				}
			}
			m_dlight->color.r = m_cCurrentColor.r;
			m_dlight->color.b = m_cCurrentColor.b;
			m_dlight->color.g = m_cCurrentColor.g;


			if( m_iTargetRadius > 64)
				m_iTargetRadius = 64;

			int newRadius = RemapVal( SkinIllume(), 0, 1, 64, 96 );
			if( m_iTargetRadius > newRadius)
			{
				m_iTargetRadius -= 1;
				m_dlight->radius  = m_iTargetRadius;
			}
			else
			{
				m_dlight->radius  = newRadius;
			}

			int newMinlight = RemapVal( SkinIllume(), 0, 1, 32, 64 );
			m_dlight->minlight = newMinlight / 256.0f; //128.0 / 256.0f;
		}
	}
	else
	{
		if(m_dlight->radius > 80)
		{
			int newRadius = m_dlight->radius;
			newRadius-=1;
			m_dlight->radius  = newRadius;
		}

		if(m_dlight->minlight > 0)
		{
			int newMinlight = m_dlight->minlight;
			newMinlight-=1;
			m_dlight->minlight  = newMinlight;
		}
	}
	// dl->die = gpGlobals->curtime + 0.1f;

	BaseClass::ClientThink();
}

//-----------------------------------------------------------------------------
// Sets the fade of the blades when the chopper starts up
//-----------------------------------------------------------------------------
class CDragonSkinMaterialProxy : public CEntityMaterialProxy
{
public:
	CDragonSkinMaterialProxy() { m_pSelfillumTint = NULL; }
	virtual ~CDragonSkinMaterialProxy() {}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( C_BaseEntity *pEntity );
	virtual IMaterial *GetMaterial();

private:

	IMaterialVar *m_pSelfillumTint;

};

bool CDragonSkinMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;

	m_pSelfillumTint = pMaterial->FindVar( "$selfillumtint", &foundVar, false );

	return foundVar;
}

void CDragonSkinMaterialProxy::OnBind( C_BaseEntity *pEnt )
{
	C_Npc_Dragon *pDragon = dynamic_cast<C_Npc_Dragon*>( pEnt );
	if ( pDragon )
	{
		float dt = pDragon->SkinIllume();
		dt = clamp( dt, 0.0f, 1.0f );
		m_pSelfillumTint->SetVecValue( dt, dt, dt );
	}
	else
	{
		m_pSelfillumTint->SetVecValue( 0, 0, 0 );
	}
}

IMaterial *CDragonSkinMaterialProxy::GetMaterial()
{
	if ( !m_pSelfillumTint )
		return NULL;

	return m_pSelfillumTint->GetOwningMaterial();
}

EXPOSE_INTERFACE( CDragonSkinMaterialProxy, IMaterialProxy, "dragon_sheet" IMATERIAL_PROXY_INTERFACE_VERSION );

