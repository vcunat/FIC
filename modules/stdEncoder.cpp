#include <memory> // auto_ptr

#include "stdEncoder.h"
#include "../fileUtil.h"

using namespace std;

/// \file

/** Quantizing stuff used by MStdEncoder \relates MStdEncoder */
namespace Quantizer {
	/** Quantizes f that belongs to [0;possib/2^scale] into [0;possib-1] */
	inline int quantizeByPower(Real f,int scale,int possib) {
		ASSERT( f>=0 && f<=possib/(Real)powers[scale] );
		int result= (int)trunc(ldexp(f,scale));
		ASSERT( result>=0 && result<=possib );
		return result<possib ? result : --result;
	}
	/** Performs the opposite to ::quantizeByPower */
	inline Real dequantizeByPower(int i,int scale,int DEBUG_ONLY(possib)) {
		ASSERT( i>=0 && i<possib );
		Real result= ldexp(i+Real(0.5),-scale);
		ASSERT( result>=0 && result<= possib/(Real)powers[scale] );
		return result;
	}

	class QuantBase {
	protected:
		int scale	///  how much should the values be scaled (like in ::quantizeByPower)
		, possib;	///< the number of possibilities for quantization
	public:
		/** Quantizes a value */
		int quant(Real val) const
			{ return quantizeByPower(val,scale,possib); }
		/** Dequantizes a value */
		Real dequant(int i) const
			{ return dequantizeByPower(i,scale,possib); }
		/** Quant-rounds a value - returns its state after quantization and dequantization */
		Real qRound(Real val) const
			{ return dequant(quant(val)); }
	};

	/** (De)%Quantizer for range-block averages, only initializes QuantBase correctly */
	class Average: public QuantBase {
	public:
		Average(int possibLog2) {
			ASSERT(possibLog2>0);
		//	the average is from [0,1]
			scale= possibLog2;
			possib= powers[possibLog2];
		}	
	};

	/** (De)%Quantizer for range-block deviations, only initializes QuantBase correctly */
	class Deviation: public QuantBase {
	public:
		Deviation(int possibLog2) {
			ASSERT(possibLog2>0);
		//	the deviation is from [0,0.5] -> we have to scale twice more (log2+=1)
			scale= possibLog2+1;
			possib= powers[possibLog2];
		}
	};
} // Quantizer namespace


/** Adjust block of a domain to be mapped equally on an incomplete range (depends on rotation) */
inline static Block adjustDomainForIncompleteRange( Block range, int rotation, Block domain ) {
	// 0, 0T: top left
	// 1, 1T: top right
	// 2, 2T: bottom right
	// 3, 3T: bottom left

	ASSERT( 0<=rotation && rotation<8 );
	int rotID= rotation/2;

	int xSize= range.width();
	int ySize= range.height();
	
	int magic= rotID+rotation%2;
	if ( magic==1 || magic==3 ) // for rotations 0T, 1, 2T, 3 -> swapped x and y
		swap(xSize,ySize);

	if ( rotID>1 )	// domain rotations 2, 3, 2T, 3T -> aligned to the bottom
		domain.y0= domain.yend-ySize;
	else			// other rotations are aligned to the top
		domain.yend= domain.y0+ySize;

	if ( rotID==1 || rotID==2 )	// rotations 1, 2, 1T, 2T -> aligned to the right
		domain.x0= domain.xend-xSize;
	else						// other rotations are aligned to the left
		domain.xend= domain.x0+xSize;
	
	return domain;
}

////	Member methods

void MStdEncoder::initialize( IRoot::Mode mode, PlaneBlock &planeBlock_ ) {
	planeBlock= &planeBlock_;
//	prepare the domains-module
	planeBlock->domains->initPools(*planeBlock);

	int maxLevel= 1 +log2ceil(max( planeBlock->width, planeBlock->height ));
//	prepare levelPoolInfos
	levelPoolInfos.resize(maxLevel);

	if (mode==IRoot::Encode) {
		typedef ISquareDomains::PoolList PoolList;
	//	prepare the domains
		planeBlock->domains->fillPixelsInPools(*planeBlock);
		for_each( planeBlock->domains->getPools(), mem_fun_ref(&Pool::summers_makeValid) );
	//	initialize the range summers
		planeBlock->summers_makeValid();

	//	prepare maximum SquareErrors allowed for regular range blocks
		stdRangeSEs.resize(maxLevel+1);
		planeBlock->settings->moduleQ2SE->regularRangeErrors
			( planeBlock->settings->quality, maxLevel+1, &stdRangeSEs.front() );
	}
}


