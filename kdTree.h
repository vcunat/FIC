#ifndef KDTREE_HEADER_
#define KDTREE_HEADER_

#include "util.h"

namespace NOSPACE {
	using namespace std;
}

/** Routines for computing with T[length] number fields */
namespace FieldMath {
	/** Calls transformer(x,y) for every x from [i1;iEnd1) and corresponding y from [i2;?) */
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

	/** Means b[i]=a[i]; Only meant for POD types */
	template<class T> T* assign(const T *a,int length,T *b) {
	#ifndef NDEBUG
		copy( a, a+length, b );				// debugging version uses STL's copy
	#else
		memcpy( b, a, length*sizeof(T) );	// release version uses C's memory-copy (faster)
	#endif
		return b;
	}

	namespace NOSPACE {
		template<class T,bool CheckNaNs> struct MoveToBounds {
			T sqrError;

			MoveToBounds()
			: sqrError(0) {}

			void operator()(const T &point,const T bounds[2],T &result) {
				if ( CheckNaNs && isNaN(point) )
					return;
				if ( point < bounds[0] ) {
					sqrError+= sqr(bounds[0]-point);
					result= bounds[0];
				} else
				if ( point > bounds[1] ) {
					sqrError+= sqr(point-bounds[1]);
					result= bounds[1];
				} else
					result= point;
			}
		};
	}
	/** Copy_moves a vector (\p point) to the nearest point (\p result)
	 *	within bounds (\p bounds) and returns SE (distance^2) */
	template<class T,bool CheckNaNs>
	T moveToBounds_copy(const T *point,const T (*bounds)[2],int length,T *result) {
		return transform3( point, point+length, bounds, result
			, MoveToBounds<T,CheckNaNs>() ) .sqrError;
	}
}

template<class T> class KDBuilder;
/** A generic KD-tree */
template<class T> class KDTree {
public:
	friend class KDBuilder<T>;
	typedef KDBuilder<T> Builder;
protected:
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
protected:
	Node *nodes;	///< The array of the tree-nodes
	int *dataIDs;	///< Data IDs for children of "leaf" nodes
	Bounds bounds;	///< The bounding box for all the data

	/** Builds a new KD-tree from \p count_ vectors of \p length_ elements stored in \data */
	KDTree(int length_,int count_)
	: depth( log2ceil(count_) ), length(length_), count(count_)
	, nodes( new Node[count_] ), dataIDs( new int[count_] ), bounds( new T[length_][2] ) {
	}

	/** Copy constructor with swapping (or rather moving) semantics */
	KDTree(KDTree &other)
	: depth(other.depth), length(other.length), count(other.count)
	, nodes(other.nodes), dataIDs(other.dataIDs), bounds(other.bounds) {
		other.nodes= 0;
		other.dataIDs= 0;
		other.bounds= 0;
	}

public:
	/** Only frees the memory */
	~KDTree() {
	//	clean up
		delete[] nodes;
		delete[] dataIDs;
		delete[] bounds;
	}
	/** Takes an index of "leaf" (nonexistent) node and returns the appropriate data ID */
	int leafID2dataID(int leafID) const {
		ASSERT( count<=leafID && leafID<2*count );
		int index= leafID-powers[depth];
		if (index<0)
			index+= count;
		ASSERT( 0<=index && index<count );
		return dataIDs[index];
	}

	/** Manages a heap from nodes of a KDTree.
	 *	It returns vectors (their indices) in the order according to distance (SE)
	 *	from a given fixed point. It can return a lower bound of the SEs of the remaining
	 *	vectors at any time. */
	class PointHeap {
		/** One element of the heap representing a node in the KDTree */
		struct HeapNode {
			int nodeIndex;	///< Index of the node in #kd
			T *data;		///< Stores the SE and the coordinates of the nearest point

			/** No-init constructor */
			HeapNode() {}
			/** Initializes the members from the parameters */
			HeapNode(int nodeIndex_,T *data_): nodeIndex(nodeIndex_), data(data_) {}

			/** Returns reference to the SE of the nearest point to this node's bounding box */
			T& getSE()			{ return *data; }
			/** Returns the SE of the nearest point to this node's bounding box */
			T getSE() const		{ return *data; }
			/** Returns pointer to the nearest point to this node's bounding box */
			T* getNearest()		{ return data+1; }
		};
		/** Defines the order of #heap - ascending according to #getSE */
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
		PointHeap(const KDTree &tree,const T *point_,bool checkNaNs)
		: kd(tree), point(point_) {
			ASSERT(point);
		//	create the root heap-node
			HeapNode rootNode( 1, allocator.makeField(kd.length+1) );
		//	compute the nearest point within the global bounds and corresponding SE
			using namespace FieldMath;
			rootNode.getSE()= checkNaNs
				? moveToBounds_copy<T,true> ( point, kd.bounds, kd.length, rootNode.getNearest() )
				: moveToBounds_copy<T,false>( point, kd.bounds, kd.length, rootNode.getNearest() );
		//	push it onto the heap (and reserve more to speed up the first leaf-gettings)
			heap.reserve(kd.depth*2);
			heap.push_back(rootNode);
		}

		/** Returns whether the heap is empty ( !isEmpty() is needed for all other methods) */
		bool isEmpty()
			{ return heap.empty(); }

		/** Returns the SE of the top node (always equals the SE of the next leaf) */
		T getTopSE() {
			ASSERT( !isEmpty() );
			return heap[0].getSE();
		}

		/** Removes a leaf, returns the matching vector's index */
		template<bool CheckNaNs> int popLeaf() {
		//	ensure a leaf is on the top of the heap and get its ID
			makeTopLeaf<CheckNaNs>();
			int result= kd.leafID2dataID( heap.front().nodeIndex );
		//	remove the top from the heap (no need to free the memory - #allocator)
			pop_heap( heap.begin(), heap.end(), HeapOrder() );
			heap.pop_back();
			return result;
		}
	protected:
		/** Divides the top nodes until there's a leaf on the top */
		template<bool CheckNaNs> void makeTopLeaf();

	}; // PointHeap class
}; // KDTree class


