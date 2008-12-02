#include "saupePredictor.h"
using namespace std;

MSaupePredictor::OneRangePred* MSaupePredictor::newPredictor(const NewPredictorData &data) {
//	ensure the levelTrees vector is long enough
	int level= data.rangeBlock->level;
	if ( level >= (int)levelTrees.size() )
		levelTrees.resize( level+1, (Tree*)0 );
//	ensure the tree is built for the level
	Tree *tree= levelTrees[level];
	if (!tree)
		tree= levelTrees[level]= createTree(data);
	assert(tree);

	int maxChunks= (int)ceil(maxChunkCoeff()*tree->count);
	if (maxChunks<=0)
		maxChunks= 1;

	OneRangePredictor *result=
		new OneRangePredictor(data,settingsInt(ChunkSize),*tree,maxChunks);

	#ifndef NDEBUG
	maxpred+= tree->count*(data.allowRotations?8:1)*(data.allowInversion?2:1);
	result->predicted= &predicted;
	#endif

	return result;
}

void MSaupePredictor::cleanUp() {
	clearContainer(levelTrees);
	levelTrees.clear();
}

MSaupePredictor::Tree* MSaupePredictor::createTree(const NewPredictorData &data) {
//	compute some accelerators
	const ISquareEncoder::LevelPoolInfos::value_type &poolInfos= *data.poolInfos;
	int domainCount= poolInfos.back().indexBegin;
	int level= data.rangeBlock->level;
	int sideLength= powers[level];
	int pixelCount= powers[2*level];
//	create space for temporary domain pixels
	KDReal *domPix= new KDReal[ domainCount * pixelCount ];
//	init domain-blocks' from every pool
	KDReal *domPixNow= domPix;
	int poolCount= data.pools->size();
	for (int poolID=0; poolID<poolCount; ++poolID) {
	//	check we are on the place we want to be; get the current pool, density, etc.
		assert( domPixNow-domPix == poolInfos[poolID].indexBegin*pixelCount );
		const ISquareDomains::Pool &pool= (*data.pools)[poolID];
		int density= poolInfos[poolID].density;
		if (!density) // no domains in this pool for this level
			continue;
		int poolXend= density*getCountForDensity( pool.width, density, sideLength );
		int poolYend= density*getCountForDensity( pool.height, density, sideLength );
	//	handle the domain block on [x0,y0] (for each in the pool)
		for (int x0=0; x0<poolXend; x0+=density)
			for (int y0=0; y0<poolYend; y0+=density) {
			//	compute the average and standard deviation (data needed for normalization)
				Real sum, sum2;
				sum= pool.summers[0].getSum(x0,y0,x0+sideLength,y0+sideLength);
				sum2= pool.summers[1].getSum(x0,y0,x0+sideLength,y0+sideLength);
			//	the same as  = 1 / sqrt( sum2 - sqr(sum)/pixelCount  ) );
				Real multiply= 1 / sqrt( sum2 - sqr(ldexp(sum,-level)) );
				if ( !finite(multiply) )  
					multiply= 1; // it would be better not to add the domains into the tree
			//	if inversion is allowed and the first pixel is below zero then invert the block
				//if ( data.allowInversion && *domPixNow < 0 )
				//	multiply= -multiply;
				Real avg= ldexp(sum,-2*level);
				MatrixWalkers::AddMulCopy<Real> oper( -avg, multiply  );
			//	copy every column of the domain's pixels (and normalize them on the way)
				using namespace FieldMath;
				KDReal *linCol= domPixNow;
				KDReal *linColEnd= domPixNow+pixelCount;
				for (int x=x0; linCol!=linColEnd; ++x,linCol+=sideLength)
					transform2( linCol, linCol+sideLength, pool.pixels[x]+y0, oper );
			//	move to the data of the next domain
				domPixNow= linColEnd;
			}
	}
	assert( domPixNow-domPix == domainCount*pixelCount ); // check we are just at the end
//	create the tree from obtained data
	Tree *result= Tree::Builder
		::makeTree( domPix, pixelCount, domainCount, &Tree::Builder::chooseLongest );
//	clean up temporaries, return the tree
	delete[] domPix;
	return result;
}

namespace NOSPACE {
	template<class T> struct AddMulCopyTo2nd {
		T toAdd, toMul;

		AddMulCopyTo2nd(T add,T mul)
		: toAdd(add), toMul(mul) {}

		template<class R1,class R2> void operator()(R1 f,R2 &res) const
			{ res= (f+toAdd)*toMul; }
		void innerEnd() const {}
	};

