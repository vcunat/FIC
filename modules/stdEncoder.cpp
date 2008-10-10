#include "stdEncoder.h"
#include "../fileUtil.h"

/// \file stdEncoder.cpp

using namespace std;

namespace Quantizer {
	/** Quantizes f that belongs to 0..possib/2^scale into 0..possib-1 */
	inline int quantizeByPower(Real f,int scale,int possib) {
		assert( f>=0 && f<=possib/(Real)powers[scale] );
		int result= (int)trunc(ldexp(f,scale));
		assert( result>=0 && result<=possib );
		return result<possib ? result : --result;
	}
	/** Performs the opposite to #quantizeByPower */
	inline Real dequantizeByPower(int i,int scale,int DEBUG_ONLY(possib)) {
		assert( i>=0 && i<possib );
		Real result= ldexp(i+Real(0.5),-scale);
		assert( result>=0 && result<= possib/(Real)powers[scale] );
		return result;
	}

	/** (De)Quantizer for range-block averages */
	class Average {
		int scale, possib;
	public:
		Average(int possibLog2) {
			assert(possibLog2>0);
		//	the average is from [0,1]
			scale= possibLog2;
			possib= powers[possibLog2];
		}
		int quant(Real avg)
			{ return quantizeByPower(avg,scale,possib); }
		Real dequant(int i)
			{ return dequantizeByPower(i,scale,possib); }
		Real qRound(Real avg)
			{ return dequant(quant(avg)); }
	};

	/** (De)Quantizer for range-block deviations */
	class Deviation {
		int scale, possib;
	public:
		Deviation(int possibLog2) {
			assert(possibLog2>0);
		//	the deviation is from [0,0.5] -> we have to scale twice more (log2+=1)
			scale= possibLog2+1;
			possib= powers[possibLog2];
		}
		int quant(Real dev)
			{ return quantizeByPower(dev,scale,possib); }
		Real dequant(int i)
			{ return dequantizeByPower(i,scale,possib); }
		Real qRound(Real dev)
			{ return dequant(quant(dev)); }
	};
}

namespace NOSPACE {
	struct LevelPoolInfo_indexComparator {
		typedef ISquareEncoder::LevelPoolInfo LevelPoolInfo;
		bool operator()( const LevelPoolInfo &a, const LevelPoolInfo &b )
			{ return a.indexBegin < b.indexBegin; }
	};
}
MStandardEncoder::PoolInfos::const_iterator MStandardEncoder
::getPoolFromDomID( int domID, const PoolInfos &poolInfos ) {
	assert( domID>=0 && domID<poolInfos.back().indexBegin );
//	get the right pool
	PoolInfos::value_type toFind;
	toFind.indexBegin= domID;
	return upper_bound( poolInfos.begin(), poolInfos.end(), toFind
		, LevelPoolInfo_indexComparator() ) -1;
}

const ISquareDomains::Pool& MStandardEncoder::getDomainData
( const RangeNode &rangeBlock, const ISquareDomains::PoolList &pools
, const PoolInfos &poolInfos, int domIndex, Block &block ) {
	
//	get the pool
	PoolInfos::const_iterator it= getPoolFromDomID( domIndex, poolInfos );
	const Pool &pool= pools[ it - poolInfos.begin() ];
//	get the coordinates
	int indexInPool= domIndex - it->indexBegin;
	assert( indexInPool>=0 && indexInPool<(it+1)->indexBegin );
	int size= powers[rangeBlock.level];
	int domsInRow= getCountForDensity( pool.width, it->density, size );
	block.x0= (indexInPool%domsInRow)*it->density;
	block.y0= (indexInPool/domsInRow)*it->density;
	block.xend= block.x0+size;
	block.yend= block.y0+size;

	return pool;
}

