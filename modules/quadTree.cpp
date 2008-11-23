#include "quadTree.h"
#include "../fileUtil.h"

using namespace std;

/** Struct for computing (acts as a functor) and storing max.\ and min.\
 *	levels of leaf range blocks */
struct MQuadTree::NodeExtremes {
	int min, max;

	NodeExtremes()
	: min( numeric_limits<int>::max() ), max( numeric_limits<int>::min() ) {}
	void operator()(const MQuadTree::RangeNode *node) {
		int now= node->level;
		if (now<min)
			min= now;
		if (now>max)
			max= now;
	}
};


void MQuadTree::encode(const PlaneBlock &toEncode) {
	assert( !root && fringe.empty() && toEncode.ranges==this && toEncode.isReady() );
	zoom= 0;
//	if allowed, prepare accelerators for heuristic dividing
	if ( heuristicAllowed() )
		toEncode.summers_makeValid();
//	create a new root, encode it via a recursive routine
	root= new Node(toEncode);
	root->encode(toEncode);
//	generate the fringe, let the encoder process it
	root->getHilbertList(fringe);
	toEncode.encoder->finishEncoding();
}

void MQuadTree::writeData(ostream &file) {
	assert( !fringe.empty() && root );
//	put in the stream the minimal and the maximal used level
	NodeExtremes extremes= for_each( fringe.begin(), fringe.end(), NodeExtremes() );
	put<Uchar>(file,extremes.min-zoom);
	put<Uchar>(file,extremes.max-zoom);
//	put the tree structure in the stream
	BitWriter bitWriter(file);
	root->toFile(bitWriter,extremes);
}

void MQuadTree::readData_buildRanges(istream &file,const Block &block,int zoom_) {
	assert( fringe.empty() && !root );
	zoom= zoom_;
//	get from the stream the minimal and the maximal used level
	NodeExtremes extremes;
	extremes.min= get<Uchar>(file)+zoom;
	extremes.max= get<Uchar>(file)+zoom;
//	build the range tree
	BitReader bitReader(file);
	root= new Node(block);
	root->fromFile(bitReader,extremes);
//	generate the fringe of the range tree
	root->getHilbertList(fringe);
}


////	MQuadTree::Node class