namespace NOSPACE {
	struct LevelPoolInfo_indexComparator {
		typedef ISquareEncoder::LevelPoolInfo LevelPoolInfo;
		bool operator()( const LevelPoolInfo &a, const LevelPoolInfo &b )
			{ return a.indexBegin < b.indexBegin; }
	};
}
MStdEncoder::PoolInfos::const_iterator MStdEncoder
::getPoolFromDomID( int domID, const PoolInfos &poolInfos ) {
	ASSERT( domID>=0 && domID<poolInfos.back().indexBegin );
//	get the right pool
	PoolInfos::value_type toFind;
	toFind.indexBegin= domID;
	return upper_bound( poolInfos.begin(), poolInfos.end(), toFind
		, LevelPoolInfo_indexComparator() ) -1;
}

const ISquareDomains::Pool& MStdEncoder::getDomainData
( const RangeNode &rangeBlock, const ISquareDomains::PoolList &pools
, const PoolInfos &poolInfos, int domIndex, int zoom, Block &block ) {

//	get the pool
	PoolInfos::const_iterator it= getPoolFromDomID( domIndex, poolInfos );
	const Pool &pool= pools[ it - poolInfos.begin() ];
//	get the coordinates
	int indexInPool= domIndex - it->indexBegin;
	ASSERT( indexInPool>=0 && indexInPool<(it+1)->indexBegin );
	int sizeNZ= powers[rangeBlock.level-zoom];
	int zoomFactor= powers[zoom];
//	changed: the domains are mapped along columns and not rows
	int domsInCol= getCountForDensity( pool.height/zoomFactor, it->density, sizeNZ );
	block.x0= (indexInPool/domsInCol)*it->density*zoomFactor;
	block.y0= (indexInPool%domsInCol)*it->density*zoomFactor;
	block.xend= block.x0+powers[rangeBlock.level];
	block.yend= block.y0+powers[rangeBlock.level];

	return pool;
}


namespace NOSPACE {
	typedef IStdEncPredictor::NewPredictorData StableInfo;

	/** Information about the best (found so far) domain block for a range block */
	struct BestInfo: public IStdEncPredictor::Prediction {
		float error;	///< The square error of the mapping
		bool inverted;	///< Whether the colours are mapped with a negative linear coefficient
		#ifndef NDEBUG
		Real rdSum		///  
		, dSum			///  Sum of all pixels in best domain
		, d2Sum;		///< Sum of squares of all pixesl in the best domain
		#endif

		/** Only initializes ::error to the maximum float value */
		BestInfo()
		: Prediction(), error( numeric_limits<float>::infinity() ) {}
		/** Only returns reference to *this typed as IStdEncPredictor::Prediciton */
		IStdEncPredictor::Prediction& prediction() 
			{ return *this; }
	};
} // namespace NOSPACE

/** Structure constructed for a range to try domains */
struct MStdEncoder::EncodingInfo {
	/** The type for exact-comparing methods */
	typedef bool (EncodingInfo::*ExactCompareProc)( Prediction prediction );

private:
	static const ExactCompareProc exactCompareArray[];	///< Possible comparing methods

	ExactCompareProc selectedProc;						///< Selected comparing method
public:
	StableInfo stable;	///< Information only depending on the range (and not domain) block
	BestInfo best;		///< Information about the best (at the moment) mapping found
	float targetSE;		///< The target SE (square error) for the range

public:
	/** Only nulls ::selectedProc */
	EncodingInfo()
	: selectedProc(0) {}

	/** Initializes a RangeInfo object from \p this */
	RangeInfo* initRangeInfo(RangeInfo *ri) const {
		ri->bestSE=		best.error;
		ri->domainID=	best.domainID;
		ri->rotation=	best.rotation;
		ri->qrAvg=		stable.qrAvg;
		ri->qrDev2=		stable.qrDev2;
		ri->inverted=	best.inverted;
		#ifndef NDEBUG
		ri->exact.avg=	stable.rSum/stable.pixCount;
		ri->exact.dev2=	stable.r2Sum/stable.pixCount - sqr(ri->exact.avg);
		ri->exact.linCoeff= ( stable.pixCount*best.rdSum - stable.rSum*best.dSum )
		/ ( stable.pixCount*best.d2Sum - sqr(best.dSum) );
		ri->exact.constCoeff= ri->exact.avg - ri->exact.linCoeff*best.dSum/stable.pixCount;
		#endif
		return ri;
	}

