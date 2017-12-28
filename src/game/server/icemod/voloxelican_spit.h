//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot by voloxelican, hard, breaks into bits. Also acidy.
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	VOLOXELICANSPIT_H
#define	VOLOXELICANSPIT_H

#include "basegrenade_shared.h"

class CParticleSystem;

#define SPIT_GRAVITY 600

class CVoloxelicanSpit : public CBaseGrenade
{
	DECLARE_CLASS( CVoloxelicanSpit, CBaseGrenade );

public:
						CVoloxelicanSpit( void );

	virtual void		Spawn( void );
	virtual void		Precache( void );
	virtual void		Event_Killed( const CTakeDamageInfo &info );

	virtual	unsigned int	PhysicsSolidMaskForEntity( void ) const { return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_WATER ); }

	void 				VoloxelicanSpitTouch( CBaseEntity *pOther );

	void				Detonate( void );
	void				Think( void );

private:
	DECLARE_DATADESC();
	
	void	InitHissSound( void );
	
	CHandle< CParticleSystem >	m_hSpitEffect;
	CSoundPatch		*m_pHissSound;
	bool			m_bPlaySound;
};

#endif	//VOLOXELICANSPIT_H