/** Adjust block of a domain to be mapped equally on an incomplete range (depends on rotation) */
inline static Block adjustDomainForIncompleteRange
( int size, int rotation, Block block ) {
	// 0, 0T: top left
	// 1, 1T: top right
	// 2, 2T: bottom right
	// 3, 3T: bottom left

	assert( 0<=rotation && rotation<8 );
	int rotID= rotation/2;

	int yShift= size - block.height();
	int xShift= size - block.width();
	int magic= rotID+rotation%2;
	if ( magic==1 || magic==3 ) // for rotations 0T, 1, 2T, 3 -> swapped x and y
		swap(xShift,yShift);

	if ( rotID>1 )	// domain rotations 2, 3, 2T, 3T -> aligned to the bottom
		block.y0+= yShift;
	else			// other rotations are aligned to the top
		block.yend-= yShift;

	if ( rotID==1 || rotID==2 )	// rotations 1, 2, 1T, 2T -> aligned to the right
		block.x0+= xShift;
	else						// other rotations are aligned to the left
		block.xend-= xShift;
	return block;
}

////	Member methods

void MStandardEncoder::initialize( IRoot::Mode mode, const PlaneBlock &planeBlock_ ) {
	planeBlock= &planeBlock_;
//	prepare the domains-module
	planeBlock->domains->initPools( planeBlock->width(), planeBlock->height() );

	int maxLevel= 1 +log2ceil(max( planeBlock->width(), planeBlock->height() ));
//	prepare levelPoolInfos
	levelPoolInfos.resize(maxLevel);

	if (mode==IRoot::Encode) {
		typedef ISquareDomains::PoolList PoolList;
	//	prepare the domains
		planeBlock->domains->fillPixelsInPools(*planeBlock);
	//	initialize the range summers
		planeBlock->summers_makeValid();

	//	prepare maximum SquareErrors allowed for regular range blocks
		stdRangeSEs.resize(maxLevel+1);
		planeBlock->moduleQ2SE->completeSquareRangeErrors
		( planeBlock->quality, maxLevel+1, &stdRangeSEs.front() );
	}
}

namespace NOSPACE {
	typedef IStdEncPredictor::NewPredictorData StableInfo;

	/** Information about the best (found so far) domain block for a range block */
	struct BestInfo: public IStdEncPredictor::Prediction {
		float error;	///< The square error of the mapping
		bool inverted;	///< Whether the colours are mapped with a negative linear coefficient
		#ifndef NDEBUG
		Real rdSum;
		#endif

		BestInfo()
		: error( numeric_limits<float>::max() ) {}
		IStdEncPredictor::Prediction& prediction()
			{ return *this; }
	};
} // namespace NOSPACE


struct MStandardEncoder::EncodingInfo {
	typedef void (EncodingInfo::*ExactCompareProc)( Prediction prediction );

private:
	/** Possible comparing methods */
	static const ExactCompareProc exactCompareArray[];

	ExactCompareProc selectedProc; ///< Selected comparing method
public:
	StableInfo stable;	///< Information only depending on the range (and not domain) block
	BestInfo best;		///< Information about the best (at the moment) mapping found
	Real bdSum			///  Sum of all pixels in best domain
	, bd2Sum;			///< Sum of squares of all pixesl in the best domain
	float targetSE		///	 The maximal accepted SE (square error) for the range
	, maxSE2Predict;	///< The maximal SE that the predictor should try to predict

public:
	EncodingInfo()
	: selectedProc(0) {}

	/** Initializes a RangeInfo object from #this */
	RangeInfo* initRangeInfo(RangeInfo *ri) const {
		ri->bestSE=			best.error;
		ri->prediction()=	best;
		ri->qrAvg=			stable.qrAvg;
		ri->qrDev2=			stable.qrDev2;
		ri->inverted=		best.inverted;
		#ifndef NDEBUG
		ri->exact.avg=	stable.rSum/stable.pixCount;
		ri->exact.dev2=	stable.r2Sum/stable.pixCount - sqr(ri->exact.avg);
		ri->exact.linCoeff= ( stable.pixCount*best.rdSum - stable.rSum*bdSum ) 
		/ ( stable.pixCount*bd2Sum - sqr(bdSum) );
		ri->exact.constCoeff= ri->exact.avg - ri->exact.linCoeff*bdSum/stable.pixCount;
		#endif
		return ri;
	}