	/** Selects the right comparing method according to ::stable */
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
	bool exactCompare(Prediction prediction)
		{ return (this->* selectedProc)(prediction); }

private:
	/** Template for all the comparing methods */
	template < bool quantErrors, bool allowInversion, bool isRegular
	, bool restrictMaxLinCoeff, bool bigScalePenalty >
	bool exactCompareProc( Prediction prediction );
}; // EncodingInfo struct

#define ALTERNATE_0(name,params...) name<params>
#define ALTERNATE_1(name,params...) ALTERNATE_0(name,##params,false), ALTERNATE_0(name,##params,true)
#define ALTERNATE_2(name,params...) ALTERNATE_1(name,##params,false), ALTERNATE_1(name,##params,true)
#define ALTERNATE_3(name,params...) ALTERNATE_2(name,##params,false), ALTERNATE_2(name,##params,true)
#define ALTERNATE_4(name,params...) ALTERNATE_3(name,##params,false), ALTERNATE_3(name,##params,true)
#define ALTERNATE_5(name,params...) ALTERNATE_4(name,##params,false), ALTERNATE_4(name,##params,true)
const MStdEncoder::EncodingInfo::ExactCompareProc MStdEncoder::EncodingInfo::exactCompareArray[]
	= {ALTERNATE_5(&MStdEncoder::EncodingInfo::exactCompareProc)};
#undef ALTERNATE_0
#undef ALTERNATE_1
#undef ALTERNATE_2
#undef ALTERNATE_3
#undef ALTERNATE_4
#undef ALTERNATE_5

template< bool quantErrors, bool allowInversion, bool isRegular
, bool restrictMaxLinCoeff, bool bigScalePenalty >
bool MStdEncoder::EncodingInfo::exactCompareProc( Prediction prediction ) {
	using namespace MatrixWalkers;
//	find out which domain was predicted (pixel matrix and position within it)
	Block domBlock;
	const Pool &pool= getDomainData( *stable.rangeBlock, *stable.pools, *stable.poolInfos
	, prediction.domainID, 0/*zoom*/, domBlock );
//	compute domain sums
	if (!isRegular)
		domBlock= adjustDomainForIncompleteRange
			( *stable.rangeBlock, prediction.rotation, domBlock );
	Real dSum, d2Sum;
	pool.getSums(domBlock).unpack(dSum,d2Sum);
//	compute the denominator common to most formulas
	Real test= stable.pixCount*d2Sum - sqr(dSum);
	if (test<=0) // skip too flat domains
		return false;
	Real denom= 1/test;

//	compute the sum of products of pixels
	Real rdSum= walkOperateCheckRotate
	( Checked<const SReal>(stable.rangePixels->pixels, *stable.rangeBlock), RDSummer<Real,SReal>()
	, pool.pixels, domBlock, prediction.rotation ) .result();

	Real nRDs_RsDs= stable.pixCount*rdSum - stable.rSum*dSum;
//	check for negative linear coefficients (if needed)
	if (!allowInversion)
		if (nRDs_RsDs<0)
			return false;
//	compute the square of linear coefficient if needed (for restricting or penalty)
	Real linCoeff2 DEBUG_ONLY(= numeric_limits<Real>::quiet_NaN() );
	if ( restrictMaxLinCoeff || bigScalePenalty )
		linCoeff2= stable.rnDev2 * denom;
	if (restrictMaxLinCoeff)
		if ( linCoeff2 > stable.maxLinCoeff2 )
			return false;

	float optSE DEBUG_ONLY(= numeric_limits<Real>::quiet_NaN() );

	if (quantErrors) {
		optSE= stable.qrAvg * ( stable.pixCount*stable.qrAvg - ldexp(stable.rSum,1) )
			+ stable.r2Sum + stable.qrDev
				* ( stable.pixCount*stable.qrDev - ldexp( abs(nRDs_RsDs)*sqrt(denom), 1 ) );
	} else { // !quantErrors
	//	assuming different linear coeffitient
		Real inner= stable.rnDev2 - stable.rnDev*abs(nRDs_RsDs)*sqrt(denom);
		optSE= ldexp( inner, 1 ) / stable.pixCount;
	}

//	add big-scaling penalty if needed
	if (bigScalePenalty)
		optSE+= linCoeff2 * targetSE * pool.contrFactor * stable.bigScaleCoeff;

//	test if the error is the best so far
	if ( optSE < best.error ) {
		best.prediction()= prediction;
		best.error= optSE;
		best.inverted= nRDs_RsDs<0;

		#ifndef NDEBUG
		best.rdSum= rdSum;
		best.dSum= dSum;
		best.d2Sum= d2Sum;
		#endif
		
		return true;
	} else
		return false;
} // EncodingInfo::exactCompareProc method


