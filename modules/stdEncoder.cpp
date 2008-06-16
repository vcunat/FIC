#include "stdEncoder.h"
#include "../fileUtil.h"

using namespace std;

namespace Quantizer {
	class Average {
		int scale,possib;
	public:
		Average(int possibLog2) {
			assert(possibLog2>0);
		//	the average is from [0,1]
			scale=possibLog2;
			possib=powers[possibLog2];
		}
		int quant(float avg)
			{ return quantizeByPower(avg,scale,possib); }
		float dequant(int i)
			{ return dequantizeByPower(i,scale,possib); }
		float qRound(float avg)
			{ return dequant(quant(avg)); }
	};

	class Deviation {
		int scale,possib;
	public:
		Deviation(int possibLog2) {
			assert(possibLog2>0);
		//	the deviation is from [0,0.5] -> we have to scale twice more (log2+=1)
			scale=possibLog2+1;
			possib=powers[possibLog2];
		}
		int quant(float dev)
			{ return quantizeByPower(dev,scale,possib); }
		float dequant(int i)
			{ return dequantizeByPower(i,scale,possib); }
		float qRound(float dev)
			{ return dequant(quant(dev)); }
	};
}

namespace {
	struct LevelPoolInfo_indexComparator {
		typedef ISquareEncoder::LevelPoolInfo LevelPoolInfo;
		bool operator()( const LevelPoolInfo &a, const LevelPoolInfo &b )
			{ return a.indexBegin < b.indexBegin; }
	};
}
static const ISquareDomains::Pool&
getDomainData( int level, const ISquareDomains::Pools &pools
, const MStandardEncoder::PoolInfos &poolInfos, int domIndex, Block &block ) {
//	do some checks
	assert( domIndex>=0 && domIndex<poolInfos.back().indexBegin );
//	get the right pool
	MStandardEncoder::PoolInfos::value_type toFind;
	toFind.indexBegin=domIndex;
	MStandardEncoder::PoolInfos::const_iterator it=lower_bound
	( poolInfos.begin(), poolInfos.end(), toFind, LevelPoolInfo_indexComparator() );
	const ISquareDomains::Pool &pool= pools[ it - poolInfos.begin() ];
//	get the coordinates
	int indexInPool= domIndex - it->indexBegin;
	int size= powers[level];
	int domsInRow= getCountForDensity(pool.width,it->density,size);
	block.x0= (indexInPool%domsInRow)*it->density;
	block.y0= (indexInPool/domsInRow)*it->density;
	block.xend= block.x0+size;
	block.yend= block.y0+size;

	return pool;
}

inline static Block adjustDomainForIncompleteRange
( int size, int rotation, Block block ) {
	int rotID= rotation/2;

	int yShift= size - block.height();
	if ( rotID>1 ) // domain rotations 2, 3, 2T, 3T -> aligned to the bottom
		block.y0+= yShift;
	else
		block.yend-= yShift;

	int xShift= size - block.width();
	if ( rotID==1 || rotID==2 ) // rotations 1, 2, 1T, 2T -> aligned to the right
		block.x0+= xShift;
	else
		block.xend-= xShift;
	return block;
}

////	Member methods

void MStandardEncoder::initialize( IRoot::Mode mode, const PlaneBlock &planeBlock_ ) {
	planeBlock=&planeBlock_;
//	prepare the domains-module
	planeBlock->domains->initPools( planeBlock->width(), planeBlock->height() );

	int maxLevel=log2ceil(max( planeBlock->width(), planeBlock->height() ));
//	prepare levelPoolInfos
	levelPoolInfos.resize(maxLevel);

	if (mode==IRoot::Encode) {
		typedef ISquareDomains::Pools Pools;
	//	fill domains' pixels and initialize their summers
		planeBlock->domains->fillPixelsInPools(*planeBlock);
		const Pools &pools=planeBlock->domains->getPools();
		for_each( pools.begin(), pools.end()
		, mem_fun_ref(&Pools::value_type::summers_makeValid) );
	//	initialize the range summers
		planeBlock->summers_makeValid();

	//	prepare maximum SquareErrors allowed for regular range blocks
		stdRangeSEs.resize(maxLevel+1);
		planeBlock->moduleQ2SE->completeSquareRangeErrors
		( planeBlock->quality, maxLevel+1, &stdRangeSEs.front() );
	}
}