void MQuadTree::Node::disconnect() {
//	disconnects itself from its brothers in the circle
	if ( father->son == this )
		father->son= ( brother==this ? 0 : brother );
	Node *prev= this;
	while (prev->brother != this)
		prev= prev->brother;
	prev->brother= this->brother;
}
void MQuadTree::Node::deleteSons() {
// 	delete all sons
	if (!son)
        return;
    Node *now= son;
    do {
        Node *next= now->brother;
        delete now;
        now= next;
    } while (now!=son);
    son= 0;
}
void MQuadTree::Node::divide() {
//	check against dividing already divided self
	assert(!son);
	short size= powers[level-1];
	short xmid= min<short>( x0+size, xend );
	short ymid= min<short>( y0+size, yend );
//  top-left (don't know the brother yet, using 0)
    son= new Node( Block(x0,y0,xmid,ymid), this, 0 );
    Node *last= son;
//  top-right
    if (xmid < xend) {
        last= new Node( Block(xmid,y0,xend,ymid), this, last );
//  bottom-right
        if (ymid < yend)
            last= new Node( Block(xmid,ymid,xend,yend), this, last );
    }
//  bottom-left
    if (ymid < yend)
        last= new Node( Block(x0,ymid,xmid,yend), this, last );
//  son finish
	son->brother= last;
}
void MQuadTree::Node::getHilbertList(RangeList &list,char start,char cw) {
	if (!son) {
		list.push_back(this);
		return;
	}
	Node *now= son
	, *sons[4]= {0,0,0,0};
//	categorize sons by their position into sons[0..3] - clockwise from top-left
	do {
		int pos= 0;
		if ( now->x0 > x0 )
			++pos;
		if ( now->y0 > y0 )
			pos= 3-pos;
		sons[pos]= now;
	} while ( (now=now->brother) != son );
//	recurse on the son that is the first on the Hilbert curve (if it's present)
	int pos= (start%= 4);
	if (sons[pos])
		sons[pos]->getHilbertList(list,start,-cw);
//	recurse on the second and the third son
	for (int i=0; i<2; ++i) {
		pos= (pos+4+cw)%4;
		if (sons[pos])
			sons[pos]->getHilbertList(list,start,cw);
	}
//	recurse on the last son
	pos= (pos+4+cw)%4;
	if (sons[pos])
		sons[pos]->getHilbertList(list,start+2,-cw);
}
int MQuadTree::Node::getSonCount() const {
	if (!son)
		return 0;
	Node *now= son->brother;
	int count= 1;
	while (now!=son) {
		now= now->brother;
		++count;
	}
	return count;
}
bool MQuadTree::Node::encode(const PlaneBlock &toEncode) {
	MQuadTree *mod= debugCast<MQuadTree*>(toEncode.ranges);
	assert( mod && level>=mod->minLevel() );
//	check for minimal level -> cannot be divided, find the best domain
	if ( level == mod->minLevel() ) {
	//	try to find the best mapping, not restricting the max.\ SE and exit
		toEncode.encoder->findBestSE(*this,true);
		return false;
	}

	int pixCount= size();
//	TODO: regular ranges optimization
	float maxSE= toEncode.moduleQ2SE->rangeSE( toEncode.quality, pixCount );
	bool tryEncode= true;

	if ( level > mod->maxLevel() )
		tryEncode= false;
	else {
	//	if heuirstic dividing is allowed and signals dividing, don't try to encode
		if ( mod->heuristicAllowed() ) {
			float rSum= toEncode.getSum(*this,BlockSummer::Values);
			float r2Sum= toEncode.getSum(*this,BlockSummer::Squares);
			if ( ldexp( r2Sum-sqr(rSum)/pixCount, -4 ) * level > maxSE )
				tryEncode= false;
		}
	//	if we decided to try to encode, do it and return if the quality is sufficient
		if ( tryEncode && toEncode.encoder->findBestSE(*this) <= maxSE )
			return false;
		#ifndef NDEBUG
		else // tried to encode, but unsuccessfully and forced to be divided
			++debugCast<MQuadTree*>(toEncode.ranges)->badTries;
		#endif
	}
//	the range needs to be divided, try to encode the sons
	divide();
	bool aSonDivided= false;
	Node *now= son;
	do {// if any of the sons is divided, set tryEncode to true
		bool divided= now->encode(toEncode);
		aSonDivided= aSonDivided || divided;
	} while ( (now=now->brother) != son );
//	if (I unsuccessfully tried to encode or a son was divided) or (I have too big level), return
	if ( aSonDivided || tryEncode || level > mod->maxLevel() )
		return true;
//	this range still has a chance, try to encode it
	if ( toEncode.encoder->findBestSE(*this) <= maxSE ) {
		#ifndef NDEBUG
			++debugCast<MQuadTree*>(toEncode.ranges)->badDivides;
		#endif
		deleteSons();
		return false;
	} else {
		#ifndef NDEBUG
			++debugCast<MQuadTree*>(toEncode.ranges)->triedMerges;
		#endif
		return true;
	}
}
void MQuadTree::Node::toFile(BitWriter &file,NodeExtremes extremes) {
	if (son) {
	//  Node is divided
		if (level<=extremes.max)
			file.putBits(1,1);
	//	recurse
		Node *now= son;
		do
			now->toFile(file,extremes);
		while ( (now= now->brother) != son );
	} else
	//  Node isn't divided
		if (level>extremes.min)
			file.putBits(0,1);
}
void MQuadTree::Node::fromFile(BitReader &file,NodeExtremes extremes) {
//	should I be divided ?
	bool div= level>extremes.max || level>extremes.min && file.getBits(1) ;
	if (div) {
		divide();
		Node *now= son;
		do
			now->fromFile(file,extremes);
		while ( (now=now->brother) != son );
	}
}