float MStdEncoder::findBestSE(const RangeNode &range,bool allowHigherSE) {
	ASSERT( planeBlock && !stdRangeSEs.empty() && !range.encoderData );
	const IColorTransformer::PlaneSettings *plSet= planeBlock->settings;

//	initialize an encoding-info object
	EncodingInfo info;

	info.stable.rangeBlock=		&range;
	info.stable.rangePixels=	planeBlock;
	info.stable.pools=			&planeBlock->domains->getPools();

	ASSERT( range.level < (int)levelPoolInfos.size() );
	info.stable.poolInfos=		&levelPoolInfos[range.level];
//	check the level has been initialized
	if ( info.stable.poolInfos->empty() ) {
		buildPoolInfos4aLevel(range.level);
		ASSERT( !info.stable.poolInfos->empty() );
	}

	info.stable.allowRotations=	settingsInt(AllowedRotations);
	info.stable.quantError=		settingsInt(AllowedQuantError);
	info.stable.allowInversion=	settingsInt(AllowedInversion);
	info.stable.isRegular=		range.isRegular();
	{
		Real coeff= settingsFloat(MaxLinCoeff);
		info.stable.maxLinCoeff2=	coeff==MaxLinCoeff_none ? -1 : sqr(coeff);
	}
	info.stable.bigScaleCoeff=	settingsFloat(BigScaleCoeff);

	planeBlock->getSums(range).unpack( info.stable.rSum, info.stable.r2Sum );
	info.stable.pixCount=	range.size();
	info.stable.rnDev2=		info.stable.pixCount*info.stable.r2Sum - sqr(info.stable.rSum);
	if (info.stable.rnDev2<0)
		info.stable.rnDev2= 0;
	info.stable.rnDev=		sqrt(info.stable.rnDev2);

	Real variance;
	{
		Quantizer::Average quantAvg( settingsInt(QuantStepLog_avg) );
		Quantizer::Deviation quantDev( settingsInt(QuantStepLog_dev) );
		Real average= info.stable.rSum / info.stable.pixCount;
		info.stable.qrAvg= quantAvg.qRound(average);

		variance= info.stable.r2Sum/info.stable.pixCount - sqr(average);
		Real deviance= variance>0 ? sqrt(variance) : 0;
		int qrDev= quantDev.quant(deviance);
	//	if we have too little deviance or no domain pool for that big level or no domain in the pool
		if ( !qrDev || range.level >= (int)levelPoolInfos.size()
		|| info.stable.poolInfos->back().indexBegin <= 0 ) // -> no domain block, only average
			goto returning; // skips to the end, assigning a constant block
		else { // the regular case, with nonzero quantized deviance
			info.stable.qrDev= quantDev.dequant(qrDev);
			info.stable.qrDev2= sqr(info.stable.qrDev);
		}
	}

	info.targetSE= info.best.error= info.stable.isRegular ? stdRangeSEs[range.level]
		: plSet->moduleQ2SE->rangeSE( plSet->quality, range.size() );
	ASSERT(info.targetSE>=0);
	if (allowHigherSE)
		info.best.error= numeric_limits<float>::infinity();
	info.selectExactCompareProc();

	{ // a goto-skippable block
	//	create and initialize a new predictor (in auto_ptr because of exceptions)
		auto_ptr<IStdEncPredictor::IOneRangePredictor> predictor
		( modulePredictor()->newPredictor(info.stable) );
		typedef IStdEncPredictor::Predictions Predictions;
		Predictions predicts;
	
		float sufficientSE= info.targetSE*settingsFloat(SufficientSEq);
	//	get and process prediction chunks until an empty one is returned
		while ( !predictor->getChunk(info.best.error,predicts).empty() )
			for (Predictions::iterator it=predicts.begin(); it!=predicts.end(); ++it) {
				bool betterSE= info.exactCompare(*it);
				if ( betterSE && info.best.error<=sufficientSE )
					goto returning;
			}
	}
		
	returning:
//	check the case that the predictor didn't return any domains (or jumped to returning:)
	if ( info.best.domainID < 0 ) { // a constant block
		info.stable.qrDev= info.stable.qrDev2= 0;
		info.best.error= info.stable.quantError
			? info.stable.r2Sum + info.stable.qrAvg
				*( info.stable.pixCount*info.stable.qrAvg - ldexp(info.stable.rSum,1) )
			: variance*info.stable.pixCount;
	}
//	store the important info and return the error
	range.encoderData= info.initRangeInfo( /*rangeInfoAlloc.make()*/ new RangeInfo );
	return info.best.error;
} // ::findBestSE method

