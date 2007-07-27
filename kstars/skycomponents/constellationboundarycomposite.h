/***************************************************************************
               constellationboundarycomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 25 Oct. 2005
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CONSTELLATIONBOUNDARYCOMPOSITE_H
#define CONSTELLATIONBOUNDARYCOMPOSITE_H

#include "skycomposite.h"

#include <QHash>
#include <QPolygonF>

/**
	*@class ConstellationBoundaryComposite
	*Collection of lines comprising the borders between constellations

	*@author Jason Harris
	*@version 0.1
	*/

class ConstellationBoundaryComposite : public SkyComposite 
{
	public:
	/**
		*@short Constructor
		*Simply adds all of the coordinate grid circles 
		*(meridians and parallels)
		*@p parent Pointer to the parent SkyComponent object
		*/
		ConstellationBoundaryComposite( SkyComponent *parent );

	/**
		*@short Initialize the Constellation boundary composite
		*Reads the constellation boundary data from cbounds.dat.  
		*The boundary data is defined by a series of RA,Dec coordinate pairs 
		*defining the "nodes" of the boundaries.  The nodes are organized into 
		*"segments", such that each segment represents a continuous series 
		*of boundary-line intervals that divide two particular constellations.
		*
		*@param data Pointer to the KStarsData object
		*/
		virtual void init( KStarsData *data );

		QPolygonF boundary( const QString &name ) const;

		QString constellation( SkyPoint *p );
		bool inConstellation( const QString &name, SkyPoint *p );

	private:
		QHash<QString, QPolygonF> Boundary;
};


#endif