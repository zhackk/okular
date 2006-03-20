/***************************************************************************
 *   Copyright (C) 2004-05 by Enrico Ros <eros.kde@email.it>               *
 *   Copyright (C) 2005 by Piotr Szymanski <niedakh@gmail.com>             *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef _KPDF_AREA_H_
#define _KPDF_AREA_H_
#include <qlinkedlist.h>
#include <qcolor.h>
#include <kdebug.h>
class QRect;
class KPDFLink;

class NormalizedShape;

/**
 * @short A point in [0,1] coordinates (only used in annotations atm)
 */
class NormalizedPoint
{
    public:
        double x, y;

        NormalizedPoint();
        NormalizedPoint( double dX, double dY );
        NormalizedPoint( int ix, int iy, int xScale, int yScale );
};

/**
 * @short A rect in normalized [0,1] coordinates.
 */
class NormalizedRect
{
    public:
        double left, top, right, bottom;

        NormalizedRect();
        NormalizedRect( double l, double t, double r, double b );
        NormalizedRect( const QRect &r, double xScale, double yScale );
        bool isNull() const;
        bool contains( double x, double y ) const;
        bool intersects( const NormalizedRect & normRect ) const;
        bool intersects( double l, double t, double r, double b ) const;
        bool intersects( const NormalizedRect * r ) const;
        QRect geometry( int xScale, int yScale ) const;
	NormalizedRect operator| (const NormalizedRect & r) const;
	NormalizedRect& operator|= (const NormalizedRect & r);
};

// kdbgstream& operator << (kdbgstream &, const NormalizedRect &);

/**
 * @short NormalizedRect that contains a reference to an object.
 *
 * These rects contains a pointer to a kpdf object (such as a link or something
 * like that). The pointer is read and stored as 'void pointer' so cast is
 * performed by accessors based on the value returned by objectType(). Objects
 * are reparented to this class.
 *
 * Type / Class correspondency tab:
 *  - Link      : class KPDFLink  : description of a link
 *  - Image     : class KPDFImage : description of an image (n/a)
 */
class ObjectRect : public NormalizedRect
{
    public:
        // definition of the types of storable objects
        enum ObjectType { Link, Image };

        // default constructor: initialize all parameters
        ObjectRect( double l, double t, double r, double b, ObjectType typ, void * obj );
        ObjectRect( NormalizedRect x, ObjectType type, void * pnt ) ;
        ~ObjectRect();

        // query type and get a const pointer to the stored object
        inline ObjectType objectType() const { return m_objectType; }
        inline const void * pointer() const { return m_pointer; }

    private:
        ObjectType m_objectType;
        void * m_pointer;
};

/**
 * Internal Storage: normalized colored highlight owned by id
 */
struct HighlightRect : public NormalizedRect
{
    // searchID of the highlight owner
    int s_id;
    // color of the highlight
    QColor color;
};

/**
 * @short A regular area of NormalizedShape which normalizes a Shape
 * 
 * Class NormalizedShape must have the following functions defined: 
 * contains (double, double)
 * intersects(NormalizedShape)
 * isNull()
 * geometry(int,int)
 * operator | and |= which unite two NormalizedShapes
 */

template <class NormalizedShape, class Shape> class RegularArea : 
public  QLinkedList<NormalizedShape*>
{
	public:
		typedef QValueListIterator<NormalizedShape*> Iterator;
		typedef QValueListConstIterator<NormalizedShape*> ConstIterator;
//        RegularArea<NormalizedShape,Shape> (NormalizedShape* x)  { QLinkedList(x) ; } ;
// 		class Iterator : public QLinkedListIterator<NormalizedShape*> {};
		bool contains( double x, double y ) const;
                bool contains( NormalizedShape * ) const;
		bool intersects (const RegularArea<NormalizedShape,Shape> * area) const;
		bool intersects (const NormalizedShape * shape) const;
		void appendArea (const RegularArea<NormalizedShape,Shape> *area);
		void simplify ();
		bool isNull() const;
		QLinkedList<Shape>* geometry( int xScale, int yScale, int dx=0,int dy=0 ) const;
};

