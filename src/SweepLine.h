/*
 * roofer is written in C++ and uses CGAL.  It computes straight skeleton roofs
 * as well as minimum- and maximum-volume roofs over a simple polygon.
 * Copyright (C) 2016 - Günther Eder - roofer@geder.at
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef SWEEPLINE_H_
#define SWEEPLINE_H_

/** 1. add all lines to SweepLine with addLine
 *  2. initiate event queue */

#include <CGAL/intersections.h>

#include <map>
#include <vector>

#include "CGALTypes.h"
#include "Definitions.h"

using namespace std;

/**
 * defines a line in the line arrangement, belongs to one 'base' edge and the arrangement
 * line is the 'bisector' between the referenced second edge 'e' and 'base'.
 * */
struct ArrangementLine {
	EdgeIterator base, e;

	Point start;
	Ray   bisector;

	/* if base and e are parallel the bisector is either a line or there is none*/
	Line  line;
	Exact dist;

	/* we will use this idx to referenc to the current facets adjacent to 'bisector' */
	int leftListIdx, rightListIdx;

	/* to enables an ordering if two ArrangementLines of different base are compared  */
	int uid;

	/* TODO: remove, just for testing */
	int lid;

	int eid;

	bool parallel, isValid, ghost;

	ArrangementLine(EdgeIterator pbase, EdgeIterator pe, int id = NIL, int edgeid = NIL):
		base(pbase),e(pe),leftListIdx(NOLIST),rightListIdx(NOLIST),uid(id),lid(NIL),eid(edgeid),
		parallel(false),isValid(true),ghost(false) {

		CGAL::Object intersect = intersection(base->supporting_line(),e->supporting_line());

		if(base != e) {
			if(!intersect.empty()) {
				if(const Point *ipoint = CGAL::object_cast<Point>(&intersect)) {
					start    = *ipoint;
					bisector = setBisector();
				} else {
					parallel = true;
				}
			} else {
				parallel = true;
			}
		} else {
			parallel = true;
		}

		if(parallel) {
			if(base->supporting_line().direction() != e->supporting_line().direction()) {
				start = INFPOINT;
				line  = CGAL::bisector(base->supporting_line(), e->supporting_line());
				dist  = CGAL::squared_distance(base->supporting_line(),line);
			} else if(base->supporting_line() == e->supporting_line()) {
				ghost   = true;
				isValid = false;
			} else {
				isValid = false;
			}
		}
	}

	friend bool operator>  (const ArrangementLine& a, const ArrangementLine& b);
	friend bool operator<  (const ArrangementLine& a, const ArrangementLine& b);
	friend bool operator== (const ArrangementLine& a, const ArrangementLine& b);
	friend bool operator>  (const ArrangementLine& a, const Point& b);
	friend bool operator<  (const ArrangementLine& a, const Point& b);
	friend ostream& operator<<(ostream& os, const ArrangementLine& al);

	/* The second Line in the bisector is with changed orientation on purpose */
	inline Ray setBisector() {
		Line bisectorLine = CGAL::bisector(base->supporting_line(), Line(e->vertex(1),e->vertex(0)));
		auto ray = Ray(start,bisectorLine);

		if(Line(*base).has_on_negative_side(start + ray.to_vector()) ||
		   Line(*e).has_on_negative_side(start + ray.to_vector()) 	) {
			ray = Ray(start,bisectorLine.opposite());
		}
		return ray;
	}

	inline void setEID(int _eid) {eid=_eid;}

	inline void setGhostVertex(Point s) {
		isValid  = true;
		ghost    = true;
		parallel = false;
		start    = s;
		bisector = Ray(start,base->supporting_line().perpendicular(s));
		Point q(base->supporting_line().projection(s));
		if(Line(q,s).direction() != bisector.direction()) {
			bisector = Ray(start,base->supporting_line().perpendicular(s).opposite());
		}
	}
};

/* used to initially sort the ArrangementLines along their 'base' line */
using ArrangementStart 		= map<EdgeIterator,set<ArrangementLine, less<ArrangementLine> > >;
/* holds the actual ArrangementLines, to which we point to */
using AllArrangementLines	= map<EdgeIterator,list<ArrangementLine> >;

using ALIterator 			= ArrangementLine*;

struct SweepItem {
	ALIterator a, b;
	EdgeIterator base;

	bool  raysIntersect;
	Exact squaredDistance;
	Point intersectionPoint;

#ifdef QTGUI
	inline Point3D getPoint3D() {return Point3D(intersectionPoint.x().doubleValue(),
			intersectionPoint.y().doubleValue(),squaredDistance.doubleValue());}
#endif

	SweepItem(const SweepItem& i):SweepItem(i.a,i.b) {}