	struct SignChanger {
		template<class R1,class R2> void operator()(R1 in,R2 &out) const
			{ out= -in; }
	};
}
MSaupePredictor::OneRangePredictor::OneRangePredictor
( const NewPredictorData &data, int chunkSize_, const Tree &tree, int maxChunks )
: chunkSize(chunkSize_), chunksRemain(maxChunks)
, firstChunk(true), allowRotations(data.allowRotations) {
	assert(data.isRegular); // TODO: we can't handle irregular ranges yet
//	compute some accelerators, allocate space for normalized range (+rotations,inversion)
	int rotationCount= allowRotations ? 8 : 1;
	heapCount= rotationCount * (data.allowInversion ? 2 : 1);
	points= new KDReal[tree.length*heapCount];
//	compute normalizing transformation and initialize a normalizator object
	Real multiply= sqrt(data.pixCount) / data.rnDev;
	AddMulCopyTo2nd<Real> oper( -data.rSum/data.pixCount, multiply );
//	compute SE-normalizing accelerator
	normalizator.initialize(data);
	{
		int sideLength= powers[data.rangeBlock->level];
		KDReal* rotMatrices[rotationCount][sideLength]; // a fake array of matrices
		Block localBlock(0,0,sideLength,sideLength);
	//	create normalized rotations
		for (int rot=0; rot<rotationCount; ++rot) {
		//	fake the matrix for this rotation
			KDReal *currRangeLin= points+rot*tree.length;
			KDReal **currRangeMatrix= rotMatrices[rot];
			initMatrixPointers( sideLength, sideLength, currRangeLin, currRangeMatrix );
		//	fill it with normalized data
			using namespace MatrixWalkers;
			walkOperateCheckRotate( Checked<const SReal>(data.rangePixels,*data.rangeBlock)
			, oper, currRangeMatrix, localBlock, rot );
		}
	}
//	create inverse of the rotations if needed
	if (data.allowInversion) {
		KDReal *pointsMiddle= points+tree.length*rotationCount;
		FieldMath::transform2( points, pointsMiddle, pointsMiddle, SignChanger() );
	}
//	create all the heaps and initialize their infos (and make a heap of the infos)
	heaps.reserve(heapCount);
	infoHeap.reserve(heapCount);
	for (int i=0; i<heapCount; ++i) {
		Tree::PointHeap *heap= new Tree::PointHeap( tree, points+i*tree.length );
		heaps.push_back(heap);
		infoHeap.push_back(HeapInfo( i, heap->getTopSE() ));
	}
//	build the heap from heap-informations
	make_heap( infoHeap.begin(), infoHeap.end() );
}

MSaupePredictor::Predictions& MSaupePredictor::OneRangePredictor
::getChunk(float maxPredictedSE,Predictions &store) {
	assert( heaps.size()==(size_t)heapCount && infoHeap.size()<=(size_t)heapCount );
	if ( infoHeap.empty() || chunksRemain<=0 ) {
		store.clear();
		return store;
	}
//	get the number of predictions to make (may be larger for the first chunk)
	int predCount= chunkSize;
	if (firstChunk) {
		firstChunk= false;
		if (heapCount>predCount)
			predCount= heapCount;
	}
//	compute the max. normalized SE to predict
	float maxNormalizedSE= normalizator.normSE(maxPredictedSE);
//	make a local working copy for the result (the prediction), adjust its size
	Predictions result;
	swap(result,store); // swapping is the quickest way
	result.resize(predCount);
//	generate the predictions
	for (Predictions::iterator it=result.begin(); it!=result.end(); ++it) {
		pop_heap( infoHeap.begin(), infoHeap.end() );
		HeapInfo &bestInfo= infoHeap.back();
	//	if the error is too high, cut the vector and exit the cycle
		if ( bestInfo.bestError > maxNormalizedSE ) {
			result.erase( it, result.end() );
			infoHeap.clear(); // to be able to exit more quickly in the next call
			break;
		}
	//	fill the prediction and pop the heap
		assert( 0<=bestInfo.index && bestInfo.index<heapCount );
		Tree::PointHeap &bestHeap= *heaps[bestInfo.index];
		it->domainID= bestHeap.popLeaf();
		it->rotation= allowRotations ? bestInfo.index%8 : 0; // modulo - for the case of inversion
	//	check for emptying the heap
		if ( !bestHeap.isEmpty() ) {
		//	rebuild the infoHeap heap
			bestInfo.bestError= bestHeap.getTopSE();
			push_heap( infoHeap.begin(), infoHeap.end() );
		} else { // just emptied a heap
			infoHeap.pop_back();
		//	check for emptying the last heap
			if ( infoHeap.empty() )
				break;
		}
	}
//	return the result
	swap(result,store);
	--chunksRemain;

	#ifndef NDEBUG
	*predicted+= store.size();
	#endif

	return store;
}