void MStdEncoder::buildPoolInfos4aLevel(int level) {
//	get the real maximum domain count (divide by the number of rotations)
	int domainCountLog2= planeBlock->settings->domainCountLog2;
	if ( settingsInt(AllowedRotations) )
		domainCountLog2-= 3;
//	get the per-domain-pool densities
	vector<short> densities= planeBlock->domains->getLevelDensities(level,domainCountLog2);
//	storing the result in this-level infos with beginIndex values for all pools
	vector<LevelPoolInfo> &poolInfos= levelPoolInfos[level];
	ASSERT( poolInfos.empty() );
	const ISquareDomains::PoolList &pools= planeBlock->domains->getPools();
	int zoom= planeBlock->settings->zoom;

	int domainSizeNZ= powers[level-zoom];
	int poolCount= pools.size();
	poolInfos.resize(poolCount+1);

	int domCount= poolInfos[0].indexBegin= 0; // accumulated count
	for (int i=0; i<poolCount; ++i) {
		int dens= poolInfos[i].density= densities[i];
		ASSERT(dens>=0);
		if (dens) // if dens==0, there are no domains -> no increase
			domCount+= getCountForDensity2D( rShift(pools[i].width,zoom)
				, rShift(pools[i].height,zoom), dens, domainSizeNZ );
		poolInfos[i+1].indexBegin= domCount;
	}
	poolInfos[poolCount].density= -1;
}

void MStdEncoder::writeSettings(ostream &file) {
	ASSERT( /*modulePredictor() &&*/ moduleCodec(true) && moduleCodec(false) );
//	put settings needed for decoding
	put<Uchar>( file, settingsInt(AllowedRotations) );
	put<Uchar>( file, settingsInt(AllowedInversion) );
	put<Uchar>( file, settingsInt(QuantStepLog_avg) );
	put<Uchar>( file, settingsInt(QuantStepLog_dev) );
//	put ID's of connected modules (the predictor module doesn't need to be known)
	file_saveModuleType( file, ModuleCodecAvg );
	file_saveModuleType( file, ModuleCodecDev );
//	put the settings of connected modules
	moduleCodec(true)->writeSettings(file);
	moduleCodec(false)->writeSettings(file);
}
void MStdEncoder::readSettings(istream &file) {
	ASSERT( !modulePredictor() && !moduleCodec(true) && !moduleCodec(false) );
//	get settings needed for decoding
	settingsInt(AllowedRotations)= get<Uchar>(file);
	settingsInt(AllowedInversion)= get<Uchar>(file);
	settingsInt(QuantStepLog_avg)= get<Uchar>(file);
	settingsInt(QuantStepLog_dev)= get<Uchar>(file);
//	create connected modules (the predictor module isn't needed)
	file_loadModuleType( file, ModuleCodecAvg );
	file_loadModuleType( file, ModuleCodecDev );
//	get the settings of connected modules
	moduleCodec(true)->readSettings(file);
	moduleCodec(false)->readSettings(file);
}

