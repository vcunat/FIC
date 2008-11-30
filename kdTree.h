#ifndef KDTREE_HEADER_
#define KDTREE_HEADER_

#include "util.h"

namespace NOSPACE {
	using namespace std;
}

/** Routines for computing with T[length] number fields */
namespace FieldMath {
	/** Calls transformer(x,y) for every x from [i1..iEnd1) and corresponding y from [i2..) */
	template<class I1,class I2,class Transf>
	Transf transform2( I1 i1, I1 iEnd1, I2 i2, Transf transformer ) {
		for (; i1!=iEnd1; ++i1,++i2)
			transformer(*i1,*i2);
		return transformer;
	}
	/** Analogous to ::transform2 */
	template<class I1,class I2,class I3,class Transf>
	Transf transform3( I1 i1, I1 iEnd1, I2 i2, I3 i3, Transf transformer ) {
		for (; i1!=iEnd1; ++i1,++i2,++i3)
			transformer(*i1,*i2,*i3);
		return transformer;
	}
	/** Analogous to ::transform2 */
	template<class I1,class I2,class I3,class I4,class Transf>
	Transf transform4( I1 i1, I1 iEnd1, I2 i2, I3 i3, I4 i4, Transf transformer ) {
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
					sqrError+= sqr(lower-point);
					result= lower;
				} else
				if (point>upper) {
					sqrError+= sqr(point-upper);
					result= upper;
				} else
					result= point;
			}
		};
	}
	/** Copy_moves a vector (\p point) to the nearest point (\p result)
	 *	within bounds (\p bounds) and returns SE (distance^2) */
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
				lower= data; else
			if (data>upper)
				upper= data;
		}
	};
}
/** Computes the bounding box of \p count vectors with length \p length stored in \p data.
 *	The vectors in \p data are stored linearly, bounds[0] and bounds[1] sould be preallocated */
template<class T> void getBounds(const T *data,int length,int count,T (*bounds)[2]) {
	using namespace FieldMath;
	assert(length>0);
//	make the initial bounds only contain the first point
	assign(data,length,bounds[0]);
	assign(data,length,bounds[1]);
//	expand the bounds by every point (except for the first one)
	while (--count) {
		data+= length;
		transform3( data, data+length, bounds[0], bounds[1], BoundsExpander<T>() );
	}
}
/** Like ::getBounds, but it only works on vectors with indices from [\p beginIDs..\p endIDs)
 *	instead of [0..\p count) */
template<class T>
void getBounds(const T *data,int length,const int *beginIDs,const int *endIDs,T **bounds) {
	using namespace FieldMath;
	assert(endIDs>beginIDs);
//	make the initial bounds only contain the first point
	const T *begin= data + *beginIDs*length;
	assign( begin, length, bounds[0] );
	assign( begin, length, bounds[1] );
//	expand the bounds by every point (except for the first one)
	while ( ++beginIDs != endIDs ) {
		begin= data + *beginIDs*length;
		transform3( begin, begin+length, bounds[0], bounds[1], BoundsExpander<T>() );
	}
}

/** A generic KD-tree */
template < class T, class Chooser
	= int (*)(int,int,const T**,const T*,int,const int*,const int*) >
