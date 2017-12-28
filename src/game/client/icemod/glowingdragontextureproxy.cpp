//=============================================================================//
//
// Purpose: Glowing Skin On Dragon Model
// 
// Special Thanks to Au-Heppa for this file as a base from witch to work off of
//=============================================================================//


#include "cbase.h"
#include <KeyValues.h>
#include "materialsystem/IMaterialVar.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/ITexture.h"
#include "materialsystem/IMaterialSystem.h"
#include "FunctionProxy.h"
#include "toolframework_client.h"
//#include "hlss_player.h"
//#include "hlss_monitor.h"
//#include "hlss_monitors_shared.h"
#include "view.h"
//#include "cl_hlss_alien_world.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ALIEN_LIGHT_WAVE_SPEED 25
#define ALIEN_LIGHT_WAVE_MIN .1
#define	ALIEN_LIGHT_WAVE_MAX .8

// forward declarations
void ToolFramework_RecordMaterialParams( IMaterial *pMaterial );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDragonGlow_Proxy : public IMaterialProxy
{
public:
	CDragonGlow_Proxy();

	bool			Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void			OnBind( void *pC_BaseEntity );
	IMaterial		*GetMaterial();
	virtual void	Release( void ) { delete this; }

private:
	IMaterialVar	*m_pColor;
	IMaterialVar	*m_pSelfillumTint;

	IMaterial		*m_pMaterial;

	float			m_fltimeCheck;

};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDragonGlow_Proxy::CDragonGlow_Proxy()
{
	m_pMaterial = NULL;
	m_pColor = NULL;
	m_pSelfillumTint = NULL;
	m_fltimeCheck = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDragonGlow_Proxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	m_pMaterial = pMaterial;

	bool bColor;
	m_pColor = pMaterial->FindVar( "$color2", &bColor );
	if (!bColor)
	{
		m_pColor = NULL;
		Warning("No $color2 found in material with DragonGlowProxy\n");
		return false;
	}

	if (pMaterial->GetMaterialVarFlag( MATERIAL_VAR_SELFILLUM ))
	{
		bool bFound;
		m_pSelfillumTint = pMaterial->FindVar("$selfillumtint", &bFound );
		
		if (!bFound)
		{
			m_pSelfillumTint = NULL;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDragonGlow_Proxy::OnBind( void *pRenderable )
{
	IMaterial *pMaterial = GetMaterial();
	if (pMaterial)
	{
		if (pRenderable)
		{
			IClientRenderable *pRend = ( IClientRenderable* )pRenderable;
			C_BaseEntity *pEnt = pRend->GetIClientUnknown()->GetBaseEntity();
			
			C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
			//CBasePlayer *pPlayer = UTIL_PlayerByIndex(1);
		
			if ( pEnt )
			{
				//pEnt->he
				if( pEnt->IsAlive() )
				{
					// fade the illume in and out constantly
					// base the effects speed on how close the player is?

					//const Vector &color = pEnt->GetLampColor();
					const color32 &color = pEnt->GetRenderColor();
					//const color32 &color = pEnt->Get

					if (m_pColor)
					{
						m_pColor->SetVecValue( color.r, color.b, color.g );
					}

					if (m_pSelfillumTint)
					{
						
						if( pPlayer )
						{
							int m_flEnemyDist = ((pPlayer->GetLocalOrigin() - pEnt->GetLocalOrigin()).Length());

							if( m_flEnemyDist <= 0)
								m_flEnemyDist = 1;
							if( m_flEnemyDist >= 100)
								m_flEnemyDist = 100;

							float flScale = RemapVal( sin( gpGlobals->curtime * m_flEnemyDist ), -1.0f, 1.0f, ALIEN_LIGHT_WAVE_MIN, ALIEN_LIGHT_WAVE_MAX );
							m_pSelfillumTint->SetVecValue( flScale * .05, flScale * .05, flScale * .05 );

							if(gpGlobals->curtime > m_fltimeCheck)
							{
								Msg("pPlayer & Alive - flScale: %d m_flEnemyDist: %d", flScale, m_flEnemyDist);
								m_fltimeCheck = gpGlobals->curtime + 2;
							}


						}
						else
						{
							float flScale = RemapVal( sin( gpGlobals->curtime * ALIEN_LIGHT_WAVE_SPEED ), -1.0f, 1.0f, ALIEN_LIGHT_WAVE_MIN, ALIEN_LIGHT_WAVE_MAX );
							m_pSelfillumTint->SetVecValue( flScale * .01, flScale * .01, flScale * .01 );

							if(gpGlobals->curtime > m_fltimeCheck)
							{
								Msg("no pPlayer & Alive - flScale: %d", flScale);
								m_fltimeCheck = gpGlobals->curtime + 1;
							}
						}
					}
				}
				else
				{
					// fade out, stay out.
					//float flScale
					if(gpGlobals->curtime > m_fltimeCheck)
					{
						Msg("not Alive");
						m_fltimeCheck = gpGlobals->curtime + 1;
					}

					m_pSelfillumTint->SetVecValue( .01, .01, .01 );
				}
			}
		}

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IMaterial *CDragonGlow_Proxy::GetMaterial()
{
	return m_pMaterial;
}

EXPOSE_INTERFACE( CDragonGlow_Proxy, IMaterialProxy, "dragon_sheet2" IMATERIAL_PROXY_INTERFACE_VERSION );