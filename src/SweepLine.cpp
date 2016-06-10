
#include "SweepLine.h"

using namespace std;

//bool operator==(const ArrangementLine* a, const ArrangementLine* b) {
//		return  a->base  == b->base &&
//				a->start == b->start &&
//				a->e     == b->e;
//}
//bool operator> (const ArrangementLine* a, const ArrangementLine* b) {
//    return (a->base == b->base && a->base->direction() == Vector(a->start - b->start).direction()) ||
//    	   (a->base != b->base && a->uid > b->uid);
//}
//bool operator< (const ArrangementLine* a, const ArrangementLine* b) {
//    return (a->base == b->base && a->base->direction() == Vector(b->start - a->start).direction()) ||
//     	   (a->base != b->base && a->uid < b->uid);
//}
bool operator==(const ArrangementLine& a, const ArrangementLine& b) {
		return  a.base  == b.base &&
				a.start == b.start &&
				a.e     == b.e;
}
bool operator> (const ArrangementLine& a, const ArrangementLine& b) {
    return (a.base == b.base && a.start != b.start &&
    		a.base->direction() == Vector(a.start - b.start).direction())
    		||
    	   (a.base == b.base && a.start == b.start &&
    		CGAL::orientation(a.bisector.to_vector(),b.bisector.to_vector()) == CGAL::RIGHT_TURN)
		    ||
		   (a.base != b.base && a.eid > b.eid);
}
bool operator< (const ArrangementLine& a, const ArrangementLine& b) {
    return (a.base == b.base && a.start != b.start &&
    		a.base->direction() == Vector(b.start - a.start).direction())
    		||
     	   (a.base == b.base && a.start == b.start &&
     		CGAL::orientation(a.bisector.to_vector(),b.bisector.to_vector()) == CGAL::LEFT_TURN)
			||
		   (a.base != b.base && a.eid < b.eid);
}

bool operator== (const SweepItem& a, const SweepItem& b) {
	return a.normalDistance == b.normalDistance && (
		   (a.a 				== b.a &&
		    b.b 				== b.b) || (
			a.a 				== b.b &&
		    b.b 				== b.a  ) );
}
bool operator> (const SweepItem& a, const SweepItem& b) {
	return (a.normalDistance  > b.normalDistance) ||
		   (a.normalDistance == b.normalDistance && a.a > b.a);
}
bool operator< (const SweepItem& a, const SweepItem& b) {
	return (a.normalDistance  < b.normalDistance) ||
		   (a.normalDistance == b.normalDistance && a.a < b.a);
}


void SweepLine::initiateEventQueue() {
	assert(arrangementStart.empty());

	for(auto& le : arrangementStart) {
		assert(le.second.empty() && le.second.size > 1);

		auto& arrangementLines = allArrangementLines[le.first];
		auto& lStatus          = status[le.first];

		while(!le.second.empty()) {
			auto a = le.second.top();

			arrangementLines.push_back(a);

			le.second.pop();
		}


		for(auto i = arrangementLines.begin(); i != arrangementLines.end(); ++i) {
			lStatus.push_back(i);
			if(i+1 != arrangementLines.end()) {
				SweepItem item(i,(i+1));

				if(item.raysIntersect && item.normalDistance > 0) {
					eventQueue.insert(item);
				}
			}
		}
	}

	/* remove dist 0 events */
//	while(!eventQueue.empty() && eventQueue.begin()->normalDistance == 0) {
//		eventQueue.erase(eventQueue.begin());
//	}

	for(auto &line : status) {
		int cnt = 0;
		for(auto l : line.second) {
			l->lid = cnt++;
		}
	}
}

void SweepLine::initializePlaneSweepStart() {

}

void SweepLine::printEventQueue() {
	cout << "Q: ";

	for(auto e : eventQueue) {
		cout << e.normalDistance.doubleValue() << " - "; e.print(); cout << endl;
	}

	cout << endl;
}

void SweepLine::printSweepLine(SweepItem& item) {
	cout << status[item.base].front()->eid << ": ";
	for(auto l : status[item.base]) {
		if(l == item.a || l == item.b) {
			cout << "(";
		}
		cout << l->lid;
		if(l == item.a || l == item.b) {
			cout << ")";
		}
		cout << ", ";
	}
	cout << endl;
}