class KDTree {
	/** Represents one node of the KD-tree */
	struct Node {
		int coord;	///< The coordinate along which the tree splits in this node
		T threshold;///< The threshold of the split (left[#coord] <= #threshold <= right[#coord])
	};
	typedef T (*Bounds)[2];

public:
	const int depth			///  The depth of the tree = ::log2ceil(#count)
	, length				///	 The length of the vectors
	, count;				///< The number of the vectors
	const Chooser chooser;	///< The best-to-split-at coordinate chooser
private:
	Node *nodes;	///< The array of the tree-nodes
	int *dataIDs;	///< Data IDs for children of "leaf" nodes
	Bounds bounds;	///< The bounding box for all the data

public:
//	Builds a new KD-tree from \p count_ vectors of \p length_ elements stored in \data
	KDTree(const T *data,int length_,int count_,Chooser coordChooser)
	: depth( log2ceil(count_) ), length(length_), count(count_), chooser(coordChooser)
	, nodes( new Node[count_] ), dataIDs( new int[count_] ) {
		assert( data && length>0 && count>1 );
	//	create the index-vector
		for (int i=0; i<count; ++i)
			dataIDs[i]= i;
	//	get the overall bounds
		bounds= new T[length][2];
		getBounds(data,length,count,bounds);
	//	build the tree
		buildNode(data,1,dataIDs,dataIDs+count,depth);
	}
	/** Only frees the memory */
	~KDTree() {
	//	clean up
		delete[] nodes;
		delete[] bounds;
		delete[] dataIDs;
	}
	/** Takes an index of "leaf" (nonexistent) node and returns the appropriate data ID */
	int leafID2dataID(int leafID) const {
		assert( count<=leafID && leafID<2*count );
		int index= leafID-powers[depth];
		if (index<0)
			index+= count;
		assert( 0<=index && index<count );
		return dataIDs[index];
	}
private:
	/** Compares vectors (given by their indices) according to a given coordinate */
	class IndexComparator {
		const T *data;
		int length;
	public:
		IndexComparator(const T *data_,int length_,int index_)
		: data(data_+index_), length(length_) {}
		bool operator()(int a,int b)
			{ return data[a*length] < data[b*length]; }
	};
	/** Recursively builds node \p nodeIndex and its subtree of depth \p depthLeft
	 *	(including leaves), operates on data \p data in the range [\p beginIDs,\p endIDs) */
	void buildNode(const T *data,int nodeIndex,int *beginIDs,int *endIDs,int depthLeft) {
		int count= endIDs-beginIDs;
	//	check we've got at least one vector and the depth&count are adequate to each other
		assert( count>=2 && powers[depthLeft-1]<count && count<=powers[depthLeft] );
		--depthLeft;
		bool shallowRight= ( count <= powers[depthLeft]+powers[depthLeft-1] );
		int *middle= shallowRight
			? endIDs-powers[depthLeft-1]
			: beginIDs+powers[depthLeft];
	//	find out the dividing coordinate and find the median in this coordinate
		int coord= chooser(depthLeft,length,bogoCast(bounds),data,nodeIndex,beginIDs,endIDs);
		nth_element( beginIDs, middle , endIDs, IndexComparator(data,length,coord) );
	//	fill the node's data (dividing coordinate and its threshold)
		nodes[nodeIndex].coord= coord;
		nodes[nodeIndex].threshold= data[*middle*length+coord];
	//	recurse on both halves (if needed; fall-through switch)
		switch (count) {
		default: //	we've got enough nodes - build both subtrees (fall through)
		//	build the right subtree
			buildNode( data, 2*nodeIndex+1, middle, endIDs, depthLeft-shallowRight );
		case 3: // only a pair in the first half
		//	build the left subtree
			buildNode( data, 2*nodeIndex, beginIDs, middle, depthLeft );
		case 2: // nothing needs to be sorted
			;
		}
	}
public:
	/** Manages a heap from nodes of a KDTree.
	 *	It returns vectors (their indices) in the order according to distance (SE)
	 *	from a given fixed point. It can return a lower bound of the SEs of the remaining
	 *	vectors at any time. */
	class PointHeap {
	// TODO (admin#2#): Incomplete range blocks searching
		/** One element of the heap representing a node in the KDTree */
		struct HeapNode {
			int nodeIndex;	///< Index of the node in #kd
			T *data;		///< Stores the SE and the coordinates of the nearest point

			/** No-init constructor */
			HeapNode() {}
			/** Initializes the members from the parameters */
			HeapNode(int nodeIndex_,T *data_)
			: nodeIndex(nodeIndex_), data(data_) {}

			/** Returns reference to the SE of the nearest point to this node's bounding box */
			/** Returns reference to the SE of the nearest point to this node's bounding box */
			T& getSE()
				{ return *data; }
			const T& getSE() const
				{ return *data; }
			/** Returns pointer to the nearest point to this node's bounding box */
			T* getNearest()
				{ return data+1; }
		};
		/** Defines the order of #heap - ascending according tho #getSE */
		struct HeapOrder {
			bool operator()(const HeapNode &a,const HeapNode &b)
				{ return a.getSE() > b.getSE(); }
		};

		const KDTree &kd;			///< Reference to the KDTree we operate on
		const T* const point;		///< Pointer to the point we are trying to approach
		vector<HeapNode> heap;		///< The current heap of the tree nodes
		BulkAllocator<T> allocator;	///< The allocator for HeapNode::data
	public:
		/** Build the heap from the KDTree \p tree and vector \p point_
		 *	(they've got to remain valid until destruction) */
		PointHeap(const KDTree &tree,const T *point_)
		: kd(tree), point(point_) {
			assert(point);
		//	create the root heap-node
			HeapNode rootNode( 1, allocator.makeField(kd.length+1) );
		//	compute the nearest point within the global bounds and corresponding SE
			rootNode.getSE()= FieldMath::moveToBounds_copy
			( point, const_cast<const T**>(kd.bounds), kd.length, rootNode.getNearest() );
		//	push it onto the heap (and reserve more to speed up the first leaf-gettings)
			heap.reserve(kd.depth*2);
			heap.push_back(rootNode);
		}