	/** Selects the right comparing method according to #stable */
	void selectExactCompareProc() {
		selectedProc= exactCompareArray
		[(((  stable.quantError *2
			+ stable.allowInversion ) *2
			+ stable.isRegular ) *2
			+ (stable.maxLinCoeff2 >= 0) ) *2
			+ (stable.bigScaleCoeff != 0)
		];
	}

	/** Uses the selected comparing method */
	void exactCompare(Prediction prediction)
		{ (this->* selectedProc)(prediction); }

private:
	/** Template for all the comparing methods */
	template < bool quantErrors, bool allowInversion, bool isRegular
	, bool restrictMaxLinCoeff, bool bigScalePenalty >
	void exactCompareProc( Prediction prediction );
}; // EncodingInfo class

#define ALTERNATE_0(name,params...) name<params>
#define ALTERNATE_1(name,params...) ALTERNATE_0(name,##params,false), ALTERNATE_0(name,##params,true)
#define ALTERNATE_2(name,params...) ALTERNATE_1(name,##params,false), ALTERNATE_1(name,##params,true)
#define ALTERNATE_3(name,params...) ALTERNATE_2(name,##params,false), ALTERNATE_2(name,##params,true)
#define ALTERNATE_4(name,params...) ALTERNATE_3(name,##params,false), ALTERNATE_3(name,##params,true)
#define ALTERNATE_5(name,params...) ALTERNATE_4(name,##params,false), ALTERNATE_4(name,##params,true)
const MStandardEncoder::EncodingInfo::ExactCompareProc
MStandardEncoder::EncodingInfo::exactCompareArray[]
= {ALTERNATE_5(&MStandardEncoder::EncodingInfo::exactCompareProc)};
#undef ALTERNATE_0
#undef ALTERNATE_1
#undef ALTERNATE_2
#undef ALTERNATE_3
#undef ALTERNATE_4
#undef ALTERNATE_5

void debugStop() {
	return;
}

