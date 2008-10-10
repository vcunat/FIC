#ifndef KDTREE_HEADER_
#define KDTREE_HEADER_

#include "util.h"

namespace NOSPACE {
	using namespace std;
}

/** Routines for computing with T[length] number fields */
namespace FieldMath {

	template<class I1,class I2,class T>
	T transform2( I1 i1, I1 iEnd1, I2 i2, T transformer ) {
		for (; i1!=iEnd1; ++i1,++i2)
			transformer(*i1,*i2);
		return transformer;
	}

	template<class I1,class I2,class I3,class T>
	T transform3( I1 i1, I1 iEnd1, I2 i2, I3 i3, T transformer ) {
		for (; i1!=iEnd1; ++i1,++i2,++i3)
			transformer(*i1,*i2,*i3);
		return transformer;
	}

	template<class I1,class I2,class I3,class I4,class T>
	T transform4( I1 i1, I1 iEnd1, I2 i2, I3 i3, I4 i4, T transformer ) {
		for (; i1!=iEnd1; ++i1,++i2,++i3,++i4)
			transformer(*i1,*i2,*i3,*i4);
		return transformer;
	}

	/** Means b[i]=a[i]; */
	template<class T> T* assign(const T *a,int length,T *b) {
		copy(a,a+length,b);
		return b;
	}

	namespace NOSPACE {
		template<class T> struct MoveToBounds {
			T sqrError;

			MoveToBounds()
			: sqrError(0) {}

			void operator()(const T &point,const T &lower,const T &upper,T &result) {
				if (point<lower) {
					sqrError+=sqr(lower-point);
					result=lower;
				} else
				if (point>upper) {
					sqrError+=sqr(point-upper);
					result=upper;
				} else
					result=point;
			}
		};
	}
	/** Copy_moves a vector to the nearest point within the bounds
	 *	and returns SE (distance^2) */
	template<class T>
	T moveToBounds_copy(const T *point,const T **bounds,int length,T *result) {
		return transform4( point, point+length, bounds[0], bounds[1]
			, result, MoveToBounds<T>() ) .sqrError;
	}
//	namespace NOSPACE {
//		template<class T> struct Min {
//			const T& operator()(const T &a,const T &b) const
//				{ return min(a,b); }
//		};
//		template<class T> struct Max {
//			const T& operator()(const T &a,const T &b) const
//				{ return max(a,b); }
//		};
//	}
//	/** Means b[i]=min(a[i],b[i]) */
//	template<class T> T* min(const T *a,int length,T *b) {
//		transform( a, a+length, b, b, Min<T>() );
//		return b;
//	}
//	/** Means b[i]=max(a[i],b[i]) */
//	template<class T> T* max(const T *a,int length,T *b) {
//		transform( a, a+length, b, b, Max<T>() );
//		return b;
//	}

//	/** Returns whether a[i]<=b[i] */
//	template<class T> bool isOrdered(const T *a,const T *b,int length) {
//		const T *aEnd=a+length;
//		do {
//			if ( *a++ > *b++ )
//				return false;
//		} while (a!=aEnd);
//		return true;
//	}
	/*
	namespace NOSPACE {
		template<class T> struct Adder {
			void operator()(const T &a,const T &b,T &c)
				{ c=a+b; }
		};
		template<class T> struct Subtractor {
			void operator()(const T &a,const T &b,T &c)
				{ c=a-b; }
		};
	}
	template<class T> T* add(const T *a,const T *b,int length,T *c) {
		transform3( a, a+length, b, c, Adder<T>() );
	}
	template<class T> T* subtract(const T *a,const T *b,int length,T *c) {
		transform3( a, a+length, b, c, Subtractor<T>() );
	}
	*/
}

namespace NOSPACE {
	/** Expands a valid bounding box to contain a point (one coordinate at once) */
	template<class T> struct BoundsExpander {
		void operator()(const T &data,T &lower,T &upper) const {
			if (data<lower)
				lower=data; else
			if (data>upper)
				upper=data;
		}
	};
}
template<class T> void getBounds(const T *data,int length,int count,T **bounds) {
	using namespace FieldMath;
	assign(data,length,bounds[0]);
	assign(data,length,bounds[1]);
	while (--count) {
		data+=length;
		transform3( data, data+length, bounds[0], bounds[1], BoundsExpander<T>() );
	}
}
template<class T>
void getBounds(const T *data,int length,const int *beginIDs,const int *endIDs,T **bounds) {
	using namespace FieldMath;
	const T *begin= data + *beginIDs*length;
	assign( begin, length, bounds[0] );
	assign( begin, length, bounds[1] );
	while ( ++beginIDs != endIDs ) {
		begin= data + *beginIDs*length;
		transform3( begin, begin+length, bounds[0], bounds[1], BoundsExpander<T>() );
	}
}