		/** Returns whether the heap is empty ( !isEmpty() is needed for all other methods) */
		bool isEmpty()
			{ return heap.empty(); }

		/** Returns the SE of the top node (always equals the SE of the next leaf) */
		T getTopSE() {
			assert( !isEmpty() );
			return heap[0].getSE();
		}

		/** Removes a leaf, returns the matching vector's index */
		int popLeaf() {
		//	ensure a leaf is on the top of the heap and get its ID
			makeTopLeaf();
			int result= kd.leafID2dataID( heap.front().nodeIndex );
		//	remove the top from the heap (no need to free the memory - #allocator)
			pop_heap( heap.begin(), heap.end(), HeapOrder() );
			heap.pop_back();
			return result;
		}

	private:
	private:
		/** Divides the top nodes until there's a leaf on the top */
		void makeTopLeaf() {
			assert( !isEmpty() );
		//	exit if there's a leaf on the top already
			if ( !(heap[0].nodeIndex<kd.count) )
				return;
			size_t oldHeapSize= heap.size();
			HeapNode heapRoot= heap[0]; // making a local working copy of the top of the heap

			do { // while ( heapRoot.nodeIndex<kd.count ) ... while heapRoot isn't leaf
				const Node &node= kd.nodes[heapRoot.nodeIndex];
			//	now replace the node with its two children:
			//		one of them will have the same SE (replaces its parent on the top),
			//		the other one can have higher SE (push_back-ed on the heap)

			//	create a new heap-node and allocate it's data
				HeapNode newHNode;
				newHNode.data= allocator.makeField(kd.length+1);
			//	the nearest point of the new heap-node only differs in one coordinate
				FieldMath::assign( heapRoot.getNearest(), kd.length, newHNode.getNearest() );
				newHNode.getNearest()[node.coord]= node.threshold;

			//	the SE of the child heap-nodes can be computed from the parent heap-node
				Real oldDistance= Real(point[node.coord]) - heapRoot.getNearest()[node.coord];
				Real newDistance= Real(point[node.coord]) - node.threshold;
				newHNode.getSE()= heapRoot.getSE() - sqr(oldDistance) + sqr(newDistance);
			/*	another way to do this, according to: A^2-B^2 = (A-B)*(A+B)
				Real nearestInCoord= heapRoot.getNearest()[node.coord];
				newHNode.getSE()= heapRoot.getSE() + (nearestInCoord-node.threshold)
					* ( ldexp((Real)point[node.coord],1) - nearestInCoord - node.threshold );
			*/

			//	correctly assign the nodeIndex of the new children
				newHNode.nodeIndex= (heapRoot.nodeIndex*= 2);
				if ( newDistance <= 0 )
				//	the point is in the left part -> the left child will be on the top
					++newHNode.nodeIndex;
				else
				//	the point is in the right part -> the right child will be on the top
					++heapRoot.nodeIndex;
				heap.push_back(newHNode);
			} while ( heapRoot.nodeIndex<kd.count );

			heap[0]= heapRoot; // restoring the working copy of the heap's top node
		//	restore the heap-property on the added nodes
			typename vector<HeapNode>::iterator it= heap.begin()+oldHeapSize;
			do
				push_heap( heap.begin(), it, HeapOrder() );
			while ( it++ != heap.end() );
		} // makeTopLeaf() method

	}; // PointHeap class
}; // KDTree class

namespace KDCoordChoosers {

	namespace NOSPACE {
		template<class T> struct MaxDiffCoord {
			T maxDiff;
			int best, next;

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
	/** Conforms to KDTree<T>::Chooser - chooses the longest coordinate of the real bounding box */
	template<class T>
	int boundBoxLongest( int /*depthLeft*/, int length, const T **bounds, const T *data
	, int nodeIndex, const int *beginIDs, const int *endIDs ) {
		assert( length>0 && bounds && data
		&& nodeIndex>0 && beginIDs && endIDs && beginIDs<endIDs );
	//	temporary storage for computed bounding box
		T lowerBounds[length], upperBounds[length];
		const T *lowerB= lowerBounds, *upperB= upperBounds;

		if ( nodeIndex>1 ) { // compute the bounding box
			T* localBounds[2]= {lowerBounds,upperBounds};
			getBounds( data, length, beginIDs, endIDs, localBounds );
		} else { // we are in the root -> we can use already computed bounds
			lowerB= bounds[0];
			upperB= bounds[1];
		}
	//	find and return the longest coordinate
		MaxDiffCoord<T> mdc= FieldMath
		::transform2( lowerB+1, lowerB+length, upperB+1, MaxDiffCoord<T>(*upperB-*lowerB) );
		assert( mdc.next == length );
		return mdc.best;
	}
}

#endif // KDTREE_HEADER_