void MStdEncoder::writeData(ostream &file,int phase) {
	typedef RangeList::const_iterator RLcIterator;
	ASSERT( moduleCodec(true) && moduleCodec(false) );
	ASSERT( planeBlock && planeBlock->ranges && planeBlock->encoder==this );
	const RangeList &ranges= planeBlock->ranges->getRangeList();
	ASSERT( !ranges.empty() );

	switch(phase) {
		case 0: { // save the averages
		//	get the list
			vector<int> averages;
			averages.reserve(ranges.size());
			Quantizer::Average quant( settingsInt(QuantStepLog_avg) );
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				ASSERT( *it && (*it)->encoderData );
				STREAM_POS(file);
				const RangeInfo *info= RangeInfo::get(*it);
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
				ASSERT( *it && (*it)->encoderData );
				STREAM_POS(file);
				const RangeInfo *info= RangeInfo::get(*it);
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
					ASSERT( *it && (*it)->encoderData );
					STREAM_POS(file);
					const RangeInfo *info= RangeInfo::get(*it);
					if ( info->domainID >= 0 )
						stream.putBits(info->inverted,1);
				}
		//	put the rotation bits if rotations are allowed
			if ( settingsInt(AllowedRotations) )
				for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
					ASSERT( *it && (*it)->encoderData );
					STREAM_POS(file);
					const RangeInfo *info= RangeInfo::get(*it);
					if ( info->domainID >= 0 ) {
						ASSERT( 0<=info->rotation && info->rotation<8 );
						stream.putBits( info->rotation, 3 );
					}
				}
		//	find out bits needed to store IDs of domains for every level
			vector<int> domBits;
			domBits.resize( levelPoolInfos.size() );
			for (int i=0; i<(int)levelPoolInfos.size(); ++i) {
				int count= levelPoolInfos[i].empty() ? 0 : levelPoolInfos[i].back().indexBegin;
				ASSERT(count>=0);
				STREAM_POS(file);
				domBits[i]= count ? log2ceil(count) : 0;
			}
		//	put the domain bits
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				ASSERT( *it && (*it)->encoderData );
				STREAM_POS(file);
				const RangeInfo *info= RangeInfo::get(*it);
				int domID= info->domainID;
				if ( domID >= 0 ) {
					int bits= domBits[ (*it)->level ];
					ASSERT( 0<=domID && domID<powers[bits] );
					if (bits>0)
						stream.putBits( domID, bits );
				}
			}
			break;
		}
		default:
			ASSERT(false);
	}
} // ::writeData method
void MStdEncoder::readData(istream &file,int phase) {
	typedef RangeList::const_iterator RLcIterator;
	ASSERT( moduleCodec(true) && moduleCodec(false) );
	ASSERT( planeBlock && planeBlock->ranges && planeBlock->encoder==this );
	const RangeList &ranges= planeBlock->ranges->getRangeList();
	ASSERT( !ranges.empty() );

	switch(phase) {
		case 0: { // create the infos and load the averages
		//	get the list of averages from the codec
			vector<int> averages;
			moduleCodec(true)->setPossibilities( powers[settingsInt(QuantStepLog_avg)] );
			moduleCodec(true)->decode( file, ranges.size(), averages );
		//	iterate the list, create infos and fill them with dequantized averages
			Quantizer::Average quant( settingsInt(QuantStepLog_avg) );
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				ASSERT( *it && !(*it)->encoderData );
				STREAM_POS(file);
				RangeInfo *info= new RangeInfo;//rangeInfoAlloc.make();
				(*it)->encoderData= info;
				info->qrAvg= quant.dequant(averages[it-ranges.begin()]);
				DEBUG_ONLY( info->bestSE= -1; ) // SE not needed for decoding,saving,...
				if ( !settingsInt(AllowedInversion) )
					info->inverted= false;
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
				ASSERT( *it && (*it)->encoderData );
				STREAM_POS(file);
				RangeInfo *info= RangeInfo::get(*it);
				int quantDev= devs[it-ranges.begin()];
				if (quantDev)
					info->qrDev2= sqr(quant.dequant(quantDev));
				else { // it's a flat block
					info->qrDev2= 0;
					
					DEBUG_ONLY( info->inverted= false; )
					ASSERT( info->rotation==-1 && info->domainID==-1 );
				}
			}
			break;
		}

		case 2: { // load the rest
			BitReader stream(file);
		//	get the inversion bits if inversion is allowed
			if ( settingsInt(AllowedInversion) )
				for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
					ASSERT( *it && (*it)->encoderData );
					STREAM_POS(file);
					RangeInfo *info= RangeInfo::get(*it);
					if (info->qrDev2)
						info->inverted= stream.getBits(1);
				}
		//	get the rotation bits if rotations are allowed
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				ASSERT( *it && (*it)->encoderData );
				STREAM_POS(file);
				RangeInfo *info= RangeInfo::get(*it);
				if (info->qrDev2)
					info->rotation= settingsInt(AllowedRotations) ? stream.getBits(3) : 0;
			}
		//	find out bits needed to store IDs of domains for every level
			vector<int> domBits( levelPoolInfos.size(), -1 );
		//	get the domain bits
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				ASSERT( *it && (*it)->encoderData );
				STREAM_POS(file);
				RangeInfo *info= RangeInfo::get(*it);
				if (!info->qrDev2)
					continue;
				int level= (*it)->level;
				int bits= domBits[level];
				if (bits<0) {
				//	yet unused level -> initialize it and get the right bits
					buildPoolInfos4aLevel(level);
					bits= domBits[level]
					= log2ceil( levelPoolInfos[level].back().indexBegin );
					ASSERT( bits>0 );
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
			ASSERT(false);
	}
} // ::readData method

