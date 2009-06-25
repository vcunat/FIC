#ifndef STDENCODER_HEADER_
#define STDENCODER_HEADER_

#include "../headers.h"

/// \ingroup modules
/** Standard square encoder - uses affine color transformation
 *	for one-domain to one-range mappings. Uses modified mappings with fixed target
 *	average and deviation and allows to set
 *	- a predictor module (IStdEncPredictor), so only some pairs have to be compared exactly
 *	- whether to try 8 isometric square transformations or identity only
 *	- whether to allow negative linear coefficients
 *	- the amount of big-scaling penalization (multiplier) 
 *	- whether to take quantization errors into account
 *	- how much to restrict the linear coefficients (its absolute values)
 *	- the part of max. error that suffices (interrupts searching for better)
 *	- the fineness of average and deviation quantization (separate, in powers of two)
 *	- codec modules for quantized averages and deviations (IIntCodec) 
 *	When encoding, given a range block the module succesively tries domains returned 
 *	by the predictor, computes exact error and keeps track of the best-fitting domain
 *	seen (yet). */
class MStdEncoder: public ISquareEncoder {
	DECLARE_debugModule;
public:
	static const float MaxLinCoeff_none= 0;

	DECLARE_TypeInfo( MStdEncoder, "Standard encoder"
	, "Classic encoder supporting one-domain to one-range mappings"
	, {
		label:	"Best-match predictor",
		desc:	"Module that chooses which domain blocks\n"
				"should be tried for a range block",
		type:	settingModule<IStdEncPredictor>()
	}, {
		label:	"Rotations and symmetries",
		desc:	"Can the projections include rotations and symmetries?",
		type:	settingCombo("identity only\nclassic 8",1)
	}, {
		label:	"Color value inversion",
		desc:	"Can the projections have negative linear coefficients?",
		type:	settingCombo("not allowed\nallowed",1)
	}, {
		label:	"Coefficient of big-scale penalization",
		desc:	"How much will big linear coefficients in color-value\n"
				"projections be penalized? (select zero to disable it)",
		type:	settingFloat(0,0.25,4)
	}, {
		label:	"Take quantization errors into account",
		desc:	"Selecting yes will result in slower but more precise encoding?",
		type:	settingCombo("no\nyes",1)
	}, {
		label:	"Maximum linear coefficient",
		desc:	"The maximum absolute value of linear coefficients",
		type:	settingFloat(MaxLinCoeff_none,MaxLinCoeff_none,5.0)
	}, {
		label:	"Sufficient quotient of SE",
		desc:	"After reaching this quotient of square error,\n"
				"no other domain blocks will be tried",
		type:	settingFloat( 0, 0.05, 1 )
	}, {
		label:	"Quantization steps for average",
		desc:	"The number (a power of two) of possible range block\n"
				"average color values (real average will be rounded)",
		type:	settingInt(2,7,10,IntLog2)
	}, {
		label:	"Quantization steps for deviation",
		desc:	"The number (a power of two) of possible range block\n"
				"standard color value deviations",
		type:	settingInt(2,7,10,IntLog2)
	}, {
		label:	"The codec for averages",
		desc:	"The module that will code and decode\n"
				"average color values of range blocks",
		type:	settingModule<IIntCodec>()
	}, {
		label:	"The codec for deviations",
		desc:	"The module that will code and decode standard\n"
				"deviations of color values of range blocks",
		type:	settingModule<IIntCodec>()
	} )

protected:
	/** Indices for settings */
	enum Settings { ModulePredictor, AllowedRotations, AllowedInversion, BigScaleCoeff
	, AllowedQuantError, MaxLinCoeff, SufficientSEq, QuantStepLog_avg, QuantStepLog_dev
	, ModuleCodecAvg, ModuleCodecDev };
//	Settings-retieval methods
	float settingsFloat(Settings index)
		{ return settings[index].val.f; }
	IStdEncPredictor* modulePredictor()
		{ return debugCast<IStdEncPredictor*>(settings[ModulePredictor].m); }
	IIntCodec* moduleCodec(bool forAverage) {
		return debugCast<IIntCodec*>
		( settings[ forAverage ? ModuleCodecAvg : ModuleCodecDev ].m );
	}

	/** Information about just encoded range block, defined in stdEncoder.cpp */
	struct EncodingInfo;

public:
	typedef LevelPoolInfos	::value_type	PoolInfos;
	typedef ISquareRanges	::RangeList		RangeList;
	typedef ISquareDomains	::Pool			Pool;
	typedef IStdEncPredictor::Prediction	Prediction;

public:
	/** Stores information about a range's mapping, to be pointed by ISquareRanges::RangeNode.encoderData */
	struct RangeInfo: public RangeNode::EncoderData, public Prediction {
		struct {
			Block domBlock;		///< domain block's position and size in its pool
			const Pool *pool;	///< the pool of the domain
		} decAccel; 	///< precomputed information about the domain (to accelerate)
		
		Real qrAvg		///  quant-rounded target average of the block
		, qrDev2;		///< quant-rounded target variance of the block
		bool inverted;	///< indicates whether the linear coefficient is negative

		#ifndef NDEBUG
		struct {
			Real avg, dev2, constCoeff, linCoeff;
		} exact;
		#endif
		
		/** Extracts RangeNode::encoderData and down-casts it to RangeInfo */
		static RangeInfo* get(RangeNode *range) {
			RangeInfo *result= static_cast<RangeInfo*>(range->encoderData);
			ASSERT(result);
			return result;
		}
		static const RangeInfo* get(const RangeNode *range)
			{ return get(constCast(range)); }
	};

protected:
//	Module's data
	PlaneBlock *planeBlock;			///< Pointer to the block to encode/decode
	std::vector<float> stdRangeSEs;	///< Caches the result of IQuality2SE::regularRangeErrors
	LevelPoolInfos levelPoolInfos;	///< see LevelPoolInfos, only initialized for used levels

protected:
//	Construction and destruction
	/** Only initializes ::planeBlock to zero */
	MStdEncoder(): planeBlock(0) {}

public:
/**	\name ISquareEncoder interface
 *	@{ */
	void initialize( IRoot::Mode mode, PlaneBlock &planeBlock_ );
	float findBestSE(const RangeNode &range,bool allowHigherSE);
	void finishEncoding() {
		initRangeInfoAccelerators();	// prepare for saving/decoding
		modulePredictor()->cleanUp();	// free unneccesary memory of the predictor
	}
	void decodeAct( DecodeAct action, int count=1 );

	void writeSettings(std::ostream &file);
	void readSettings(std::istream &file);

	int phaseCount() const
		{ return 3; }
	void writeData(std::ostream &file,int phase);
	void readData(std::istream &file,int phase);
///	@}
protected:
	/** Builds ::levelPoolInfos[\p level], uses ::planeBlock->domains */
	void buildPoolInfos4aLevel(int level);
	/** Initializes decoding accelerators (in RangeInfo) for all range blocks */
	void initRangeInfoAccelerators();

	/** Considers a domain on a \p level number \p domIndex (in \p pools and \p poolInfos)
	 *	and sets \p block to the domain's block and returns a reference to its pool */
	static const Pool& getDomainData
	( const RangeNode &rangeBlock, const ISquareDomains::PoolList &pools
	, const PoolInfos &poolInfos, int domIndex, int zoom, Block &block );

	/** Return iterator to the pool of domain with index \p domID */
	static PoolInfos::const_iterator getPoolFromDomID( int domID, const PoolInfos &poolInfos );
};

#endif // STDENCODER_HEADER_
