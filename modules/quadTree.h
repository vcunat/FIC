#ifndef QUADTREE_HEADER_
#define QUADTREE_HEADER_

#include "../interfaces.h"

//	forwards from "fileUtils.h"
class BitWriter;
class BitReader;

/// \ingroup modules
/** Module dividing range blocks with a quad-tree.
 *	It can use heuristic dividing, minimum and maximum block level can be specified. */
class MQuadTree: public ISquareRanges {
	DECLARE_debugModule;

	DECLARE_TypeInfo( MQuadTree, "Quad-tree"
	, "Splits the range blocks according to the <b>quad-tree</b> algorithm"
	, {
		label:	"Min. range-block size",
		desc:	"Blocks with this size won't be divided",
		type:	settingInt(2,2,8,IntLog2)
	}, {
		label:	"Max. range-block size",
		desc:	"Blocks with bigger size will be always divided",
		type:	settingInt(2,12,12,IntLog2)
	}, {
		label:	"Heuristic dividing",
		desc:	"Pre-divide the ranges heuristically and later divide ranges\n"
				"with bad quality and try to merge ranges with good quality",
		type:	settingCombo("no\nyes",1)
	} )
	/// \todo better heuristics via changes in ISquareEncoder interface
	
protected:
	class Node;	// forward declaration, derived from RangeNode
	friend class Node;

protected:
	/** Indices for settings */
	enum Settings { MinLevel, MaxLevel, HeuristicAllowed };
//	Settings-retrieval methods
	int minLevel()			{ return settingsInt(MinLevel); }
	int maxLevel()			{ return settingsInt(MaxLevel); }
	bool heuristicAllowed()	{ return settingsInt(HeuristicAllowed); }

protected:
//	Module's data
	Node *root;						///< Quad-tree's root node
	std::vector<RangeNode*> fringe;	///< List of quad-tree's leaves
	int zoom;						///< The zoom
#ifndef NDEBUG /* things needed for showing debugging info */
	int badDivides, triedMerges, badTries;
	const PlaneBlock *planeBlock;
#endif

protected:
//	Construction and destruction
	/** Only zeroes #root */
	MQuadTree()
	: root(0), zoom(-1)
	#ifndef NDEBUG
	, badDivides(0), triedMerges(0), badTries(0), planeBlock(0)
	#endif
		{}
	/** Only deletes #root */
	~MQuadTree() { delete root; }
public:
/** \name ISquareRanges interface
 *	@{ */
	void encode(const PlaneBlock &toEncode);
	const RangeList& getRangeList() const
		{ return fringe; }

	/* The module doesn't need to preserve any settings */
	void writeSettings(std::ostream&) {}
	void readSettings(std::istream&) {}

	void writeData(std::ostream &file);
	void readData_buildRanges(std::istream &file,const PlaneBlock &block);
///	@}
protected:
	static float estimateSE(Real rSum,Real r2Sum,int pixCount,int level)
		{ return ldexp( r2Sum-sqr(rSum)/pixCount, -4 ) * level; }
protected:
	struct NodeExtremes;
	/** Class used for tree nodes in the quad-tree structure */
	class Node: public RangeNode {
		Node *father	///< Pointer to the father-node of this node in the tree
		, *brother		///< Pointer to the next brother-node of this node in the tree
		, *son;			///  Pointer to the first son-node of this node in the tree

		/** Constructor for son initialization - used in dividing method */
		Node( const Block &block, Node *father_, Node* brother_ )
		: RangeNode( block, father_->level-1 )
		, father(father_), brother(brother_), son(0) {}
		/** Disconnects itself from father and brothers */
		void disconnect();
	public:
		/** Constuctor for initializing a new root */
		Node(const Block &block)
		: RangeNode( block, log2ceil(std::max(block.width(),block.height())) )
		, father(0), brother(this), son(0) {}
		/** Only calls #deleteSons */
		~Node() { deleteSons(); }
		/** Only deletes the sons - they do the same recursively */
		void deleteSons();

		/** Divides the Range in four */
		void divide();
		/** Recursively walks the Ranges in the QuadTree according to a Hilbert curve
		 *	and fills the vector with pointers to leaves in this order */
		void getHilbertList(RangeList &list,char start=0,char clockwise=1);
		/** Counts the sons of this node and returns their number */
		int getSonCount() const;

		/** Encodes a range block (recursively), returns whether it was divided */
		bool encode(const PlaneBlock &toEncode);

		/** Saves sons into a stream (extremes contain the min.\ and max.\ block level) */
		void toFile(BitWriter &file,NodeExtremes extremes);
		/** Loads sons from a stream (extremes contain the min.\ and max.\ block level) */
		void fromFile(BitReader &file,NodeExtremes extremes);
	}; // MQuadTree::Node class
};

#endif // QUADTREE_HEADER_