namespace NOSPACE {
	/** Compares vectors (given by their indices) according to a given coordinate */
	template<class T> class IndexComparator {
		const T *data;
		int length;
	public:
		IndexComparator(const T *data_,int length_,int index_)
		: data(data_+index_), length(length_) {}
		bool operator()(int a,int b)
			{ return data[a*length] < data[b*length]; }
	};
}
/** A generic KD-tree */
template < class T, class Chooser
	= int (*)(int,int,/*const*/T**,const T*,int,const int*,const int*) >
class KDTree {
	struct Node {
		int coord;
		T threshold;
	};

public:
	const int depth
	, length ///	the length of the vectors
	, count; ///<	the count of the vectors
	const Chooser chooser;
private:
	Node *nodes;
	int *dataIDs;///<	data IDs for children of "leaf" nodes
	T *bounds[2];

public:
	KDTree(const T *data,int length_,int count_,Chooser coordChooser)
	: depth( log2ceil(count_) ), length(length_), count(count_), chooser(coordChooser)
	, nodes( new Node[count_] ), dataIDs( new int[count_] ) {
		assert( data && length>0 && count>1 );
	//	create the index-vector
		for (int i=0; i<count; ++i)
			dataIDs[i]= i;
	//	get the overall bounds
		bounds[0]= new T[length];
		bounds[1]= new T[length];
		getBounds(data,length,count,bounds);
	//	build the tree
		buildNode(data,1,dataIDs,dataIDs+count,depth);
	}
	~KDTree() {
	//	clean up
		delete[] nodes;
		delete[] bounds[0];
		delete[] bounds[1];
		delete[] dataIDs;
	}
	/** Takes an index of "leaf" (nonexistent) node, returns the appropriate data ID */
	int leafID2dataID(int leafID) const {
		assert( count<=leafID && leafID<2*count );
		int index= leafID-powers[depth];
		if (index<0)
			index+= count;
		assert( 0<=index && index<count );
		return dataIDs[index];
	}
private:
	void buildNode(const T *data,int nodeIndex,int *beginIDs,int *endIDs,int depthLeft) {
		assert( endIDs-beginIDs >= 2 );
	//
		int count= endIDs-beginIDs;
		--depthLeft;
		bool shallowRight= ( powers[depthLeft]+powers[depthLeft-1] > count );
		int *middle= shallowRight
			? endIDs-powers[depthLeft-1]
			: beginIDs+powers[depthLeft];
	//	find out the dividing coordinate and find the median in this coordinate
		int coord= chooser(depth,length,bounds,data,nodeIndex,beginIDs,endIDs);
		nth_element( beginIDs, middle , endIDs, IndexComparator<T>(data,length,coord) );
	//	fill the node's data (dividing coordinate and its threshold)
		nodes[nodeIndex].coord= coord;
		nodes[nodeIndex].threshold= data[*middle*length];
	//	recurse on both halves (if needed; fall-through switch)
		switch (count) {
		default: //	we've got enough nodes
			buildNode( data, 2*nodeIndex+1, middle, endIDs, depthLeft-shallowRight );
		case 3: //	only a pair in the first half
			buildNode( data, 2*nodeIndex, beginIDs, middle, depthLeft );
		case 2: //	nothing needs to be sorted
			;
		}
	}
public:
	/**  */
	class PointHeap {
	// TODO (admin#2#): Incomplete range blocks searching
		struct HeapNode {
			int nodeIndex;
			T *data;

			HeapNode() {}
			HeapNode(int nodeIndex_,T *data_)
			: nodeIndex(nodeIndex_), data(data_) {}

			T& getSE()
				{ return *data; }
			const T& getSE() const
				{ return *data; }
			T* getNearest()
				{ return data+1; }
		};
		struct HeapOrder {
			bool operator()(const HeapNode &a,const HeapNode &b)
				{ return a.getSE() < b.getSE(); }
		};

