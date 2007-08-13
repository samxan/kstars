/***************************************************************************
                          skymesh.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 200-07-03
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QHash>
#include <QPolygonF>
#include <QPointF>

#include "skypoint.h"
#include "skymesh.h"
#include "starobject.h"
#include "ksnumbers.h"

// these are just for the draw routine:
#include <QPainter>
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "kstarsdata.h"

SkyMesh* SkyMesh::pinstance = 0;

SkyMesh* SkyMesh::Create( KStarsData* data, int level )
{
	if ( pinstance ) delete pinstance;
	pinstance = new SkyMesh( data, level );
	return pinstance;
}

SkyMesh* SkyMesh::Instance( )
{
	return pinstance;
}

SkyMesh::SkyMesh( KStarsData* data, int level) :
    HTMesh(level, level, NUM_MESH_BUF),
    m_drawID(0), m_data( data ), m_KSNumbers( 0 )
{
    errLimit = HTMesh::size() / 4;
    m_zoomedInPercent = 25;
}

void SkyMesh::aperture(SkyPoint *p0, double radius, MeshBufNum_t bufNum)
{
    SkyPoint p1( p0->ra(), p0->dec() );
    long double now = m_data->updateNum()->julianDay();
    p1.apparentCoord( now, J2000 );

    if ( radius == 1.0 ) {
        printf("\n ra0 = %8.4f   dec0 = %8.4f\n", p0->ra()->Degrees(), p0->dec()->Degrees() );
        printf(" ra1 = %8.4f   dec1 = %8.4f\n", p1.ra()->Degrees(), p1.dec()->Degrees() );

        SkyPoint p2( p1.ra(), p1.dec() );
        p2.updateCoords( m_data->updateNum() );
        printf(" ra2 = %8.4f  dec2 = %8.4f\n", p2.ra()->Degrees(), p2.dec()->Degrees() );
        printf("p0 - p1 = %6.4f degrees\n", p0->angularDistanceTo( &p1 ).Degrees() );
        printf("p0 - p2 = %6.4f degrees\n", p0->angularDistanceTo( &p2 ).Degrees() );
    }

    HTMesh::intersect( p1.ra()->Degrees(), p1.dec()->Degrees(), radius, (BufNum) bufNum);
    m_drawID++;
}

bool SkyMesh::isZoomedIn( int percent )
{
    if ( ! percent ) percent = m_zoomedInPercent;
    return ( intersectSize( DRAW_BUF ) * 100 < percent * size() ); 
}

Trixel SkyMesh::index(SkyPoint *p)
{
    return HTMesh::index( p->ra0()->Degrees(), p->dec0()->Degrees() );
}

Trixel SkyMesh::indexStar( StarObject *star )
{
    double ra, dec;
    star->getIndexCoords( &m_KSNumbers, &ra, &dec );
    return HTMesh::index( ra, dec );
}

void SkyMesh::indexStar( StarObject* star1, StarObject* star2 )
{
    double ra1, ra2, dec1, dec2;
    star1->getIndexCoords( &m_KSNumbers, &ra1, &dec1 );
    star2->getIndexCoords( &m_KSNumbers, &ra2, &dec2 );
    HTMesh::intersect( ra1, dec1, ra2, dec2 );
}


void SkyMesh::index(SkyPoint *p, double radius, MeshBufNum_t bufNum )
{
    HTMesh::intersect( p->ra()->Degrees(), p->dec()->Degrees(), radius, (BufNum) bufNum );
}

void SkyMesh::index( SkyPoint* p1, SkyPoint* p2 )
{
    HTMesh::intersect( p1->ra0()->Degrees(), p1->dec0()->Degrees(),
                        p2->ra0()->Degrees(), p2->dec0()->Degrees() );
}

void SkyMesh::index( SkyPoint* p1, SkyPoint* p2, SkyPoint* p3 )
{
    HTMesh::intersect( p1->ra0()->Degrees(), p1->dec0()->Degrees(),
                        p2->ra0()->Degrees(), p2->dec0()->Degrees(),
                        p3->ra0()->Degrees(), p3->dec0()->Degrees() );
}

void SkyMesh::index( SkyPoint* p1, SkyPoint* p2, SkyPoint* p3, SkyPoint* p4 )
{
    HTMesh::intersect( p1->ra0()->Degrees(), p1->dec0()->Degrees(),
                        p2->ra0()->Degrees(), p2->dec0()->Degrees(),
                        p3->ra0()->Degrees(), p3->dec0()->Degrees(),
                        p4->ra0()->Degrees(), p4->dec0()->Degrees() );
}

void SkyMesh::index( const QPointF &p1, const QPointF &p2, const QPointF &p3 )
{
    HTMesh::intersect( p1.x() * 15.0, p1.y(),
                       p2.x() * 15.0, p2.y(),
                       p3.x() * 15.0, p3.y() );
}

void SkyMesh::index( const QPointF &p1, const QPointF &p2, const QPointF &p3, const QPointF &p4 )
{
    HTMesh::intersect( p1.x() * 15.0, p1.y(),
                       p2.x() * 15.0, p2.y(),
                       p3.x() * 15.0, p3.y(),
                       p4.x() * 15.0, p4.y() );
}

const IndexHash& SkyMesh::indexLine( SkyList* points )
{
    return indexLine( points, NULL );
}

const IndexHash& SkyMesh::indexStarLine( SkyList* points )
{
    SkyPoint *pThis, *pLast;

    indexHash.clear();

    if ( points->size() == 0 ) return indexHash;

    pLast = points->at( 0 );
    for ( int i=1 ; i < points->size() ; i++ ) {
        pThis = points->at( i );

        indexStar( (StarObject*) pThis, (StarObject*) pLast );
        MeshIterator region( this );

        while ( region.hasNext() ) {
            indexHash[ region.next() ] = true;
        }
        pLast = pThis;
    }

    //printf("indexStarLine %d -> %d\n", points->size(), indexHash.size() );
    return indexHash;
}


const IndexHash& SkyMesh::indexLine( SkyList* points, IndexHash* skip )
{
    SkyPoint *pThis, *pLast;

    indexHash.clear();

    if ( points->size() == 0 ) return indexHash;

    pLast = points->at( 0 );
    for ( int i=1 ; i < points->size() ; i++ ) {
        pThis = points->at( i );

        if (skip != NULL && skip->contains( i ) ) {
            pLast = pThis;
            continue;
        }

        index( pThis, pLast );
        MeshIterator region( this );

        if ( region.size() > errLimit ) {
            printf("\nSkyMesh::indexLine: too many trixels: %d\n", region.size() );
            printf("    ra1  = %f;\n", pThis->ra0()->Degrees());
            printf("    ra2  = %f;\n", pLast->ra0()->Degrees());
            printf("    dec1 = %f;\n", pThis->dec0()->Degrees());
            printf("    dec2 = %f;\n", pLast->dec0()->Degrees());
            HTMesh::setDebug( 10 );
            index( pThis, pLast );
            HTMesh::setDebug ( 0 );
        }

        // This was used to track down a bug in my HTMesh code. The bug was caught
        // and fixed but I've left this debugging code in for now.  -jbb

        else {
            while ( region.hasNext() ) {
                indexHash[ region.next() ] = true;
            }
        }
        pLast = pThis;
    }
    return indexHash;
}


// ----- Create HTMesh Index for Polygons -----
// Create (mostly) 4-point polygons that cover the mw polygon and
// all share the same first vertex.  Use indexHash to eliminate
// the many duplicate indices that are generated with this procedure.
// There are probably faster and better ways to do this.

const IndexHash& SkyMesh::indexPoly( SkyList *points )
{

    indexHash.clear();

    if (points->size() < 3) return indexHash;

    SkyPoint* startP = points->first();

    int end = points->size() - 2;     // 1) size - 1  -> last index,
                                      // 2) minimum of 2 points

    for( int p = 1; p <= end; p+= 2 ) {

        if ( p == end ) {
            index( startP, points->at(p), points->at(p+1) );
        }
        else {
            index( startP, points->at(p), points->at(p+1), points->at(p+2) );
        }

        MeshIterator region( this );

        if ( region.size() > errLimit ) {
            printf("\nSkyMesh::indexPoly: too many trixels: %d\n", region.size() );

            printf("    ra1 = %f;\n", startP->ra0()->Degrees());
            printf("    ra2 = %f;\n", points->at(p)->ra0()->Degrees());
            printf("    ra3 = %f;\n", points->at(p+1)->ra0()->Degrees());
            if ( p < end )
                printf("    ra4 = %f;\n", points->at(p+2)->ra0()->Degrees());

            printf("    dec1 = %f;\n", startP->dec0()->Degrees());
            printf("    dec2 = %f;\n", points->at(p)->dec0()->Degrees());
            printf("    dec3 = %f;\n", points->at(p+1)->dec0()->Degrees());
            if ( p < end )
                printf("    dec4 = %f;\n", points->at(p+2)->dec0()->Degrees());

            printf("\n");

        }
        while ( region.hasNext() ) {
            indexHash[ region.next() ] = true;
        }
    }
    return indexHash;
}

const IndexHash& SkyMesh::indexPoly( const QPolygonF* points )
{
    indexHash.clear();

    if (points->size() < 3) return indexHash;

    const QPointF startP = points->first();

    int end = points->size() - 2;     // 1) size - 1  -> last index,
                                      // 2) minimum of 2 points
    for( int p = 1; p <= end; p+= 2 ) {

        if ( p == end ) {
            index( startP, points->at(p), points->at(p+1) );
        }
        else {
            index( startP, points->at(p), points->at(p+1), points->at(p+2) );
        }

        MeshIterator region( this );

        if ( region.size() > errLimit ) {
            printf("\nSkyMesh::indexPoly: too many trixels: %d\n", region.size() );

            printf("    ra1 = %f;\n", startP.x() );
            printf("    ra2 = %f;\n", points->at(p).x() );
            printf("    ra3 = %f;\n", points->at(p+1).x()) ;
            if ( p < end )
                printf("    ra4 = %f;\n", points->at(p+2).x() );

            printf("    dec1 = %f;\n", startP.y() );
            printf("    dec2 = %f;\n", points->at(p).y() );
            printf("    dec3 = %f;\n", points->at(p+1).y() );
            if ( p < end )
                printf("    dec4 = %f;\n", points->at(p+2).y());

            printf("\n");

        }
        while ( region.hasNext() ) {
            indexHash[ region.next() ] = true;
        }
    }
    return indexHash;
}

void SkyMesh::draw(KStars *kstars, QPainter& psky, double scale, MeshBufNum_t bufNum)
{
    SkyMap*     map  = kstars->map();
    KStarsData* data = kstars->data();
    //KSNumbers*  num  = data->updateNum();

    //QPainter psky;
    //psky.begin( map );

    double r1, d1, r2, d2, r3, d3;

    MeshIterator region( this, bufNum );
    while ( region.hasNext() ) {
        Trixel trixel = region.next();
        vertices( trixel, &r1, &d1, &r2, &d2, &r3, &d3 );
        SkyPoint s1( r1 / 15.0, d1 );
        SkyPoint s2( r2 / 15.0, d2 );
        SkyPoint s3( r3 / 15.0, d3 );
        //s1.updateCoords( num );
        //s2.updateCoords( num );
        //s3.updateCoords( num );
        s1.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        s2.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        s3.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        QPointF q1 = map->toScreen( &s1, scale );
        QPointF q2 = map->toScreen( &s2, scale );
        QPointF q3 = map->toScreen( &s3, scale );
        psky.drawLine( q1, q2 );
        psky.drawLine( q2, q3 );
        psky.drawLine( q3, q1 );
    }
}