void MStdEncoder::decodeAct( DecodeAct action, int count ) {
//	do some checks
	ASSERT( planeBlock && planeBlock->ranges && planeBlock->encoder==this );
	const RangeList &ranges= planeBlock->ranges->getRangeList();
	ASSERT( !ranges.empty() );

	switch (action) {
	default:
		ASSERT(false);
	case Clear:
		ASSERT(count==1);
		planeBlock->pixels.fillSubMatrix
			( Block(0,0,planeBlock->width,planeBlock->height), 0.5f );
		planeBlock->summers_invalidate();
		break;
	case Iterate:
		ASSERT(count>0);
		do {
		//	prepare the domains, iterate each range block
			planeBlock->domains->fillPixelsInPools(*planeBlock);
			for (RangeList::const_iterator it=ranges.begin(); it!=ranges.end(); ++it) {
				const RangeInfo &info= *RangeInfo::get(*it);
				if ( info.domainID < 0 ) { // no domain - constant color
					planeBlock->pixels.fillSubMatrix( **it, info.qrAvg );
					continue;
				}
			//	get domain sums and the pixel count
				Real dSum, d2Sum;
				info.decAccel.pool->summers_makeValid();
				info.decAccel.pool->getSums(info.decAccel.domBlock).unpack(dSum,d2Sum);
				Real pixCount= (*it)->size();
			//	find out the coefficients and handle constant blocks
				Real linCoeff= (info.inverted ? -pixCount : pixCount)
					* sqrt( info.qrDev2 / ( pixCount*d2Sum - sqr(dSum) ) );
				if ( !isnormal(linCoeff) || !linCoeff ) {
					planeBlock->pixels.fillSubMatrix( **it, info.qrAvg );
					continue;
				}
				Real constCoeff= info.qrAvg - linCoeff*dSum/pixCount;
			//	map the nonconstant blocks
				using namespace MatrixWalkers;
				MulAddCopyChecked<Real> oper( linCoeff, constCoeff, 0, 1 );
				walkOperateCheckRotate( Checked<SReal>(planeBlock->pixels, **it), oper
				, info.decAccel.pool->pixels, info.decAccel.domBlock, info.rotation );
			}
		} while (--count);
		break;
	} // switch (action)
} // ::decodeAct method

void MStdEncoder::initRangeInfoAccelerators() {
//	get references that are the same for all range blocks
	const RangeList &ranges= planeBlock->ranges->getRangeList();
	const ISquareDomains::PoolList &pools= planeBlock->domains->getPools();
	int zoom= planeBlock->settings->zoom;
//	iterate over the ranges (and their accelerators)
	for ( RangeList::const_iterator it=ranges.begin(); it!=ranges.end(); ++it ) {
	//	get range level, reference to the range's info and to the level's pool infos
		int level= (*it)->level;
		RangeInfo &info= *RangeInfo::get(*it);
		if ( info.domainID < 0 )
			continue;
		const PoolInfos &poolInfos= levelPoolInfos[level];
	//	build pool infos for the level (if neccesary), fill info's accelerators
		if ( poolInfos.empty() ) 
			buildPoolInfos4aLevel(level), ASSERT( !poolInfos.empty() );
		info.decAccel.pool= &getDomainData
			( **it, pools, poolInfos, info.domainID, zoom, info.decAccel.domBlock );
	//	adjust domain's block if the range isn't regular
		if ( !(*it)->isRegular() )
			info.decAccel.domBlock= adjustDomainForIncompleteRange
				( **it, info.rotation, info.decAccel.domBlock );
	}
}