template< bool quantErrors, bool allowInversion, bool isRegular
, bool restrictMaxLinCoeff, bool bigScalePenalty >
void MStandardEncoder::EncodingInfo::exactCompareProc( Prediction prediction ) {
	using namespace MatrixWalkers;
//	find out which domain was predicted (pixel matrix and position within it)
	Block domBlock;
	const Pool &pool= getDomainData( *stable.rangeBlock
	, *stable.pools, *stable.poolInfos, prediction.domainID, domBlock );
//	compute domain sums
	if (!isRegular)
		domBlock= adjustDomainForIncompleteRange
		( powers[stable.rangeBlock->level], prediction.rotation, domBlock );
	Real dSum, d2Sum;
	pool.getSums(domBlock,dSum,d2Sum);
//	compute the denominator common to most formulas
	Real test= stable.pixCount*d2Sum - sqr(dSum);
	if (test<=0) // skip too flat domains
		return;
	Real denom= 1/test;
	
//	compute the sum of products of pixels
	Real rdSum= walkOperateCheckRotate
	( Checked<const SReal>(stable.rangePixels, *stable.rangeBlock), RDSummer<Real>()
	, bogoCast(pool.pixels), domBlock, prediction.rotation ) .result();

	Real nRDs_RsDs= stable.pixCount*rdSum - stable.rSum*dSum;
//	check for negative linear coefficients (if needed)
	if (!allowInversion)
		if (nRDs_RsDs<0)
			return;
//	compute the square of linear coefficient if needed (for restricting or penalty)
	Real linCoeff2 DEBUG_ONLY(= numeric_limits<Real>::quiet_NaN() );
	if ( restrictMaxLinCoeff || bigScalePenalty )
		linCoeff2= stable.rnDev2 * denom;
	if (restrictMaxLinCoeff)
		if ( linCoeff2 > stable.maxLinCoeff2 )
			return;
		
	float optSE DEBUG_ONLY(= numeric_limits<Real>::quiet_NaN() );
	
	if (quantErrors) {
		optSE= stable.qrAvg * ( stable.pixCount*stable.qrAvg - ldexp(stable.rSum,1) )
			+ stable.r2Sum + stable.qrDev 
				* ( stable.pixCount*stable.qrDev - ldexp( abs(nRDs_RsDs)*sqrt(denom), 1 ) );
		
		debugStop();
	} else { // !quantErrors
	/*
	//	normal SE computing
		Real nsxy= stable.pixCount*rdSum;
	    Real sxsy= dSum*stable.rSum;
	    optSE= ( rdSum*(ldexp(sxsy,1)-nsxy) - d2Sum*sqr(stable.rSum) ) 
	    	*denom +stable.r2Sum;
	*/
		
	//	assuming different linear coeffitient
		Real inner= stable.rnDev2 - stable.rnDev*abs(nRDs_RsDs)*sqrt(denom);
		optSE= ldexp( inner, 1 ) / stable.pixCount;
	}
	
//	add big-scaling penalty if needed
	if (bigScalePenalty)
		optSE+= linCoeff2 * targetSE * sqr(pool.contrFactor) * stable.bigScaleCoeff;
	
/*
//	check for too big linear coefficients (if needed)
	if (restrictMaxLinCoeff)
		if ( stable.qrDev2*denom*sqr(stable.pixCount) > stable.maxLinCoeff2 )
			return;
//	compute the SquareError
	float optSE;
	if (quantErrors) {
		Real rDev2Denom= stable.qrDev2*denom;
		optSE= stable.r2Sum + stable.pixCount*stable.qrDev2
		+ stable.qrAvg*(stable.pixCount*stable.qrAvg-ldexp(stable.rSum,1))
		+ ldexp(nRDs_RsDs,1)*sqrt(rDev2Denom);
		if (bigScalePenalty)
			optSE+= (rDev2Denom*sqr(stable.pixCount))
			* (stable.bigScaleCoeff*pool.contrFactor);
	} else { // !quantErrors
		Real common= rdSum*(stable.rSum*dSum-nRDs_RsDs) - d2Sum*sqr(stable.rSum);
		if (bigScalePenalty)
			optSE= ( (stable.bigScaleCoeff*sqr(nRDs_RsDs)) * (denom*pool.contrFactor)
			+ common ) * denom + stable.r2Sum;
		else
			optSE= common*denom + stable.r2Sum;
	}
	
	*/
//*/
//	test if the error is the best so far
	if ( optSE < best.error ) {
		best.prediction()= prediction;
		best.error= maxSE2Predict= optSE;
		best.inverted= nRDs_RsDs<0;
		
		#ifndef NDEBUG
		best.rdSum= rdSum;
		bdSum= dSum;
		bd2Sum= d2Sum;
		#endif
	}
} // EncodingInfo::exactCompareProc method