namespace {
	struct StableInfo: public IStdEncPredictor::NewPredictorData {
	};

	struct BestInfo: public IStdEncPredictor::Prediction {
		float error;
		bool inverted;

		BestInfo()
		: error( numeric_limits<float>::max() ) {}
		IStdEncPredictor::Prediction& prediction()
			{ return *this; }
	};

	struct EncodingInfo {
		typedef IStdEncPredictor::Prediction Prediction;
		typedef MStandardEncoder::RangeInfo RangeInfo;
		typedef void (EncodingInfo::*ExactCompareProc)( Prediction prediction );

	private:
		static ExactCompareProc exactCompareArray[];

		ExactCompareProc selectedProc;
	public:
		StableInfo stable;
		BestInfo best;
		Real dSum, d2Sum;
		float maxSE2Predict;

	public:
		EncodingInfo()
		: selectedProc(0) {}

		RangeInfo* initRangeInfo(RangeInfo *ri) const {
			ri->bestSE= best.error;
			ri->prediction()= best;
			ri->rAvg= stable.rAvg;
			ri->rDev2= stable.rDev2;
			ri->inverted= best.inverted;
			return ri;
		}

		void selectExactCompareProc() {
			selectedProc= exactCompareArray
			[(((  stable.quantError *2
				+ stable.allowInversion ) *2
				+ stable.isRegular ) *2
				+ (stable.maxLinCoeff2>=0) ) *2
				+ (stable.bigScaleCoeff!=0)
			];
		}

		void exactCompare(Prediction prediction)
			{ (this->* selectedProc)(prediction); }

	private:
		template< bool quantErrors, bool allowInversion, bool isRegular
		, bool restrictMaxLinCoeff, bool bigScalePenalty >
		void exactCompareProc( Prediction prediction );
	};

	#define ALTERNATE_0(name,params...) name<params>
	#define ALTERNATE_1(name,params...) ALTERNATE_0(name,false,##params), ALTERNATE_0(name,true,##params)
	#define ALTERNATE_2(name,params...) ALTERNATE_1(name,false,##params), ALTERNATE_1(name,true,##params)
	#define ALTERNATE_3(name,params...) ALTERNATE_2(name,false,##params), ALTERNATE_2(name,true,##params)
	#define ALTERNATE_4(name,params...) ALTERNATE_3(name,false,##params), ALTERNATE_3(name,true,##params)
	#define ALTERNATE_5(name,params...) ALTERNATE_4(name,false,##params), ALTERNATE_4(name,true,##params)
	EncodingInfo::ExactCompareProc EncodingInfo::exactCompareArray[]
	= {ALTERNATE_5(&EncodingInfo::exactCompareProc)};
	#undef ALTERNATE_0
	#undef ALTERNATE_1
	#undef ALTERNATE_2
	#undef ALTERNATE_3
	#undef ALTERNATE_4
	#undef ALTERNATE_5