		const KDTree &kd;
		const T* const point;
		vector<HeapNode> heap;
		BulkAllocator<T> allocator;
	public:
		PointHeap(const KDTree &tree,const T *point_)
		: kd(tree), point(point_) {
			assert(point);
		//	create the root heap-node
			T *data= allocator.makeField(kd.length+1);
			data[0]= FieldMath::moveToBounds_copy
			( point, const_cast<const T**>(kd.bounds), kd.length, data+1 );
		//	push it onto the heap (and reserve more to speed up the first leaf-getting)
			heap.reserve(kd.depth*2);
			heap.push_back(HeapNode(1,data));
		}

		bool empty()
			{ return heap.empty(); }

		/** Returns the SE of the top node (always equals the SE of the next leaf) */
		T getTopSE() {
			assert( !empty() );
			return heap[0].getSE();
		}

		/** Removes a leaf, returns the matching data's index */
		int popLeaf() {
			makeTopLeaf();

			int result= kd.leafID2dataID( heap.front().nodeIndex );

			pop_heap( heap.begin(), heap.end(), HeapOrder() );
			heap.pop_back();
			return result;
		}

		/** Divides the top nodes until there's a leaf on the top */
		void makeTopLeaf() {
			assert( !empty() );
			if ( !(heap[0].nodeIndex<kd.count) )
				return;
			size_t oldHeapSize= heap.size();
			HeapNode heapRoot= heap[0]; // making a local working copy
			do { // while ( heapRoot.nodeIndex<kd.count )
				const Node &node= kd.nodes[heapRoot.nodeIndex];
			//	create a new heap-node and allocate it's data
				HeapNode newHNode;
				newHNode.data= allocator.makeField(kd.length+1);
			//	the nearest point of the new heap-node only differs in one coordinate
				FieldMath::assign
				( heapRoot.getNearest(), kd.length, newHNode.getNearest() );
				newHNode.getNearest()[node.coord]= node.threshold;
			//	the SE of the new heap-node can be computed from the previous heap-node
				T oldDistance= point[node.coord] - heapRoot.getNearest()[node.coord]
				, newDistance= point[node.coord] - node.threshold;
				newHNode.getSE()= heapRoot.getSE() - sqr(oldDistance) + sqr(newDistance);
			//	rigthly assign the nodeIndex of the new heap node and increase ours
				heapRoot.nodeIndex*= 2;
				if ( newDistance <= 0 )
				//	the point is in the left part
					newHNode.nodeIndex= heapRoot.nodeIndex+1;
				else
				//	the point is in the right part
					newHNode.nodeIndex= heapRoot.nodeIndex++;
				heap.push_back(newHNode);
			} while ( heapRoot.nodeIndex<kd.count );
			heap[0]= heapRoot; // restoring the working copy
		//	restore the heap-property on the added nodes
			typename vector<HeapNode>::iterator it= heap.begin()+oldHeapSize;
			do {
				push_heap( heap.begin(), it, HeapOrder() );
			} while ( it++ != heap.end() );
		}
	}; // PointHeap class
}; // KDTree class

namespace KDCoordChoosers {

	namespace NOSPACE {
		template<class T> struct MaxDiffCoord {
			T maxDiff;
			int best,next;

			MaxDiffCoord(const T &diff0)
			: maxDiff(diff0), best(0), next(1) {}

			void operator()(const T &low,const T &high) {
				T diff= high-low;
				if (diff>maxDiff) {
					best= next;
					maxDiff= diff;
				}
				++next;
			}
		};
	}
	template<class T>
	int boundBoxLongest( int /*depth*/, int length, T **bounds, const T *data
	, int nodeIndex, const int *beginIDs, const int *endIDs ) {
		assert( length>0 && bounds && data
		&& nodeIndex>0 && beginIDs && endIDs && beginIDs<endIDs );
	//	temporary storage for computed bounding box
		T lower[length], upper[length];
		T* localBounds[2]= {lower,upper};
	//	if nodeIndex==1, we are in the root -> we can use already computed bounds
		if ( nodeIndex>1 ) {
			getBounds( data, length, beginIDs, endIDs, localBounds );
			bounds= localBounds;
		}
	//	find and return the longest coordinate
		MaxDiffCoord<T> mdc= FieldMath::transform2( localBounds[0]+1, localBounds[0]+length
		, localBounds[1]+1, MaxDiffCoord<T>( *localBounds[1] - *localBounds[0] ) );
		assert( mdc.next == length );
		return mdc.best;
	}
}

#endif // KDTREE_HEADER_
