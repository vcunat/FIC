#include "saupePredictor.h"
#include "stdDomains.h" // because of HalfShrinker
using namespace std;

IStdEncPredictor::IOneRangePredictor* MSaupePredictor
::newPredictor(const NewPredictorData &data) {
//	ensure the levelTrees vector is long enough
	int level= data.rangeBlock->level;
	if ( level >= (int)levelTrees.size() )
		levelTrees.resize( level+1, (Tree*)0 );
//	ensure the tree is built for the level
	Tree *tree= levelTrees[level];
	if (!tree)
		tree= levelTrees[level]= createTree(data);
	ASSERT(tree);
//	get the max. number of domains to predict and create the predictor
	int maxPredicts= (int)ceil(maxPredCoeff()*tree->count);
	if (maxPredicts<=0)
		maxPredicts= 1;
	OneRangePredictor *result= 
		new OneRangePredictor( data, settingsInt(ChunkSize), *tree, maxPredicts );
		
	#ifndef NDEBUG // collecting debugging stats
		maxpred+= tree->count*(data.allowRotations?8:1)*(data.allowInversion?2:1);
		result->predicted= &predicted;
	#endif

	return result;
}

namespace MatrixWalkers {
	/** Creates a Checked structure to walk a linear array as a matrix */
	template<class T,class I> Checked<T,I> makeLinearizer(T* start,I inCount,I outCount) {
		return Checked<T,I>
			( MatrixSlice<T,I>::makeRaw(start,inCount), Block(0,0,outCount,inCount) );
	}
	
	/** Walker that iterates over rectangles in a matrix and returns their sums,
	 *	to be constructed by ::makeSumWalker */
	template < class SumT, class PixT, class I >
	struct SumWalker {
		SummedMatrix<SumT,PixT,I> matrix;
		I width, height, y0;//, xEnd, yEnd;
		I x, y;
		
		//bool outerCond()	{ return x!=xEnd; }
		void outerStep()	{ x+= width; }
		
		void innerInit()	{ y= y0; }
		//bool innerCond()	{ return y!=yEnd; }
		void innerStep()	{ y+= height; }
		
		SumT get()			{ return matrix.getValueSum(x,y,x+width,y+height); }
	};
	/** Constructs a square SumWalker on \p matrix starting on [\p x0,\p y0], iterating
	 *	over squares of level (\p inLevel-\p outLevel) and covering a square of level \p outLevel */
	template < class SumT, class PixT, class I >
	SumWalker<SumT,PixT,I> makeSumWalker
	( const SummedMatrix<SumT,PixT,I> &matrix, I x0, I y0, I inLevel, I outLevel ) {
		ASSERT( x0>=0 && y0>=0 && inLevel>=0 && outLevel>=0 && inLevel>=outLevel );
		SumWalker<SumT,PixT,I> result;
		result.matrix= matrix;
		result.width= result.height= powers[inLevel-outLevel];
		result.y0= y0;
		//result.xEnd= x0+powers[inLevel];
		//result.yEnd= y0+powers[inLevel];
		result.x= x0;
		DEBUG_ONLY( result.y= numeric_limits<I>::max(); )
		return result;
	}
	
	/** Similar to HalfShrinker, but instead of multiplying the sums with 0.25
	 *	it uses an arbitrary number */
	template<class T,class I=PtrInt,class R=Real>
	struct HalfShrinkerMul: public HalfShrinker<T,I> { ROTBASE_INHERIT
		R toMul;
		
		HalfShrinkerMul( TMatrix matrix, R toMul_, I x0=0, I y0=0 )
		: HalfShrinker<T,I>(matrix,x0,y0), toMul(toMul_) {}
		