template<class T> template<bool CheckNaNs>
void KDTree<T>::PointHeap::makeTopLeaf() {
	ASSERT( !isEmpty() );
//	exit if there's a leaf on the top already
	if ( !(heap[0].nodeIndex<kd.count) )
		return;
	PtrInt oldHeapSize= heap.size();
	HeapNode heapRoot= heap[0]; // making a local working copy of the top of the heap

	do { // while heapRoot isn't leaf ... while ( heapRoot.nodeIndex<kd.count )
		const Node &node= kd.nodes[heapRoot.nodeIndex];
	//	now replace the node with its two children:
	//		one of them will have the same SE (replaces its parent on the top),
	//		the other one can have higher SE (push_back-ed on the heap)

	//	create a new heap-node and allocate it's data
		HeapNode newHNode;
		newHNode.data= allocator.makeField(kd.length+1);
	//	assign index of the left child for both the heap-nodes (for now)
		newHNode.nodeIndex= (heapRoot.nodeIndex*= 2);
	//	the nearest point of the new heap-node only differs in one coordinate
		FieldMath::assign( heapRoot.getNearest(), kd.length, newHNode.getNearest() );

		if ( !CheckNaNs || !isNaN(point[node.coord]) ) { // valid coordinate -> normal processing
			newHNode.getNearest()[node.coord]= node.threshold;

		//	the SE of the child heap-nodes can be computed from the parent heap-node
			Real oldDistance= Real(point[node.coord]) - heapRoot.getNearest()[node.coord];
			Real newDistance= Real(point[node.coord]) - node.threshold;
			newHNode.getSE()= heapRoot.getSE() - sqr(oldDistance) + sqr(newDistance);
			ASSERT( newHNode.getSE() >= heapRoot.getSE() );
		/*	another way to do this, according to: A^2-B^2 = (A-B)*(A+B)
			Real nearestInCoord= heapRoot.getNearest()[node.coord];
			newHNode.getSE()= heapRoot.getSE() + (nearestInCoord-node.threshold)
				* ( ldexp((Real)point[node.coord],1) - nearestInCoord - node.threshold );
		*/
		//	correct the nodeIndex of the new children
			if ( newDistance <= 0 )
			//	the point is in the left part -> the left child will be on the top
				++newHNode.nodeIndex;
			else
			//	the point is in the right part -> the right child will be on the top
				++heapRoot.nodeIndex;

		} else  { // the coordinate doesn't have any effect
			newHNode.getSE()= heapRoot.getSE();
			++newHNode.nodeIndex;
		}

	//	add the new node to the back, restore the heap-property later
		heap.push_back(newHNode);
	} while ( heapRoot.nodeIndex<kd.count );

	heap[0]= heapRoot; // restoring the working copy of the heap's top node
//	restore the heap-property on the added nodes
	typename vector<HeapNode>::iterator it= heap.begin()+oldHeapSize;
	do
		push_heap( heap.begin(), it, HeapOrder() );
	while ( it++ != heap.end() );
} // KDTree<T>::PointHeap::makeTopLeaf<CheckNaNs>() method


