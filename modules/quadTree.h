#ifndef QUADTREE_HEADER_
#define QUADTREE_HEADER_

#include "../interfaces.h"

//	forwards from "fileUtils.h"
class BitWriter;
class BitReader;

/** Module dividing range blocks with a quad-tree, can use heuristic dividing */
class MQuadTree: public ISquareRanges {
	DECLARE_debugModule;

	DECLARE_M_cloning_name_desc( MQuadTree, "Quad-tree"
	, "Splits the range blocks according to the <b>quad-tree</b> algorithm" )

	DECLARE_M_settings_type({
		type:	IntLog2,
		data: {	i: {2,8} },
		label:	"Min. range-block size",
		desc:	"Blocks with this size won't be divided"
	}, {
		type:	IntLog2,
		data: {	i: {2,12} },
		label:	"Max. range-block size",
		desc:	"Blocks with bigger size will be always divided"
	}, {
		type:	Combo,
		data: {	text: "no\nyes" },
		label:	"Heuristic dividing",
		desc:	"Pre-divide the ranges heuristically and later divide ranges\n"
				"with bad quality and try to merge ranges with good quality"
	})

	DECLARE_M_settings_default(
		2,	// min. block-size
		12,	// max. block-size
		1	// heuristic dividing
	)
private:
	/** Indices for settings */
	enum Settings { MinLevel, MaxLevel, HeuristicAllowed };
//	Settings-retrieval methods
	int minLevel()			{ return settings[MinLevel].i; }
	int maxLevel()			{ return settings[MaxLevel].i; }
	bool heuristicAllowed()	{ return settings[HeuristicAllowed].i; }
protected:
	class Node;	// forward declaration, derived from RangeNode
	friend class Node;
private:
//	Module's data
	Node *root;						///< Quad-tree's root node
	std::vector<RangeNode*> fringe;	///< List of quad-tree's leaves
#ifndef NDEBUG
	int badDivides, triedMerges, badTries;
#endif
	
protected:
//	Construction and destruction
	/** Only zeroes #root */
	MQuadTree()
	: root(0)
	#ifndef NDEBUG
	, badDivides(0), triedMerges(0), badTries(0)
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
	void readData_buildRanges(std::istream &file,const Block &block);
///	@}
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
	};
};

#endif // QUADTREE_HEADER_