float MStandardEncoder::findBestSE(const RangeNode &range) {
	assert( planeBlock && !stdRangeSEs.empty() && !range.encoderData );

//	initialize an encoding-info object
	EncodingInfo info;

	info.stable.rangeBlock=		&range;
	info.stable.rangePixels=	bogoCast(planeBlock->pixels);
	info.stable.pools=			&planeBlock->domains->getPools();

	assert( range.level < (int)levelPoolInfos.size() );
	info.stable.poolInfos=		&levelPoolInfos[range.level];
//	check the level has been initialized
	if ( info.stable.poolInfos->empty() )
		buildPoolInfos4aLevel(range.level), assert( !info.stable.poolInfos->empty() );

	info.stable.allowRotations=	settingsInt(AllowedRotations);
	info.stable.quantError=		settingsInt(AllowedQuantError);
	info.stable.allowInversion=	settingsInt(AllowedInversion);
	info.stable.isRegular=		range.isRegular();
	{
		Real coeff= settingsFloat(MaxLinCoeff);
		info.stable.maxLinCoeff2=	coeff==MaxLinCoeff_none ? -1 : sqr(coeff);
	}
	info.stable.bigScaleCoeff=	settingsFloat(BigScaleCoeff);

	info.stable.rSum=		planeBlock->getSum(range,BlockSummer::Values);
	info.stable.r2Sum=		planeBlock->getSum(range,BlockSummer::Squares);
	info.stable.pixCount=	range.size();
	info.stable.rnDev2=		info.stable.pixCount*info.stable.r2Sum - sqr(info.stable.rSum);
	info.stable.rnDev=		sqrt(info.stable.rnDev2);
	{
		Quantizer::Average quantAvg( settingsInt(QuantStepLog_avg) );
		Quantizer::Deviation quantDev( settingsInt(QuantStepLog_dev) );
		Real average= info.stable.rSum / info.stable.pixCount;
		info.stable.qrAvg= quantAvg.qRound(average);

		Real variance= info.stable.r2Sum/info.stable.pixCount - sqr(average);
		Real deviance= variance>0 ? sqrt(variance) : 0;
		int qrDev= quantDev.quant(deviance);
	//	if we have too little deviance or no domain pool for that big level or no domain in the pool
		if ( !qrDev || range.level >= (int)levelPoolInfos.size()
		|| !info.stable.poolInfos->back().indexBegin ) { // -> no domain block, only average
			info.stable.qrDev= info.stable.qrDev2= 0;
			info.best.error= info.stable.quantError
				? info.stable.r2Sum + info.stable.qrAvg
					*( info.stable.pixCount*info.stable.qrAvg - ldexp(info.stable.rSum,1) )
				: variance*info.stable.pixCount;
			range.encoderData= info.initRangeInfo( rangeInfoAlloc.make() );
			return range.encoderData->bestSE;
		} else { // the regular case, with nonzero quantized deviance
			info.stable.qrDev= quantDev.dequant(qrDev);
			info.stable.qrDev2= sqr(info.stable.qrDev);
		}
	}

	info.targetSE= info.maxSE2Predict= info.stable.isRegular ? stdRangeSEs[range.level]
		: planeBlock->moduleQ2SE->rangeSE( planeBlock->quality, range.size() );
	info.selectExactCompareProc();

//	create and initialize a new predictor
	auto_ptr<IStdEncPredictor::OneRangePred> predictor
	( modulePredictor()->newPredictor(info.stable) );
	typedef IStdEncPredictor::Predictions Predictions;
	Predictions predicts;

//	get and process prediction chunks until an empty one is returned
	while ( !predictor->getChunk(info.maxSE2Predict,predicts).empty() )
		for (Predictions::iterator it=predicts.begin(); it!=predicts.end(); ++it)
			info.exactCompare(*it);

//	store the important info and return the error
	range.encoderData= info.initRangeInfo( rangeInfoAlloc.make() );
	return info.best.error;
}

void MStandardEncoder::buildPoolInfos4aLevel(int level) {
//	get the real maximum domain count (divide by the number of rotations)
	int domainCountLog2= planeBlock->domainCountLog2;
	if ( settingsInt(AllowedRotations) )
		domainCountLog2-= 3;
//	get the per-domain-pool densities
	vector<short> densities= planeBlock->domains->getLevelDensities( level, domainCountLog2 );
//	store it in the this-level infos with beginIndex values for all pools
	vector<LevelPoolInfo> &poolInfos= levelPoolInfos[level];
	assert( poolInfos.empty() );
	const ISquareDomains::PoolList &pools= planeBlock->domains->getPools();

	int domainSize= powers[level];
	int poolCount= pools.size();
	poolInfos.resize(poolCount+1);

	int domCount= poolInfos[0].indexBegin= 0; // accumulated count
	for (int i=0; i<poolCount; ++i) {
		int dens= poolInfos[i].density= densities[i];
		assert(dens>=0);
		if (dens) // if dens==0, there are no domains -> no increase
			domCount+= getCountForDensity2D( pools[i].width, pools[i].height, dens, domainSize );
		poolInfos[i+1].indexBegin= domCount;
	}
	poolInfos[poolCount].density= -1;
}