template<class T> class KDBuilder: public KDTree<T> {
public:
	typedef KDTree<T> Tree;
	typedef T BoundsPair[2];
	typedef typename Tree::Bounds Bounds;
	typedef int (KDBuilder::*CoordChooser)
		(int nodeIndex,int *beginIDs,int *endIDs,int depthLeft) const;
protected:
	using Tree::depth;	using Tree::length;		using Tree::count;
	using Tree::nodes;	using Tree::dataIDs;	using Tree::bounds;

	const T *data;
	const CoordChooser chooser;
	mutable Bounds chooserTmp;

	KDBuilder(const T *data_,int length,int count,CoordChooser chooser_)
	: Tree(length,count), data(data_), chooser(chooser_), chooserTmp(0) {
		ASSERT( length>0 && count>0 && chooser && data );
	//	create the index-vector, coumpute the bounding box, build the tree
		for (int i=0; i<count; ++i)
			dataIDs[i]= i;
		getBounds(bounds);
		if (count>1)
			buildNode(1,dataIDs,dataIDs+count,depth);
		delete[] chooserTmp;
		DEBUG_ONLY( chooserTmp= 0; )
	}

	/** Creates bounds containing one value */
	struct NewBounds {
		void operator()(const T &val,BoundsPair &bounds) const
			{ bounds[0]= bounds[1]= val; }
	};
	/** Expands a valid bounding box to contain a point (one coordinate at once) */
	struct BoundsExpander {
		void operator()(const T &val,BoundsPair &bounds) const {
			if ( val < bounds[0] ) // lower bound
				bounds[0]= val; else
			if ( val > bounds[1] ) // upper bound
				bounds[1]= val;
		}
	};
	/** Computes the bounding box of #count vectors with length #length stored in #data.
	 *	The vectors in #data are stored linearly, /p boundsRes should be preallocated */
	void getBounds(Bounds boundsRes) const {
		using namespace FieldMath;
		ASSERT(length>0);
	//	make the initial bounds only contain the first point
		transform2(data,data+length,boundsRes,NewBounds());
		int count= Tree::count;
		const T *nowData= data;
	//	expand the bounds by every point (except for the first one)
		while (--count) {
			nowData+= length;
			transform2( nowData, nowData+length, boundsRes, BoundsExpander() );
		}
	}
	/** Like #getBounds, but it only works on vectors with indices from [\p beginIDs;\p endIDs)
	 *	instead of [0;#count), \p boundsRes should be preallocated to store the result */
	void getBounds(const int *beginIDs,const int *endIDs,Bounds boundsRes) const {
		using namespace FieldMath;
		ASSERT(endIDs>beginIDs);
	//	make the initial bounds only contain the first point
		const T *begin= data + *beginIDs*length;
		transform2(begin,begin+length,boundsRes,NewBounds());
	//	expand the bounds by every point (except for the first one)
		while ( ++beginIDs != endIDs ) {
			begin= data + *beginIDs*length;
			transform2( begin, begin+length, boundsRes, BoundsExpander() );
		}
	}

	/** Recursively builds node \p nodeIndex and its subtree of depth \p depthLeft
 	*	(including leaves), operates on data \p data in the range [\p beginIDs,\p endIDs) */
	void buildNode(int nodeIndex,int *beginIDs,int *endIDs,int depthLeft);

public:
	/** CoordChooser choosing the longest coordinate of the bounding box of the current interval */
	int choosePrecise(int nodeIndex,int *beginIDs,int *endIDs,int /*depthLeft*/) const;
	/** CoordChooser choosing the coordinate only according to the depth */
	int chooseFast(int /*nodeIndex*/,int* /*beginIDs*/,int* /*endIDs*/,int depthLeft) const
		{ return depthLeft%length; }
	/** CoordChooser choosing a random coordinate */
	int chooseRand(int /*nodeIndex*/,int* /*beginIDs*/,int* /*endIDs*/,int /*depthLeft*/) const
		{ return rand()%length; }
	int chooseApprox(int nodeIndex,int* /*beginIDs*/,int* /*endIDs*/,int depthLeft) const;

	static Tree* makeTree(const T *data,int length,int count,CoordChooser chooser) {
		KDBuilder builder(data,length,count,chooser);
		return new Tree(builder);
	}
}; // KDBuilder class