	template< bool quantErrors, bool allowInversion, bool isRegular
	, bool restrictMaxLinCoeff, bool bigScalePenalty >
	void EncodingInfo::exactCompareProc( EncodingInfo::Prediction prediction ) {
		using namespace MatrixWalkers;
	//	find out which domain was predicted (pixel matrix and position within it)
		::Block domBlock;
		const ISquareDomains::Pool &pool= getDomainData( stable.rangeBlock->level
		, *stable.pools, *stable.poolInfos, prediction.domainID, domBlock );
	//	compute the sum of products of pixels
		Real rdSum= walkOperateCheckRotate
		( (const float**)stable.rangePixels, *stable.rangeBlock, RDSummer<Real>()
		, (const float**)pool.pixels, domBlock, prediction.rotation ) .totalSum;
	//	compute domain sums (rdSum computed, I can change domBlock)
		if (!isRegular)
			domBlock= adjustDomainForIncompleteRange
			( powers[stable.rangeBlock->level], prediction.rotation, domBlock );
		pool.getSums(domBlock,dSum,d2Sum);

		Real nRDs_RsDs= stable.pixCount*rdSum - stable.rSum*dSum;
	//	check for negative linear coeffitients (if needed)
		if (!allowInversion)
			if (nRDs_RsDs<0)
				return;
		Real denom= 1/( stable.pixCount*d2Sum - sqr(dSum) );
	//	check for too big linear coeffitients (if needed)
		if (restrictMaxLinCoeff)
			if ( stable.rDev2*denom*sqr(stable.pixCount) > stable.maxLinCoeff2 )
				return;
	//	compute the SquareError
		float optSE;
		if (quantErrors) {
			Real rDev2Denom=stable.rDev2*denom;
			optSE= stable.r2Sum + stable.pixCount*stable.rDev2
			+ stable.rAvg*(stable.pixCount*stable.rAvg-ldexp(stable.rSum,1))
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
	//	test if the error is acceptable and the best so far
		if ( optSE < maxSE2Predict ) {
			best.prediction()= prediction;
			best.error= maxSE2Predict= optSE;
			best.inverted= nRDs_RsDs<0;
		}
	}
}
float MStandardEncoder::findBestSE(const RangeNode &range) {
	assert( planeBlock && !stdRangeSEs.empty() && range.level<(int)levelPoolInfos.size() );
	assert( !range.encoderData );

//	initialize an encoding-info object
	EncodingInfo info;

	info.stable.rangeBlock=		&range;
	info.stable.rangePixels=	(const float**)planeBlock->pixels;
	info.stable.pools=			&planeBlock->domains->getPools();
	info.stable.poolInfos=		&levelPoolInfos[range.level];

	info.stable.allowRotations=	settingsInt(AllowedRotations);
	info.stable.quantError=		settingsInt(AllowedQuantError);
	info.stable.allowInversion=	settingsInt(AllowedInversion);
	info.stable.isRegular=		range.isRegular();
	{
		float coeff= settingsFloat(MaxLinCoeff);
		info.stable.maxLinCoeff2=	coeff==MaxLinCoeff_none ? -1 : sqr(coeff);
	}
	info.stable.bigScaleCoeff=	settingsFloat(BigScaleCoeff);

	info.stable.rSum=		planeBlock->getSum(range,BlockSummer::Values);
	info.stable.r2Sum=		planeBlock->getSum(range,BlockSummer::Squares);
	info.stable.pixCount=	range.size();
	{
		Quantizer::Average quantAvg( settingsInt(QuantStepLog_avg) );
		Quantizer::Deviation quantDev( settingsInt(QuantStepLog_dev) );
		info.stable.rAvg= quantAvg.qRound( info.stable.rSum / info.stable.pixCount );

		int qrDev= quantDev.quant(
			sqrt( info.stable.pixCount*info.stable.r2Sum - sqr(info.stable.rSum) )
			/ info.stable.pixCount
		);
		if (qrDev)
			info.stable.rDev2= sqr(quantDev.dequant(qrDev));
		else {
		//	handle flat ranges - no domain block, only average
			info.stable.rDev2= 0;
			info.best.error= info.stable.quantError
				? info.stable.r2Sum + info.stable.rAvg
					*( info.stable.pixCount*info.stable.rAvg - ldexp(info.stable.rSum,1) )
				: info.stable.r2Sum - sqr(info.stable.rSum)/info.stable.pixCount;
			range.encoderData= info.initRangeInfo( rangeInfoAlloc.make() );
			return range.encoderData->bestSE;
		}
	}

	info.maxSE2Predict= info.stable.isRegular
	? stdRangeSEs[range.level]
	: planeBlock->moduleQ2SE->rangeSE( planeBlock->quality, range.size() );

//	check the level has been initialized
	if ( info.stable.poolInfos->empty() )
		buildPoolInfos4aLevel(range.level);
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
	vector<short> densities= planeBlock->domains->getLevelDensities
	( level, domainCountLog2 );
//	store it in the this-level infos with beginIndex values for all pools
	vector<LevelPoolInfo> &poolInfos= levelPoolInfos[level];
	const ISquareDomains::Pools &pools= planeBlock->domains->getPools();

	int domainSize= powers[level];
	int poolCount= pools.size();
	poolInfos.resize(poolCount+1);

	int domCount= poolInfos[0].indexBegin= 0; // accumulated count
	for (int i=0; i<poolCount; ++i) {
		int dens= poolInfos[i].density= densities[i];
		if (dens) // if dens==0, there are no domains -> no increase
			domCount+= getCountForDensity2D
			( pools[i].width, pools[i].height, dens, domainSize );
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
				averages.push_back( quant.quant(info->rAvg) );
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
				devs.push_back( quant.quant(sqrt(info->rDev2)) );
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
			for (size_t i=0; i<levelPoolInfos.size(); ++i)
				domBits[i]= levelPoolInfos[i].empty() ? 0
				: log2ceil( levelPoolInfos[i].back().indexBegin );
		//	put the domain bits
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				assert( *it && (*it)->encoderData );
				const RangeInfo *info= static_cast<RangeInfo*>( (*it)->encoderData );
				int domID= info->domainID;
				if ( domID >=0 ) {
					int bits= domBits[ (*it)->level ];
					assert( 0<=domID && domID<powers[bits] );
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
				info->rAvg= quant.dequant(averages[it-ranges.begin()]);
				info->rDev2= 0;
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
					info->rDev2= sqr(quant.dequant(quantDev));
				else {
					info->rDev2= 0;
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
					if (info->rDev2)
						info->inverted= stream.getBits(1);
				}
		//	get the rotation bits if rotations are allowed
			if ( settingsInt(AllowedRotations) )
				for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
					assert( *it && (*it)->encoderData );
					RangeInfo *info= static_cast<RangeInfo*>( (*it)->encoderData );
					if (info->rDev2)
						info->rotation=stream.getBits(3);
				}
		//	find out bits needed to store IDs of domains for every level
			vector<int> domBits( levelPoolInfos.size(), -1 );
		//	get the domain bits
			for (RLcIterator it=ranges.begin(); it!=ranges.end(); ++it) {
				assert( *it && (*it)->encoderData );
				RangeInfo *info= static_cast<RangeInfo*>( (*it)->encoderData );
				if (!info->rDev2)
					continue;
				int level=(*it)->level;
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
				if ( domID >= levelPoolInfos[level].back().indexBegin )
					throw std::exception();
				info->domainID=domID;
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
		planeBlock->domains->invalidatePoolSummers();
		break;
	case Iterate:
		assert(count>0);
		do {
		//	validate the domain pool summers, iterate each range block
			planeBlock->domains->validatePoolSummers();
			for(RangeList::const_iterator it=ranges.begin(); it!=ranges.end(); ++it) {
				const RangeInfo &info= (const RangeInfo&)* (*it)->encoderData;
				if ( info.domainID < 0 ) {
					fillSubMatrix<float>( planeBlock->pixels, **it, info.rAvg );
					continue;
				}
				Real dSum, d2Sum;
				info.decAccel.pool->getSums( info.decAccel.domBlock, dSum, d2Sum );
				Real pixCount= (*it)->size();

				Real linCoeff= (info.inverted ? -pixCount : pixCount) * sqrt
				( info.rDev2 / ( pixCount*d2Sum - sqr(dSum) ) );
				if ( !isnormal(linCoeff) ) {
					fillSubMatrix<float>( planeBlock->pixels, **it, info.rAvg );
					continue;
				}
				Real constCoeff= info.rAvg - linCoeff*dSum/pixCount;

				using namespace MatrixWalkers;
				AddMulCopyChecked<Real> oper( constCoeff, linCoeff, 0, 1 );
				walkOperateCheckRotate( planeBlock->pixels, *planeBlock, oper
				, info.decAccel.pool->pixels, info.decAccel.domBlock, info.rotation );
			}
		} while (--count);
		break;
	}
}

void MStandardEncoder::initRangeInfoAccelerators() {
//	get references that are the same for all range blocks
	const RangeList &ranges= planeBlock->ranges->getRangeList();
	const ISquareDomains::Pools &pools= planeBlock->domains->getPools();
//	iterate over the ranges (and their accelerators)
	for ( RangeList::const_iterator it=ranges.begin(); it!=ranges.end(); ++it ) {
	//	get range level, reference to the range's info and to the level's pool infos
		int level= (*it)->level;
		RangeInfo &info= *(RangeInfo*)(*it)->encoderData;
		if ( info.domainID < 0 )
			continue;
		const PoolInfos &poolInfos= levelPoolInfos[level];
	//	build pool infos for the level (if neccesary), fill info's accelerators
		if ( poolInfos.empty() )
			buildPoolInfos4aLevel(level),	assert( !poolInfos.empty() );
		info.decAccel.pool= &getDomainData
		( level, pools, poolInfos, info.domainID, info.decAccel.domBlock );
	}
}