void MStandardEncoder::writeSettings(ostream &file) {
	assert( modulePredictor() && moduleCodec(true) && moduleCodec(false) );
//	put settings needed for decoding
	put<Uchar>( file, settingsInt(AllowedRotations) );
	put<Uchar>( file, settingsInt(AllowedInversion) );
	put<Uchar>( file, settingsInt(QuantStepLog_avg) );
	put<Uchar>( file, settingsInt(QuantStepLog_dev) );
//	put ID's of connected modules
	//the predictor module doesn't need to be known
	file_saveModuleType( file, ModuleCodecAvg );
	file_saveModuleType( file, ModuleCodecDev );
//	put the settings of connected modules
	moduleCodec(true)->writeSettings(file);
	moduleCodec(false)->writeSettings(file);
}
void MStandardEncoder::readSettings(istream &file) {
	assert( !modulePredictor() && !moduleCodec(true) && !moduleCodec(false) );
//	get settings needed for decoding
	settingsInt(AllowedRotations)= get<Uchar>(file);
	settingsInt(AllowedInversion)= get<Uchar>(file);
	settingsInt(QuantStepLog_avg)= get<Uchar>(file);
	settingsInt(QuantStepLog_dev)= get<Uchar>(file);
//	create connected modules
	//the predictor module doesn't need to be known
	file_loadModuleType( file, ModuleCodecAvg );
	file_loadModuleType( file, ModuleCodecDev );
//	get the settings of connected modules
	moduleCodec(true)->readSettings(file);
	moduleCodec(false)->readSettings(file);
}