		T get() { 
			TMatrix &c= current;
			T *cs= c.start;
			return toMul * ( cs[0] + cs[1] + cs[c.colSkip] + cs[c.colSkip+1] );
		}
	};
} // MatrixWalkers namespace
namespace NOSPACE {
	typedef MSaupePredictor::KDReal KDReal;
	/** The common part of ::refineDomain and ::refineRange. It does the actual shrinking
	 *	(if needed). \p multiply parameter would normalize pixels, not sums. */
	inline static void refineBlock( const SummedPixels &pixMatrix, int x0, int y0
	, int predWidth, int predHeight, Real multiply, Real avg
	, int realLevel, int predLevel, SReal *pixelResult ) {
		using namespace MatrixWalkers;
	//	adjust the multiplication coefficients for normalizing from sums of values
		int levelDiff= realLevel-predLevel;
		if (levelDiff)
			multiply= ldexp(multiply,-2*levelDiff);
		ASSERT( finite(multiply) && finite(avg) );
	//	construct the operator and the walker on the result
		AddMulCopy<Real> oper( -avg, multiply );
		Checked<KDReal> linWalker= makeLinearizer(pixelResult,predWidth,predHeight);
	//	decide the shrinking method
		if (levelDiff==0)		// no shrinking
			walkOperate( linWalker, Rotation_0<SReal>(pixMatrix.pixels,x0,y0), oper );
		else if (levelDiff==1)	// shrinking by groups of four
			walkOperate( linWalker
				, HalfShrinkerMul<SReal>(pixMatrix.pixels,multiply,x0,y0), oper );
		else // levelDiff>=2	// shrinking by bigger groups - using the summer
			walkOperate( linWalker
				, makeSumWalker(pixMatrix,x0,y0,realLevel,predLevel), oper );
	}
}
void MSaupePredictor::refineDomain( const SummedPixels &pixMatrix, int x0, int y0
, bool allowInversion, int realLevel, int predLevel, SReal *pixelResult ) {
//	compute the average and standard deviation (data needed for normalization)
	int realSide= powers[realLevel];
	Real sum, sum2;
	pixMatrix.getSums(x0,y0,x0+realSide,y0+realSide).unpack(sum,sum2);
	Real avg= ldexp(sum,-2*realLevel); // means avg= sum / (2^realLevel)^2
//	the same as  = 1 / sqrt( sum2 - sqr(sum)/pixelCount  ) );
	Real multiply= 1 / sqrt( sum2 - sqr(ldexp(sum,-realLevel)) );
	if ( !finite(multiply) )
		multiply= 1; // it would be better not to add the domains into the tree
//	if inversion is allowed and the first pixel is below average, then invert the block
	if ( allowInversion && pixMatrix.pixels[x0][y0] < avg )
		multiply= -multiply;
//	do the actual work
	int predSide= powers[predLevel];
	refineBlock( pixMatrix, x0, y0, predSide, predSide, multiply, avg
	, realLevel, predLevel, pixelResult );
}
void MSaupePredictor::refineRange
( const NewPredictorData &data, int predLevel, SReal *pixelResult ) {
	const ISquareRanges::RangeNode &rb= *data.rangeBlock;
	Real avg, multiply;
	if ( data.isRegular || predLevel==rb.level ) { // no cropping of the block
	//	compute the average and standard deviation (data needed for normalization)
		avg= data.rSum/data.pixCount;
		multiply= ( data.isRegular ? powers[rb.level] : sqrt(data.pixCount) ) / data.rnDev;
	} else { // the block needs cropping
	//	crop the range block so it doesn't contain parts of shrinked pixels
		int mask= powers[rb.level-predLevel]-1;
		int adjWidth= rb.width() & mask;
		int adjHeight= rb.height() & mask;
		int pixCount= adjWidth*adjHeight;
		if (pixCount==0)
			return;
	//	compute the coefficients needed for normalization, assertion tests in ::refineBlock
		Real sum, sum2;
		data.rangePixels->getSums( rb.x0, rb.y0, rb.x0+adjWidth, rb.y0+adjHeight )
			.unpack(sum,sum2);
		avg= sum/pixCount;
		multiply= 1 / sqrt( sum2 - sqr(sum)/pixCount );
	}
//	do the actual work
	int levelDiff= rb.level-predLevel;
	refineBlock( *data.rangePixels, rb.x0, rb.y0
		, rShift(rb.width(),levelDiff), rShift(rb.height(),levelDiff)
		, multiply, avg, rb.level, predLevel, pixelResult );
}

MSaupePredictor::Tree* MSaupePredictor::createTree(const NewPredictorData &data) {
//	compute some accelerators
	const ISquareEncoder::LevelPoolInfos::value_type &poolInfos= *data.poolInfos;
	const int domainCount= poolInfos.back().indexBegin
	, realLevel= data.rangeBlock->level
	, predLevel= getPredLevel(realLevel)
	, realSide= powers[realLevel]
	, predPixCount= powers[2*predLevel];
	ASSERT(realLevel>=predLevel);
//	create space for temporary domain pixels, can be too big to be on the stack
	KDReal *domPix= new KDReal[ domainCount * predPixCount ];
//	init domain-blocks from every pool
	KDReal *domPixNow= domPix;
	int poolCount= data.pools->size();
	for (int poolID=0; poolID<poolCount; ++poolID) {
	//	check we are on the place we want to be; get the current pool, density, etc.
		ASSERT( domPixNow-domPix == poolInfos[poolID].indexBegin*predPixCount );
		const ISquareDomains::Pool &pool= (*data.pools)[poolID];
		int density= poolInfos[poolID].density;
		if (!density) // no domains in this pool for this level
			continue;
		int poolXend= density*getCountForDensity( pool.width, density, realSide );
		int poolYend= density*getCountForDensity( pool.height, density, realSide );
	//	handle the domain block on [x0,y0] (for each in the pool)
		for (int x0=0; x0<poolXend; x0+=density)
			for (int y0=0; y0<poolYend; y0+=density) {
				refineDomain( pool, x0, y0, data.allowInversion, realLevel, predLevel, domPixNow );
				domPixNow+= predPixCount;
			}
	}
	ASSERT( domPixNow-domPix == domainCount*predPixCount ); // check we are just at the end
//	create the tree from obtained data
	Tree *result= Tree::Builder
		::makeTree( domPix, predPixCount, domainCount, &Tree::Builder::chooseApprox );
//	clean up temporaries, return the tree
	delete[] domPix;
	return result;
}

