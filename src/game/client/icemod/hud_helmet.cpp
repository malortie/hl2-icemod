#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "c_basehlplayer.h" //alternative #include "c_baseplayer.h"
 
#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>
 
// memdbgon must be the last include file in a .cpp file!
#include "tier0/memdbgon.h"

#define MAXBLOODSPATTER 25
//
// ICE HELMET AND HELMET EFFECTS
//

class CHudHelmet : public vgui::Panel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudHelmet, vgui::Panel );
 
public:
	CHudHelmet( const char *pElementName );
 
	void Init();
	void MsgFunc_ShowHelmet( bf_read &msg );
	void Reset();

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *scheme);
	virtual void Paint( void );
	virtual void OnThink( void );
 
private:
	void UpdateHelmetElements( C_BasePlayer* pPlayer );

	bool m_bShow;

	struct m_sBloods{
		bool Alive;
		int Color;
		Vector Position;
		int Alpha;
		int Image;
	};
	m_sBloods		m_blBloods[MAXBLOODSPATTER];
	int				oldAmount;
	int				aliveSpatter;

	float			m_flOffsetX;
	float			m_flOffsetY;
	QAngle			m_aOldAngles;

	float			m_flBloodAlphaTime;

	bool			m_bClearHelmet;

    CHudTexture*	m_pHelmet;
	CHudTexture*	m_pHelmetBlast;

	CHudTexture*	m_pHelmetBSR1;
	CHudTexture*	m_pHelmetBSR2;
	CHudTexture*	m_pHelmetBSY1;
	CHudTexture*	m_pHelmetBSY2;
};
 
//DECLARE_HUDELEMENT( CHudHelmet );
DECLARE_HUDELEMENT_DEPTH( CHudHelmet, 60 ); // use "DECLARE_HUDELEMENT_DEPTH" to place helmet under the hud, 0 is below, 50 is default. I guessed 60.
DECLARE_HUD_MESSAGE( CHudHelmet, ShowHelmet );
 
using namespace vgui;
 
/**
 * Constructor - generic HUD element initialization stuff. Make sure our 2 member variables
 * are instantiated.
 */
CHudHelmet::CHudHelmet( const char *pElementName ) : CHudElement(pElementName), BaseClass(NULL, "HudHelmet")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_bShow = false;
	m_pHelmet = 0;
	m_pHelmetBlast = 0;
	m_pHelmetBSR1 = 0;
	m_pHelmetBSR2 = 0;
	m_pHelmetBSY1 = 0;
	m_pHelmetBSY2 = 0;

	m_flOffsetX = 0;
	m_flOffsetY = 0;
	
	// for future reference Jason - this is how you should initilize a struct array
#if 1
	for(int i = 0; i < MAXBLOODSPATTER; i++)
		m_blBloods[i] = { false, 0, Vector(0, 0, 0), 0, RandomInt(0, 4) };
#else
	// for future reference Jason - this is how you should initilize a struct array
	m_sBloods m_blBloods[MAXBLOODSPATTER] = { false, 0, 0, (0,0,0), RandomInt(0, 4) };
#endif

	// not like this!
	/*
	for(int i = 0; i < MAXBLOODSPATTER; i++)
	{
		m_blBloods[i].Alive = false;
		m_blBloods[i].Alpha = 0;
		m_blBloods[i].Color = 0;
		m_blBloods[i].Position = Vector(0,0,0);
		m_blBloods[i].Image = RandomInt(0, 4);
	}*/

	oldAmount = 0;
	aliveSpatter = 0;

	m_bClearHelmet = true;

    // fix for users with diffrent screen ratio (Lodle)
	int screenWide, screenTall;
	GetHudSize(screenWide, screenTall);
	SetBounds(0, 0, screenWide, screenTall);
	
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();

    if (!pPlayer)
    {
        m_aOldAngles = QAngle(0,0,0);
    }
	else
	{
		m_aOldAngles = pPlayer->GetAbsAngles();
	}

	m_flBloodAlphaTime = 0;
}
 
/**
 * Hook up our HUD message, and make sure we are not showing the helmet
 */
void CHudHelmet::Init()
{
	HOOK_HUD_MESSAGE( CHudHelmet, ShowHelmet );
 
	m_bShow = false;
}
 
void CHudHelmet::Reset()
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if(pPlayer != NULL)
	{
		if(pPlayer->IsSuitEquipped())
		{
			if(pPlayer->IsHelmetEquipped())
			{
				m_bShow = true;
			}
			else
			{
				m_bShow = false;
			}
		}
		else
		{
			m_bShow = false;
		}

		for (int i = 0; i < MAXBLOODSPATTER; i++)
		{
			if(m_blBloods[i].Alive)
			{
				m_blBloods[i].Alpha = 0;
				m_blBloods[i].Alive = false;
				aliveSpatter--;
			}
		}
	}
	m_flBloodAlphaTime = 0;
}