void MStandardEncoder::writeData(ostream &file,int phase) {
	typedef RangeList::const_iterator RLcIterator;
	assert( moduleCodec(true) && moduleCodec(false) );
	assert( planeBlock && planeBlock->ranges && planeBlock->encoder==this );
	const RangeList &ranges= planeBlock->ranges->getRangeList();
	assert( !ranges.empty() );

	switch(phase) {
		case 0: { // save the averages
		//	get the list
			vector<int> averages;
			averages.reserve(ranges.size());
			Quantizer::Average quant( settingsInt(QuantStepLog_avg) );
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				assert( *it && (*it)->encoderData );
				const RangeInfo *info= static_cast<RangeInfo*>( (*it)->encoderData );
				averages.push_back( quant.quant(info->qrAvg) );
			}
		//	pass it to the codec
			moduleCodec(true)->setPossibilities( powers[settingsInt(QuantStepLog_avg)] );
			moduleCodec(true)->encode( averages, file );
			break;
		}

		case 1: { // save the deviations
		//	get the list
			vector<int> devs;
			devs.reserve(ranges.size());
			Quantizer::Deviation quant( settingsInt(QuantStepLog_dev) );
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				assert( *it && (*it)->encoderData );
				const RangeInfo *info= static_cast<RangeInfo*>( (*it)->encoderData );
				int dev= ( info->domainID>=0 ? quant.quant(sqrt(info->qrDev2)) : 0 );
				devs.push_back(dev);
			}
		//	pass it to the codec
			moduleCodec(false)->setPossibilities( powers[settingsInt(QuantStepLog_dev)] );
			moduleCodec(false)->encode( devs, file );
			break;
		}

		case 2: { // save the rest
			BitWriter stream(file);
		//	put the inversion bits if inversion is allowed
			if ( settingsInt(AllowedInversion) )
				for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
					assert( *it && (*it)->encoderData );
					const RangeInfo *info= static_cast<RangeInfo*>( (*it)->encoderData );
					if ( info->domainID >= 0 )
						stream.putBits(info->inverted,1);
				}
		//	put the rotation bits if rotations are allowed
			if ( settingsInt(AllowedRotations) )
				for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
					assert( *it && (*it)->encoderData );
					const RangeInfo *info= static_cast<RangeInfo*>( (*it)->encoderData );
					if ( info->domainID >= 0 ) {
						assert( 0<=info->rotation && info->rotation<8 );
						stream.putBits( info->rotation, 3 );
					}
				}
		//	find out bits needed to store IDs of domains for every level
			vector<int> domBits;
			domBits.resize( levelPoolInfos.size() );
			for (size_t i=0; i<levelPoolInfos.size(); ++i) {
				int count= levelPoolInfos[i].empty() ? 0 : levelPoolInfos[i].back().indexBegin;
				assert(count>=0);
				domBits[i]= count ? log2ceil(count) : 0;
			}
		//	put the domain bits
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				assert( *it && (*it)->encoderData );
				const RangeInfo *info= static_cast<RangeInfo*>( (*it)->encoderData );
				int domID= info->domainID;
				if ( domID >= 0 ) {
					int bits= domBits[ (*it)->level ];
					assert( 0<=domID && domID<powers[bits] );
					if (bits>0) 
						stream.putBits( domID, bits );
				}
			}
			break;
		}
		default:
			assert(false);
	}
}
void MStandardEncoder::readData(istream &file,int phase) {
	typedef RangeList::const_iterator RLcIterator;
	assert( moduleCodec(true) && moduleCodec(false) );
	assert( planeBlock && planeBlock->ranges && planeBlock->encoder==this );
	const RangeList &ranges= planeBlock->ranges->getRangeList();
	assert( !ranges.empty() );

	switch(phase) {
		case 0: { // create the infos and load the averages
		//	get the list of averages from the codec
			vector<int> averages;
			moduleCodec(true)->setPossibilities( powers[settingsInt(QuantStepLog_avg)] );
			moduleCodec(true)->decode( file, ranges.size(), averages );
		//	iterate the list, create infos and fill the with dequantized averages
			Quantizer::Average quant( settingsInt(QuantStepLog_avg) );
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				assert( *it && !(*it)->encoderData );
				RangeInfo *info= rangeInfoAlloc.make();
				(*it)->encoderData= info;
				info->qrAvg= quant.dequant(averages[it-ranges.begin()]);
				info->bestSE= -1;
			}
			break;
		}

		case 1: { // load the deviations
		//	get the list of deviations from the codec
			vector<int> devs;
			moduleCodec(false)->setPossibilities( powers[settingsInt(QuantStepLog_dev)] );
			moduleCodec(false)->decode( file, ranges.size(), devs );
		//	iterate the list, create infos and fill the with dequantized deviations
			Quantizer::Deviation quant( settingsInt(QuantStepLog_dev) );
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				assert( *it && (*it)->encoderData );
				RangeInfo *info= static_cast<RangeInfo*>( (*it)->encoderData );
				int quantDev= devs[it-ranges.begin()];
				if (quantDev)
					info->qrDev2= sqr(quant.dequant(quantDev));
				else {
					info->qrDev2= 0;
					//info->rotation= info->domainID= -1;  no need, they already equal -1
				}
			}
			break;
		}

		case 2: { // load the rest
			BitReader stream(file);
		//	get the inversion bits if inversion is allowed
			if ( settingsInt(AllowedInversion) )
				for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
					assert( *it && (*it)->encoderData );
					RangeInfo *info= static_cast<RangeInfo*>( (*it)->encoderData );
					if (info->qrDev2)
						info->inverted= stream.getBits(1);
				}
		//	get the rotation bits if rotations are allowed
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				assert( *it && (*it)->encoderData );
				RangeInfo *info= static_cast<RangeInfo*>( (*it)->encoderData );
				if (info->qrDev2)
					info->rotation= settingsInt(AllowedRotations) ? stream.getBits(3) : 0;
			}
		//	find out bits needed to store IDs of domains for every level
			vector<int> domBits( levelPoolInfos.size(), -1 );
		//	get the domain bits
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				assert( *it && (*it)->encoderData );
				RangeInfo *info= static_cast<RangeInfo*>( (*it)->encoderData );
				if (!info->qrDev2)
					continue;
				int level= (*it)->level;
				int bits= domBits[level];
				if (bits<0) {
				//	yet unused level -> initialize it and get the right bits
					buildPoolInfos4aLevel(level);
					bits= domBits[level]
					= log2ceil( levelPoolInfos[level].back().indexBegin );
					assert( bits>0 );
				}
			//	get the domain ID, check it's OK and store it in the range's info
				int domID= stream.getBits(bits);
				checkThrow( domID < levelPoolInfos[level].back().indexBegin );
				info->domainID= domID;
			}
		//	initialize decoding accelerators for domain-to-range mappings
			initRangeInfoAccelerators();
			break;
		}
		default:
			assert(false);
	}
}