template <class NormalizedShape, class Shape>
void RegularArea<NormalizedShape, Shape>::simplify()
{
            int end=this->count(),i=0,x=0;
            QLinkedList <NormalizedShape*> m_remove;
            for (;i<end;i++)
            {
                if ( i < (end-1) )
                {
                    if ( (*this)[x]->intersects( (*this)[i+1] ) )
                    {
                        *((*this)[x]) |= *((*this)[i+1]);
                        m_remove.append( (*this)[i+1] );
                    }
                    else
                    {
                        x=i+1;
                   }
                }
            }
            while (!m_remove.isEmpty())
            {
                this->remove( m_remove.last() );
                m_remove.pop_back();
            }
            kDebug() << "from " << end << " to " << this->count() << endl;
}

template <class NormalizedShape, class Shape>
bool RegularArea<NormalizedShape, Shape>::isNull() const
{
	if (!this)
		return false;

	if (this->isEmpty())
		return false;

	ConstIterator i;
	for (i=this->begin();i!=this->end();++i)
		if (!((*i)->isNull()))
			return false;
	return true;

}

template <class NormalizedShape, class Shape>
bool RegularArea<NormalizedShape, Shape>::intersects (const NormalizedShape *rect) const
{
	if (!this)
		return false;

	if (this->isEmpty())
		return false;

	ConstIterator i;
	for (i=this->begin();i!=this->end();++i)
	{
		if(!((*i)->isNull()) && (*i)->intersects (rect))
			return true;
	}
	return false;
}

template <class NormalizedShape, class Shape>
bool RegularArea<NormalizedShape, Shape>::intersects 
	(const RegularArea<NormalizedShape,Shape> *area) const
{
	if (!this)
		return false;
	if (this->isEmpty())
		return false;

	Iterator i,j;
	for (i=this->begin();i!=this->end();++i)
	{
		for (j=area->begin();j!=area->end();++j)
		{
			if(!((*i)->isNull) && (*i)->intersects (j))
				return true;
		}
	}
	return false;
}

template <class NormalizedShape, class Shape>
void RegularArea<NormalizedShape, Shape>::appendArea
	(const RegularArea<NormalizedShape, Shape> *area) 
{
	if (!this)
		return false;

	ConstIterator j;
	for (j=area->begin();j!=area->end();++j)
	{
        this->append(*j);
	}
}


template <class NormalizedShape, class Shape>
bool RegularArea<NormalizedShape, Shape>::contains (double x, double y) const
{
	if (!this)
		return false;
	if (this->isEmpty())
		return false;

	ConstIterator i;
	for (i=this->begin();i!=this->end();++i)
	{
		if((*i)->contains (x,y))
			return true;
	}
	return false;
}

template <class NormalizedShape, class Shape>
bool RegularArea<NormalizedShape, Shape>::contains (NormalizedShape * shape) const
{
        if (!this)
                return false;
        if (this->isEmpty())
                return false;

        const QLinkedList<NormalizedShape*> * const lista=dynamic_cast<const QLinkedList<NormalizedShape*> * const >(this);
        return lista->contains(shape);
}

template <class NormalizedShape, class Shape>
QLinkedList<Shape> *
RegularArea<NormalizedShape, Shape>::geometry( int xScale, int yScale, int dx, int dy ) const
{
	if (!this)
		return false;
	if (this->isEmpty()) 
		return 0;

	ConstIterator i;
	QLinkedList<Shape>* ret=new QLinkedList<Shape>;
        Shape t;
	for (i=this->begin();i!=this->end();++i)
	{
            t=(*i)->geometry(xScale,yScale);
            t.moveBy(dx,dy); 
            ret->append(t);
	}

	return ret;
}

typedef RegularArea<NormalizedRect,QRect> RegularAreaRect;


class HighlightAreaRect : public RegularAreaRect {
	public:
		HighlightAreaRect(RegularAreaRect *area);
		// searchID of the highlight owner
		int s_id;
		// color of the highlight
		QColor color;
};

#endif