/**
 * Load  in the helmet material here
 */
void CHudHelmet::ApplySchemeSettings( vgui::IScheme *scheme )
{

	BaseClass::ApplySchemeSettings(scheme);
 
	SetPaintBackgroundEnabled(false);
	SetPaintBorderEnabled(false);
	
	if (!m_pHelmet)
	{
		m_pHelmet = gHUD.GetIcon("helmet");
	}
	if (!m_pHelmetBlast)
	{
		m_pHelmetBlast = gHUD.GetIcon("helmetblast");
	}
	if (!m_pHelmetBSR1)
	{
		m_pHelmetBSR1 = gHUD.GetIcon("helmetbsr1");
	}
	if (!m_pHelmetBSR2)
	{
		m_pHelmetBSR2 = gHUD.GetIcon("helmetbsr2");
	}
	if (!m_pHelmetBSY1)
	{
		m_pHelmetBSY1 = gHUD.GetIcon("helmetbsy1");
	}
	if (!m_pHelmetBSY2)
	{
		m_pHelmetBSY2 = gHUD.GetIcon("helmetbsy2");
	}
}

// update
void CHudHelmet::OnThink()
{
	if (m_bShow)
	{
		C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
		UpdateHelmetElements( pPlayer );
		if(!m_bClearHelmet) m_bClearHelmet = true;
	}
	else
	{
		if(m_bClearHelmet)
		{
			//C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();

			for (int i = 0; i < MAXBLOODSPATTER; i++)
			{
				if(m_blBloods[i].Alive)
				{
					m_blBloods[i].Alpha = 0;
					m_blBloods[i].Alive = false;
					aliveSpatter--;
				}
			}
			m_bClearHelmet = false;
		}
	}
	// Just update the Alive Blood effects here, this way the time* donsn't get messed up
	if(gpGlobals->curtime > m_flBloodAlphaTime)
	{
		for (int i = 0; i < MAXBLOODSPATTER; i++)
		{
			if(m_blBloods[i].Alive)
			{
				if(m_blBloods[i].Alpha > 0)
				{
					m_blBloods[i].Alpha--;
				}
				else
				{
					aliveSpatter--;

					int x,y,w,h;
					GetBounds( x, y, w, h);
					m_blBloods[i].Position = Vector( RandomInt(x, w-512), RandomInt(y, h-512), 0);

					m_blBloods[i].Image = RandomInt(0,4);
					m_blBloods[i].Alive = false;
				}
			}
		}
		m_flBloodAlphaTime = gpGlobals->curtime + 0.01f;
	}
}

/**
 * Simple - if we want to show the helmet, draw it. Otherwise don't.
 */
void CHudHelmet::Paint( void )
{
    C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();

    if (!pPlayer)
    {
        return;
    }

	if (m_bShow)
	{
		//
		// blood spatter and splat effect
		//
		for (int i = 0; i < MAXBLOODSPATTER; i++)
		{
			if(m_blBloods[i].Alive)
			{
				// red bloods
				if(m_blBloods[i].Color == 0)
				{
					if(m_blBloods[i].Image == 0)
						m_pHelmetBSR1->DrawSelf(m_blBloods[i].Position.x + (m_flOffsetY), m_blBloods[i].Position.y - (m_flOffsetX), 512, 512, Color(255,255,255, m_blBloods[i].Alpha ));
					if(m_blBloods[i].Image == 1)	
						m_pHelmetBSR2->DrawSelf(m_blBloods[i].Position.x + (m_flOffsetY), m_blBloods[i].Position.y - (m_flOffsetX), 512, 512, Color(255,255,255, m_blBloods[i].Alpha ));
				}
				else
				{
					// yellow bloods
					if(m_blBloods[i].Image == 0)
						m_pHelmetBSY1->DrawSelf(m_blBloods[i].Position.x + (m_flOffsetY), m_blBloods[i].Position.y - (m_flOffsetX), 512, 512, Color(255,255,255, m_blBloods[i].Alpha ));
					if(m_blBloods[i].Image == 1)	
						m_pHelmetBSY2->DrawSelf(m_blBloods[i].Position.x + (m_flOffsetY), m_blBloods[i].Position.y - (m_flOffsetX), 512, 512, Color(255,255,255, m_blBloods[i].Alpha ));
				}
			}
		}
		//
		// dirt blast effect
		//
		int i = pPlayer->GetHelmetDamageAlpha();
		if(i != 0)
		{
			m_pHelmetBlast->DrawSelf(-32 + (m_flOffsetY), -32 - (m_flOffsetX), GetWide()+64, GetTall()+64, Color(255,255,255,i));
		}

		// This will draw the scope at the origin of this HUD element, and
		// stretch it to the width and height of the element. As long as the
		// HUD element is set up to cover the entire screen, so will the helmet
		m_pHelmet->DrawSelf(-32 + (m_flOffsetY), -32 - (m_flOffsetX), GetWide()+64, GetTall()+64, Color(255,255,255,255));
	}
}