namespace MatrixWalkers {
	/** Transformer performing an affine function */
	template<class T> struct AddMulCopyTo2nd {
		T toAdd, toMul;

		AddMulCopyTo2nd(T add,T mul)
		: toAdd(add), toMul(mul) {}

		template<class R1,class R2> void operator()(R1 f,R2 &res) const
			{ res= (f+toAdd)*toMul; }
		void innerEnd() const {}
	};
	/** A simple assigning operator - assigns its second argument into the first one */
	struct Assigner: public OperatorBase {
		template<class R1,class R2> void operator()(const R1 &src,R2 &dest) const
			{ dest= src; }
	};
	/** Transformer performing sign change */
	struct SignChanger {
		template<class R1,class R2> void operator()(R1 src,R2 &dest) const
			{ dest= -src; }
	};
}
MSaupePredictor::OneRangePredictor::OneRangePredictor
( const NewPredictorData &data, int chunkSize_, const Tree &tree, int maxPredicts )
	: chunkSize(chunkSize_), predsRemain(maxPredicts)
	, firstChunk(true), allowRotations(data.allowRotations), isRegular(data.isRegular) 
{
//	compute some accelerators, allocate space for normalized range (+rotations,inversion)
	int rotationCount= allowRotations ? 8 : 1;
	heapCount= rotationCount * (data.allowInversion ? 2 : 1);
	points= new KDReal[tree.length*heapCount];	
	
//	if the block isn't regular, fill the space with NaNs (to be left on unused places)
	if (!isRegular)
		fill( points, points+tree.length, numeric_limits<KDReal>::quiet_NaN() );
	
//	find out the prediction level (the level of the size) and the size of prediction sides
	int predLevel= log2ceil(tree.length)/2;
	int predSideLen= powers[predLevel];
	ASSERT( powers[2*predLevel] == tree.length );
//	compute SE-normalizing accelerator
	errorNorm.initialize(data,predLevel);
	
	refineRange( data, predLevel, points );

//	if rotations are allowed, rotate the refined block
	if (allowRotations) {
		using namespace MatrixWalkers;
	//	create walker for the refined (and not rotated) block
		Checked<KDReal> refBlockWalker= makeLinearizer( points, predSideLen, predSideLen );
		
		MatrixSlice<KDReal> rotMatrix= MatrixSlice<KDReal>::makeRaw(points,predSideLen);
		Block shiftedBlock( 0, 0, predSideLen, predSideLen );
		
		for (int rot=1; rot<rotationCount; ++rot) {
			rotMatrix.start+= tree.length; // shifting the matrix to the next rotation
			walkOperateCheckRotate
				( refBlockWalker, Assigner(), rotMatrix, shiftedBlock, rot );
		}
		ASSERT( rotMatrix.start == points+tree.length*(rotationCount-1) );
	}

//	create inverse of the rotations if needed
	if (data.allowInversion) {
		KDReal *pointsMiddle= points+tree.length*rotationCount;
		FieldMath::transform2
			( points, pointsMiddle, pointsMiddle, MatrixWalkers::SignChanger() );
	}
	
//	create all the heaps and initialize their infos (and make a heap of the infos)
	heaps.reserve(heapCount);
	infoHeap.reserve(heapCount);
	for (int i=0; i<heapCount; ++i) {
		PointHeap *heap= new PointHeap( tree, points+i*tree.length, !data.isRegular );
		heaps.push_back(heap);
		infoHeap.push_back(HeapInfo( i, heap->getTopSE() ));
	}
//	build the heap from heap-informations
	make_heap( infoHeap.begin(), infoHeap.end() );
}

MSaupePredictor::Predictions& MSaupePredictor::OneRangePredictor
::getChunk(float maxPredictedSE,Predictions &store) {
	ASSERT( PtrInt(heaps.size())==heapCount && PtrInt(infoHeap.size())<=heapCount );
	if ( infoHeap.empty() || predsRemain<=0 ) {
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
//	check the limit for prediction count
	if (predCount>predsRemain)
		predCount= predsRemain;
	predsRemain-= predCount;
//	compute the max. normalized SE to predict
	float maxNormalizedSE= errorNorm.normSE(maxPredictedSE);
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
		ASSERT( 0<=bestInfo.index && bestInfo.index<heapCount );
		PointHeap &bestHeap= *heaps[bestInfo.index];
		it->domainID= isRegular
			? bestHeap.popLeaf<false>(maxNormalizedSE)
			: bestHeap.popLeaf<true>(maxNormalizedSE);
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

	#ifndef NDEBUG
	*predicted+= store.size();
	#endif

	return store;
}