	SweepItem(ALIterator pa, ALIterator pb):a(pa),b(pb),base(pa->base) {
		if(a->base != b->base) {
			if(a->base->supporting_line() == b->base->supporting_line()) {LOG(INFO) << " same line though ";}
			LOG(INFO) << "ERROR: Base Line not equal!" << endl <<
			a->uid << " " << b->uid << endl;
			if(a->parallel) LOG(INFO) << "(a parallel)";
			if(b->parallel) LOG(INFO) << "(b parallel)";
			LOG(INFO) << "a: ";
			LOG(INFO) << a->base->vertex(0).x().doubleValue() << "," <<
					a->base->vertex(0).y().doubleValue() << "-" <<
					a->base->vertex(1).x().doubleValue() << "," <<
					a->base->vertex(1).y().doubleValue() << "  ";
			LOG(INFO) << "b: ";
			LOG(INFO) << b->base->vertex(0).x().doubleValue() << "," <<
					b->base->vertex(0).y().doubleValue() << "-" <<
					b->base->vertex(1).x().doubleValue() << "," <<
					b->base->vertex(1).y().doubleValue() << "  ";
			throw runtime_error("ERROR: Base Lines not equal!");
		}

		CGAL::Object intersect = intersection(a->bisector,b->bisector);

		if(a->parallel && b->parallel) {
			intersect = intersection(a->line,b->line);
		} else if(a->parallel) {
			intersect = intersection(a->line,b->bisector);
		} else if(b->parallel) {
			intersect = intersection(a->bisector,b->line);
		}

		if(!intersect.empty()) {
			if(const Point *ipoint = CGAL::object_cast<Point>(&intersect)) {
				raysIntersect     = true;
				squaredDistance    = CGAL::squared_distance(a->base->supporting_line(),*ipoint);
				intersectionPoint = *ipoint;
			} else {
				raysIntersect  = false;
				squaredDistance = -1;
			}
		} else {
			raysIntersect  	   = false;
			squaredDistance     = -1;
		}
	}

	inline Exact dist() { return squaredDistance; }

	/* enable accessing the list indices of left/right refs of a,b via setter/getter
	 * (0,0) a left, (0,1) a right, (1,0) b left, (1,1) b right
	 * */
	inline int get(int i, int j) const {
		if(i == 0) {
			if(j == 0) {
				return a->leftListIdx;
			} else if(j == 1) {
				return a->rightListIdx;
			}
		} else if(i == 1) {
			if(j == 0) {
				return b->leftListIdx;
			} else if(j == 1) {
				return b->rightListIdx;
			}
		}
		else throw runtime_error("get(i,j) i or j > 1!");
		return NOLIST;
	}

	inline void set(int i, int j, int idx) {
		if(i == 0) {
			if(j == 0) {
				a->leftListIdx = idx;
			} else if(j == 1) {
				a->rightListIdx = idx;
			}
		} else if(i == 1) {
			if(j == 0) {
				b->leftListIdx = idx;
			} else if(j == 1) {
				b->rightListIdx = idx;
			}
		}
		else throw runtime_error("get(i,j) i or j > 1!");
	}

	inline void setAll(int idx) {
		for(int i = 0; i < 2; ++i) {
			for(int j = 0; j < 2; ++j) {
				set(i,j,idx);
			}
		}
	}

	inline bool isPossibleDivideNode() {
		return a->leftListIdx  != NOLIST    &&
			   a->rightListIdx == NOLIST    &&
			   b->leftListIdx  == NOLIST    &&
			   b->rightListIdx != NOLIST;
	}

	inline bool isEmptyNode() {
		return a->leftListIdx  == NOLIST    &&
			   a->rightListIdx == NOLIST    &&
			   b->leftListIdx  == NOLIST    &&
			   b->rightListIdx == NOLIST;
	}

	inline bool isEdgeEvent() {
		return a->leftListIdx  == NOLIST    &&
			   a->rightListIdx != NOLIST    &&
			   b->leftListIdx  != NOLIST    &&
			   b->rightListIdx == NOLIST;
	}

	inline bool isInteriorNode() {
		return a->leftListIdx  != NOLIST          &&
			   a->rightListIdx == a->leftListIdx  &&
//			   b->leftListIdx  == a->rightListIdx &&
			   b->leftListIdx  != NOLIST &&
			   b->rightListIdx == b->leftListIdx;
	}

	inline bool isBoundaryNode() {
		return (a->leftListIdx == NOLIST && a->rightListIdx != NOLIST) ||
			   (a->leftListIdx != NOLIST && a->rightListIdx == NOLIST) ||
			   (b->leftListIdx == NOLIST && b->rightListIdx != NOLIST) ||
			   (b->leftListIdx != NOLIST && b->rightListIdx == NOLIST);
	}

	inline int firstListIndex() {
		for(int i = 0; i < 2; ++i) {
			for(int j = 0; j < 2; ++j) {
				if(get(i,j) != NOLIST) {
					return get(i,j);
				}
			}
		}
		return NOLIST;
	}

	inline int numberOfActiveIndices() {
		int cnt = 0;
		for(int i = 0; i < 2; ++i) {
			for(int j = 0; j < 2; ++j) {
				if(get(i,j) != NOLIST) {
					++cnt;
				}
			}
		}
		return cnt;
	}