SweepEvent SweepLine::popEvent() {
	if(queueEmpty()) {
		throw out_of_range("EventQueue is empty!");
	} else {
		SweepEvent event;
		SweepEvent temp;

		auto first = *eventQueue.begin();
		eventQueue.erase(eventQueue.begin());
		event.push_back(first);

		while(!queueEmpty() && first.normalDistance == eventQueue.begin()->normalDistance) {
			auto other = eventQueue.begin();

			if((first.base != other->base) &&
			   (first.a->e  == other->base || first.b->e  == other->base ) ) {
				event.push_back(*other);
			} else {
				temp.push_back(*other);
				cout << "Warning: event added to temp!" << endl;
			}
			eventQueue.erase(other);
		}

		if(temp.size() > 0) {
			cout << "Temp: ";
			for(auto e : temp)  {
				cout << "(" << e.intersectionPoint.x().doubleValue() << ","
					 << e.intersectionPoint.y().doubleValue() << ") ";
				e.print();
			}
			cout << endl;

			for(auto e : temp)  { eventQueue.insert(e); }
		}

		if(event.size() != 3){
			cout << "Event: ";
			for(auto e : event) {
				cout << "(" << e.intersectionPoint.x().doubleValue()
					 << "," << e.intersectionPoint.y().doubleValue() << ") D:"
					 << e.normalDistance.doubleValue();
				e.print();
			}
			cout << endl;
		}

		for(auto e : event) {
			handlePopEvent(e);
		}

		return event;
	}
}

void SweepLine::handlePopEvent(SweepItem& item) {
	const ALIterator a = item.a;
	const ALIterator b = item.b;
	auto& lStatus = status[item.base];

	DistanceCompare comp(item.intersectionPoint);

	assert(!(a == b));
	if(a == b) throw runtime_error("ERROR: handlePopEvent(a==b)!");

	auto FoundA = lower_bound(lStatus.begin(),lStatus.end(),a,comp);
	auto FoundB = FoundA;

	if(a == *FoundA) {
		if(b == *(FoundA+1)) {
			FoundB = FoundA+1;
		} else if(b == *(FoundA-1)) {
			FoundA = FoundA-1;
			FoundB = FoundA+1;
		} else {
			cout << "Edge: " << a->eid << " ";
			throw runtime_error("ERROR: handlePopEvent b not at a+-1!");
		}
	} else if(!(a == *FoundA) && b == *FoundA) {
		FoundB = FoundA;
		if(a == *(FoundB+1)) {
			FoundB = FoundB+1;
			FoundA = FoundB-1;
		} else if(a == *(FoundB-1)) {
			FoundA = FoundB-1;
		} else {
			if(a == *find(lStatus.begin(),lStatus.end(),a)) {cout << "--found A with normal find...." << endl;}
			throw runtime_error("ERROR: handlePopEvent a not at b+-1!");
		}
	} else {
		throw runtime_error("ERROR: handlePopEvent a,b not found!");
	}

//	cout << "+"; fflush(stdout);

	if(!(*FoundA == lStatus.front())) {
		SweepItem beforeA(*(FoundA-1), *FoundB);
		if(beforeA.raysIntersect && beforeA.normalDistance > item.normalDistance) {
			eventQueue.insert(beforeA);
		}
	}

//	cout << "+"; fflush(stdout);

	if(!(*FoundB == lStatus.back()))  {
		SweepItem afterB(*FoundA, *(FoundB+1) );
		if(afterB.raysIntersect && afterB.normalDistance > item.normalDistance) {
			eventQueue.insert(afterB);
		}
	}

//	cout << "+" << endl; fflush(stdout);

	// swap line segments in status, as of the intersection point.
	iter_swap(FoundA, FoundB);
}


/*****************************************************************************/
/*                               SweepEvent                                  */
/*****************************************************************************/

//EventInfo SweepEvent::getEventType() {
//	SweepEventReturnContainer cont;
//	auto activeCells = getActivCells();
//	if(activeCells.size() > 0) {
//		/* edge, split or crate event */
//		int numBoundaryCells = 0;
//		int numInteriorCells = 0;
//		for(auto cell : activeCells) {
//			if(cell->isInteriorNode()) {++numInteriorCells; cont.interiorNodes.push_back(*cell); }
//			else {++numBoundaryCells; cont.boundaryNodes.push_back(*cell); }
//		}
//
//		if(numInteriorCells == 1 && numBoundaryCells == 0) {
//			/* merge or create event (2) */
//			return make_pair(EventType::CREATE2ORMERGE,cont);
//
//		} else if(numInteriorCells == 1 && numBoundaryCells == 2) {
//			/* split event */
//			return make_pair(EventType::SPLIT,cont);
//
//		} else if(numInteriorCells == 0 && numBoundaryCells == 3) {
//			/* edge event */
//			return make_pair(EventType::EDGE,cont);
//
//		} else if(numInteriorCells == 0 && numBoundaryCells == 2) {
//			/* create event (1) */
//			return make_pair(EventType::CREATE1ORENTER,cont);
//
//		} else if(numInteriorCells == 0 && numBoundaryCells == 1) {
//			/* merge event */
//			return make_pair(EventType::ENTER,cont);
//
//		} else {
//			cout << "BN: " << numBoundaryCells << ", IN: " << numInteriorCells << endl;
//			for(auto c : activeCells) {
//				c->print();
//			}
//			cout << endl;
//			//throw runtime_error("Not supported!");
//			return make_pair(EventType::EMPTY,cont);
//		}
//	}
//	// TODO: fix for non-general position!
//	//throw runtime_error("input not in general position, more than three events!");
//	return make_pair(EventType::EMPTY,cont);
//}