void MStandardEncoder::decodeAct( DecodeAct action, int count ) {
//	do some checks
	assert( planeBlock && planeBlock->ranges && planeBlock->encoder==this );
	const RangeList &ranges= planeBlock->ranges->getRangeList();
	assert( !ranges.empty() );

	switch (action) {
	default:
		assert(false);
	case Clear:
		assert(count==1);
		fillSubMatrix( planeBlock->pixels, *planeBlock, 0.5f );
		planeBlock->summers_invalidate();
		break;
	case Iterate:
		assert(count>0);
		do {
		//	prepare the domains, iterate each range block
			planeBlock->domains->fillPixelsInPools(*planeBlock);
			for (RangeList::const_iterator it=ranges.begin(); it!=ranges.end(); ++it) {
				const RangeInfo &info= *(const RangeInfo*) (*it)->encoderData;
				if ( info.domainID < 0 ) { // no domain - constant color
					fillSubMatrix<SReal>( planeBlock->pixels, **it, info.qrAvg );
					continue;
				}
			//	get domain sums and the pixel count
				Real dSum, d2Sum;
				info.decAccel.pool->getSums( info.decAccel.domBlock, dSum, d2Sum );
				Real pixCount= (*it)->size();
				
				
				Real linCoeff=(info.inverted ? -pixCount : pixCount)
					* sqrt( info.qrDev2 / ( pixCount*d2Sum - sqr(dSum) ) );
				 /*
					info.exact.linCoeff;
				*/
				if ( !isnormal(linCoeff) || !linCoeff ) {
					fillSubMatrix<SReal>( planeBlock->pixels, **it, info.qrAvg );
					continue;
				}
				Real constCoeff= info.qrAvg/*info.exact.avg*/ - linCoeff*dSum/pixCount;

				using namespace MatrixWalkers;
				MulAddCopyChecked<Real> oper( linCoeff, constCoeff, 0, 1 );
				walkOperateCheckRotate( Checked<SReal>(planeBlock->pixels, **it), oper
				, info.decAccel.pool->pixels, info.decAccel.domBlock, info.rotation );
			}
		} while (--count);
		break;
	}
}

void MStandardEncoder::initRangeInfoAccelerators() {
//	get references that are the same for all range blocks
	const RangeList &ranges= planeBlock->ranges->getRangeList();
	const ISquareDomains::PoolList &pools= planeBlock->domains->getPools();
//	iterate over the ranges (and their accelerators)
	for ( RangeList::const_iterator it=ranges.begin(); it!=ranges.end(); ++it ) {
	//	get range level, reference to the range's info and to the level's pool infos
		int level= (*it)->level;
		RangeInfo &info= *static_cast<RangeInfo*>( (*it)->encoderData );
		if ( info.domainID < 0 )
			continue;
		const PoolInfos &poolInfos= levelPoolInfos[level];
	//	build pool infos for the level (if neccesary), fill info's accelerators
		if ( poolInfos.empty() )
			buildPoolInfos4aLevel(level), assert( !poolInfos.empty() );
		info.decAccel.pool= &getDomainData
		( **it, pools, poolInfos, info.domainID, info.decAccel.domBlock );
	//	adjust domain's block if the range isn't regular
		if ( !(*it)->isRegular() )
			adjustDomainForIncompleteRange(powers[level],info.rotation,info.decAccel.domBlock);
	}
}
