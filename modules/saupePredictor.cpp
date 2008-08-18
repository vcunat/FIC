#include "saupePredictor.h"

using namespace std;

void MSaupePredictor::cleanUp() {
// TODO (admin#1#): uncomment this
	clearContainer(levelTrees);
	levelTrees.clear();
}
MSaupePredictor::OneRangePred* MSaupePredictor::
newPredictor(const NewPredictorData &data) {
//	ensure the levelTrees vector is long enough
	int level= data.rangeBlock->level;
	if ( level >= (int)levelTrees.size() )
		levelTrees.resize( level+1, (Tree*)0 );
//	ensure the tree is built for the level
	Tree *tree= levelTrees[level];
	if ( !tree )
		tree= levelTrees[level]= createTree(data);

	return new OneRangePredictor(data,settingsInt(ChunkSize),*tree);
}

MSaupePredictor::Tree* MSaupePredictor::createTree(const NewPredictorData &data) {
//	compute some accelerators
	const ISquareEncoder::LevelPoolInfos::value_type &poolInfos= *data.poolInfos;
	int domainCount= poolInfos.back().indexBegin;
	int level= data.rangeBlock->level;
	int sideLength= powers[level];
	int pixelCount= powers[2*level];
//	create space for temporary domain pixels
	float *domPix= new float[ domainCount * pixelCount ];
//	init domain-blocks' from every pool
	float *domPixNow= domPix;
	int poolCount= data.pools->size();
	for (int poolID=0; poolID<poolCount; ++poolID) {
		assert( domPixNow-domPix == poolInfos[poolID].indexBegin*pixelCount );
		const ISquareDomains::Pool &pool= (*data.pools)[poolID];
		int density= poolInfos[poolID].density;
		int poolXend= density*getCountForDensity( pool.width, density, sideLength );
		int poolYend= density*getCountForDensity( pool.height, density, sideLength );
	//	handle the domain block on [x0,y0] (for each in the pool)
		for (int y0=0; y0<poolYend; y0+=density)
			for (int x0=0; x0<poolXend; x0+=density) {
			//	compute the average and standard deviation (data needed for normalization)
				Real avg, avg2, countInv= 1/pixelCount;
				avg= countInv*pool.summers[0].getSum(x0,y0,x0+sideLength,y0+sideLength);
				avg2= countInv*pool.summers[1].getSum(x0,y0,x0+sideLength,y0+sideLength);
				MatrixWalkers::AddMulCopy<Real> oper( -avg, 1/sqrt( avg2-sqr(avg) ) );
			//	copy every column of the domain pixels (and normalize them on the way)
				using namespace FieldMath;
				float *linCol= domPixNow;
				float *linColEnd= domPixNow+pixelCount;
				for (int x=x0; linCol!=linColEnd; ++x,linCol+=sideLength)
					transform2( linCol, linCol+sideLength, pool.pixels[x]+y0, oper );
			//	move to the next domain's data
				domPixNow= linColEnd;
			}
	}
//	create the tree from obtained data
	Tree *result= new Tree
	( domPix, pixelCount, domainCount, &KDCoordChoosers::boundBoxLongest );
//	clean up temporaries, return the tree
	delete[] domPix;
	return result;
}

namespace NOSPACE {
	template<class T> struct AddMulCopyTo2nd {
		T toAdd, toMul;

		AddMulCopyTo2nd(T add,T mul)
		: toAdd(add), toMul(mul) {}

		void operator()(float f,float &res) const
			{ res= (f+toAdd)*toMul; }
		void endLine() const {}
	};

	template<class T> struct SignChanger {
		void operator()(const T &in,T &out) const
			{ out= -in; }
	};
}
MSaupePredictor::OneRangePredictor::
OneRangePredictor(const NewPredictorData &data,int chunkSize_,const Tree &tree)
: chunkSize(chunkSize_), firstChunk(true) {
	assert(data.isRegular);
//	compute some accelerators, allocate space for normalized range (+rotations,inversion)
	int rotationCount= data.allowRotations ? 8 : 1;
	heapCount= rotationCount * (data.allowInversion ? 2 : 1);
	points= new float[tree.length*heapCount];
//	compute normalizing transformation and initialize a normalizator object
	Real rSum2= sqr(data.rSum);
	Real multiply= data.pixCount / sqrt(data.r2Sum*data.pixCount-rSum2);
	AddMulCopyTo2nd<Real> oper( -data.rSum/data.pixCount, multiply );
//	compute SE-normalizing accelerator
	errorConvAccel= 1/( sqr(data.pixCount) * cube(data.r2Sum-rSum2) );
	{
		int sideLength= powers[data.rangeBlock->level];
		float *rotMatrices[rotationCount][sideLength];
		Block localBlock(0,0,sideLength,sideLength);
	//	create normalized rotations
		for (int rot=0; rot<rotationCount; ++rot) {
			float *currRangeLin= points+rot*tree.length;
			float **currRangeMatrix= rotMatrices[rot];
			initMatrixPointers( sideLength, sideLength, currRangeLin, currRangeMatrix );
			MatrixWalkers::walkOperateCheckRotate
			( data.rangePixels, *data.rangeBlock, oper, currRangeMatrix, localBlock, rot );
		}
	}
//	create inverse of the rotations if needed
	if (data.allowInversion) {
		float *pointsMiddle= points+tree.length*rotationCount;
		FieldMath::transform2( points, pointsMiddle, pointsMiddle, SignChanger<float>() );
	}
//	create all the heaps and initialize their infos (and make a heap of the infos)
	heaps.reserve(heapCount);
	infoHeap.reserve(heapCount);
	for (int i=0; i<heapCount; ++i) {
		Tree::PointHeap *heap= new Tree::PointHeap( tree, points+i*tree.length );
		heaps.push_back(heap);
		infoHeap.push_back(HeapInfo( i, heap->getTopSE() ));
	}
	make_heap( infoHeap.begin(), infoHeap.end() );
}

MSaupePredictor::Predictions& MSaupePredictor::OneRangePredictor::
getChunk(float maxPredictedSE,Predictions &store) {
	if ( infoHeap.empty() ) {
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
//
	float maxNormalizedSE= normalizeSE(maxPredictedSE);
//	make a local working copy for the result, adjust its size
	Predictions result;
	swap(result,store);
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
		Tree::PointHeap &bestHeap= *heaps[bestInfo.index];
		it->domainID= bestHeap.popLeaf();
		it->rotation= bestInfo.index%8; // modulo - for the case of inversion
	//	check for emptying the heap
		if ( !bestHeap.empty() ) {
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
	return store;
}