namespace NOSPACE {
	/** Finds the longest coordinate of a bounding box */
	template<class T> struct MaxDiffCoord {
		typedef typename KDBuilder<T>::BoundsPair BoundsPair;

		T maxDiff;
		int bestIndex, nextIndex;

		MaxDiffCoord(const BoundsPair& bounds0)
		: maxDiff(bounds0[1]-bounds0[0]), bestIndex(0), nextIndex(1) {}

		void operator()(const BoundsPair& bounds_i) {
			T diff= bounds_i[1]-bounds_i[0];
			if (diff>maxDiff) {
				bestIndex= nextIndex;
				maxDiff= diff;
			}
			++nextIndex;
		}
	};
}
template<class T> int KDBuilder<T>
::choosePrecise(int nodeIndex,int *beginIDs,int *endIDs,int) const {
	ASSERT( nodeIndex>0 && beginIDs && endIDs && beginIDs<endIDs );
//	temporary storage for computed bounding box
	BoundsPair boundsStorage[length];
	const BoundsPair *localBounds;

	if ( nodeIndex>1 ) { // compute the bounding box
		localBounds= boundsStorage;
		getBounds( beginIDs, endIDs, boundsStorage );
	} else // we are in the root -> we can use already computed bounds
		localBounds= bounds;
//	find and return the longest coordinate
	MaxDiffCoord<T> mdc= for_each
		( localBounds+1, localBounds+length, MaxDiffCoord<T>(localBounds[0]) );
	ASSERT( mdc.nextIndex == length );
	return mdc.bestIndex;
}
template<class T> int KDBuilder<T>::chooseApprox(int nodeIndex,int*,int*,int) const {
	using namespace FieldMath;
	ASSERT(nodeIndex>0);

	int myDepth= log2ceil(nodeIndex+1)-1;
	if (!myDepth) { // I'm in the root - copy the bounds
		ASSERT(nodeIndex==1);
		chooserTmp= new BoundsPair[length*(depth+1)]; // allocate my temporary bound-array
		assign( bounds, length, chooserTmp );
	}

	Bounds myBounds= chooserTmp+length*myDepth;
	if (myDepth) { // I'm not the root - copy parent's bounds and modify them
		const typename Tree::Node &parent= nodes[nodeIndex/2];
		Bounds parentBounds= myBounds-length;
		if (nodeIndex%2) {	// I'm the right son -> bounds on this level not initialized
			assign( parentBounds, length, myBounds );
			myBounds[parent.coord][0]= parent.threshold; // adjusting the lower bound
		} else // I'm the left son
			if ( nodeIndex+1 < count ) { // almost the same as brother -> only adjust the coordinate
				myBounds[parent.coord][0]= parentBounds[parent.coord][0];
				myBounds[parent.coord][1]= parent.threshold;
			} else { // I've got no brother
				ASSERT( nodeIndex+1 == count );
				assign( parentBounds, length, myBounds );
				myBounds[parent.coord][1]= parent.threshold;
			}
	}
//	find out the widest dimension
	MaxDiffCoord<T> mdc= for_each( myBounds+1, myBounds+length, MaxDiffCoord<T>(myBounds[0]) );
	ASSERT( mdc.nextIndex == length );
	return mdc.bestIndex;
}

namespace NOSPACE {
	/** Compares vectors (given by their indices) according to a given coordinate */
	template<class T> class IndexComparator {
		const T *data;
		int length;
	public:
		IndexComparator(const T *data_,int length_,int index_)
		: data(data_+index_), length(length_) {}
		bool operator()(int a,int b) const
			{ return data[a*length] < data[b*length]; }
	};
}
template<class T> void KDBuilder<T>
::buildNode(int nodeIndex,int *beginIDs,int *endIDs,int depthLeft) {
	int count= endIDs-beginIDs; // owershadowing Tree::count
//	check we've got at least one vector and the depth&count are adequate to each other
	ASSERT( count>=2 && powers[depthLeft-1]<count && count<=powers[depthLeft] );
	--depthLeft;
	bool shallowRight= ( count <= powers[depthLeft]+powers[depthLeft-1] );
	int *middle= shallowRight
		? endIDs-powers[depthLeft-1]
		: beginIDs+powers[depthLeft];
//	find out the dividing coordinate and find the median in this coordinate
	int coord= (this->*chooser)(nodeIndex,beginIDs,endIDs,depthLeft);
	nth_element( beginIDs, middle , endIDs, IndexComparator<T>(data,length,coord) );
//	fill the node's data (dividing coordinate and its threshold)
	nodes[nodeIndex].coord= coord;
	nodes[nodeIndex].threshold= data[*middle*length+coord];
//	recurse on both halves (if needed; fall-through switch)
	switch (count) {
	default: //	we've got enough nodes - build both subtrees (fall through)
	//	build the right subtree
		buildNode( 2*nodeIndex+1, middle, endIDs, depthLeft-shallowRight );
	case 3: // only a pair in the first half
	//	build the left subtree
		buildNode( 2*nodeIndex, beginIDs, middle, depthLeft );
	case 2: // nothing needs to be sorted
		;
	}
}

#endif // KDTREE_HEADER_