void CHudHelmet::UpdateHelmetElements( C_BasePlayer* pPlayer )
{
	QAngle newAngle = pPlayer->GetAbsAngles();
	QAngle offsetAngle;
	if( newAngle != m_aOldAngles)
	{
		// calculate the difference
		offsetAngle = m_aOldAngles - newAngle;

		if(!(offsetAngle.x > 1 || offsetAngle.x < -1)) // offset will jump too much
		{
			float result = m_flOffsetX += offsetAngle.x;
			// add the difference to the offsets
			if( (result) > -2)
			{
				if( (result) < 2)
				{
					m_flOffsetX += offsetAngle.x;
				}
			}
		}
		if(!(offsetAngle.y > 1 || offsetAngle.y < -1)) // offset will jump too much
		{
			float result = m_flOffsetY += offsetAngle.y;
			// add the difference to the offsets
			if( (result) > -2)
			{
				if( (result) < 2)
				{
					m_flOffsetY += offsetAngle.y;
				}
			}
		}

		// prevent moving too far off the screen
		if(m_flOffsetX > 32)
			m_flOffsetX = 32;
		if(m_flOffsetX < -32)
			m_flOffsetX = -32;
		if(m_flOffsetY > 32)
			m_flOffsetY = 32;
		if(m_flOffsetY < -32)
			m_flOffsetY = -32;

		// update the oldoffset
		m_aOldAngles = newAngle;
		// update last time player moved angles

	}

	// I try to reset to normal if you are moving
	Vector forwardmov = pPlayer->GetAbsVelocity();
	if(forwardmov.y > 0 || forwardmov.y < 0)
	{
		// reset Y
		if( m_flOffsetY > 0)
		{
			if( m_flOffsetY > .2)
				m_flOffsetY-=.2;
			else
				m_flOffsetY = 0;
		}
		if( m_flOffsetY < 0)
		{
			if( m_flOffsetY < -.2)
				m_flOffsetY+=.2;
			else
				m_flOffsetY = 0;
		}
		// reset x
		if( m_flOffsetX > 0)
		{
			if( m_flOffsetX > .2)
				m_flOffsetX-=.2;
			else
				m_flOffsetX = 0;
		}
		if( m_flOffsetX < 0)
		{
			if( m_flOffsetX < -.2)
				m_flOffsetX+=.2;
			else
				m_flOffsetX = 0;
		}
	}

	//
	// blood spatter and splat effect
	//

	/*
	if the amount of blood is limited to a interger from zero to 100 (which will also be our maz distance)
	then we will devide that by 10, and have a max 10 blood elements on the screen.
	*/

	// Spawn
	int newAmount = pPlayer->GetHelmetBloodAlpha(); // GetHelmetBloodAmount

	if((newAmount > oldAmount) && (aliveSpatter < MAXBLOODSPATTER))
	{
		int amntToAdd = newAmount - oldAmount;
		if( amntToAdd < 1 ) amntToAdd = 1;
		if( amntToAdd > 6 ) amntToAdd = 6;

		for( int a = 0; a < amntToAdd; a++ )
		{
			// should test for HasWeapons, but you must have weapons? yes - or I crash!
			//CBaseCombatWeapon *pActiveWeapon = pPlayer->GetActiveWeapon();
			// ironsighted
			//if(pActiveWeapon->IsIronsighted())
				//break;
			// too much blood already
			if( (aliveSpatter + 1) > MAXBLOODSPATTER)
				break;
			else
			{
				for (int i = 0; i < MAXBLOODSPATTER; i++)
				{
					if(!m_blBloods[i].Alive)
					{
						m_blBloods[i].Color = pPlayer->GetHelmetBloodColor();
							
						int x,y,w,h;
						GetBounds( x, y, w, h);
						m_blBloods[i].Position = Vector( RandomInt(x, w-512), RandomInt(y, h-512), 0);

						m_blBloods[i].Alpha = 255;
						m_blBloods[i].Alive = true;
						aliveSpatter++;
						break;
					}
				}
			}
		}
	}
	oldAmount = newAmount;
}
 
/**
 * Callback for our message - set the show variable to whatever
 * boolean value is received in the message
 */
void CHudHelmet::MsgFunc_ShowHelmet(bf_read &msg)
{
	m_bShow = msg.ReadByte();
}