	inline bool isBoundaryOrInteriorNode() {
		return isBoundaryNode() || isInteriorNode();
	}

	inline bool hasAtLeastOneListIdx() {
		return firstListIndex() != NOLIST;
	}

	inline bool hasGhostVertex() {return a->ghost || b->ghost;}


	friend std::ostream& operator<<(std::ostream& out, const SweepItem& item);

	friend bool operator>  (const SweepItem& a, const SweepItem& b);
	friend bool operator<  (const SweepItem& a, const SweepItem& b);
	friend bool operator== (const SweepItem& a, const SweepItem& b);
};

/* used as compare object, to enable binary search in the sweep line status
 * at a specific distance to the base line */
struct DistanceCompare {
	Point currentIntersection;
	DistanceCompare(Point p): currentIntersection(p) {}

	// < Operator
	bool operator()  (ALIterator a, ALIterator b) {
		// TODO: remove and check if needed (bad idea)
		return operator()(*a,*b);
	}

	bool operator()  (ArrangementLine a, ArrangementLine b) {
		if(a.base != b.base) {return a.uid < b.uid;}

		if(a.parallel && b.parallel) {
			return b.dist < a.dist;
		}

		Line currentBase(currentIntersection,a.base->direction());

		CGAL::Object intersectionA = intersection(currentBase, a.bisector.supporting_line());
		CGAL::Object intersectionB = intersection(currentBase, b.bisector.supporting_line());

		if(!intersectionA.empty() && !intersectionB.empty()) {
			if(const Point *pointA = CGAL::object_cast<Point>(&intersectionA)) {
				if(const Point *pointB = CGAL::object_cast<Point>(&intersectionB)) {
					return a.base->direction() == Vector(*pointB - *pointA).direction();
				}
			}
		}

		throw runtime_error("ERROR: empty intersections!");
	}
};

struct ParallelALIteratorLess {
	// > Operator
	bool operator()  (ALIterator a, ALIterator b) {
		return (b->dist > a->dist) || (b->dist == a->dist && b->eid > a->eid);
	}
};

using EventQueue 	   		= set<SweepItem,less<SweepItem> >;
using ParallelEventQueue	= set<ALIterator,ParallelALIteratorLess>;

using LocalSweepLineStatus  = vector<ALIterator>;
using SweepLineStatus  		= map<EdgeIterator,LocalSweepLineStatus>;


struct SweepEvent : public vector<SweepItem> {
	inline int numberActiveCells() {
		int cnt = 0;
		if(!empty()) {
			for(auto& e : *this) {
				if(e.hasAtLeastOneListIdx()) {
					++cnt;
				}
			}
		}
		return cnt;
	}

	inline int numberDivideNodes() {
		int cnt = 0;
		auto cells = getActiveCells();
		for(auto cell : cells) {
			if(cell->isPossibleDivideNode()) {
				++cnt;
			}
		}
		return cnt;
	}

	inline bool containsEdgeEvent() {
		for(auto& e : *this) {
			if(e.isEdgeEvent()) {
				return true;
			}
		}
		return false;
	}

	inline bool containsInteriorNode() {
		for(auto& e : *this) {
			if(e.isInteriorNode()) {
				return true;
			}
		}
		return false;
	}

	inline bool hasGhostVertex() {
		for(auto c : *this) {
			if(c.a->ghost || c.b->ghost) {
				return true;
			}
		}
		return false;
	}

	inline vector<SweepItem*> getActiveCells() {
		vector<SweepItem*> r;
		for(auto& c : *this) {if(c.hasAtLeastOneListIdx()) r.push_back(&c);}
		return r;
	}

	inline vector<SweepItem*> getAllCells() {
		vector<SweepItem*> r;
		for(auto& c : *this) {r.push_back(&c);}
		return r;
	}

	friend std::ostream& operator<<(std::ostream& out, const SweepEvent& event);
};

class SweepLine {
public:
	SweepLine():config(nullptr) {}

	inline void addLine(ArrangementLine a) { arrangementStart[a.base].insert(a); }
	void initiateEventQueue();

	inline bool queueEmpty() { return eventQueue.empty(); }
	inline int queueSize()  { return eventQueue.size(); }

	SweepEvent popEvent();


	inline void addParallelLine(ArrangementLine al) {
		allParallelAL.push_back(al);
	}

	bool handlePopEvent(SweepItem& item);

	void printSweepLine(SweepItem& item);
	void printEventQueue();

	inline void setConfig(const Config* conf)   { config  = conf;}
	ALIterator insertGhostVertex(SweepItem* cell, SweepEvent& ghostCells);
	void deleteGhostVertex(SweepItem* cell, ALIterator gv);

	SweepLineStatus 			status;

private:
	ArrangementStart 			arrangementStart;
	AllArrangementLines 		allArrangementLines;

	EventQueue 					eventQueue;

	vector<ArrangementLine> 	allParallelAL;
	ParallelEventQueue 			parallelEventQueue;

	const Config*   			config;
};

#endif /* SWEEPLINE_H_*/
