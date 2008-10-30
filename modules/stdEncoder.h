#ifndef STDENCODER_HEADER_
#define STDENCODER_HEADER_

#include "../interfaces.h"

/** Standard square encoder - uses affine color transformation
 *	for one-domain to one-range mappings */
class MStandardEncoder: public ISquareEncoder {
	DECLARE_debugModule;
public:
	static const Real MaxLinCoeff_none= 0;

	DECLARE_M_cloning_name_desc( MStandardEncoder, "Standard encoder"
	, "Classic encoder supporting one-domain to one-range mappings" )

	DECLARE_M_settings_type({
		type:	ModuleCombo,
		data: {	compatIDs: &IStdEncPredictor::getCompMods() },
		label:	"Best-match predictor",
		desc:	"Module that chooses which domain blocks\n"
				"should be tried for a range block"
	}, {
		type:	Combo,
		data: {	text:"identity only\nclassic 8" },
		label:	"Rotations and symmetries",
		desc:	"Can the projections include rotations and symmetries?"
	}, {
		type:	Combo,
		data: {	text:"not allowed\nallowed" },
		label:	"Color value inversion",
		desc:	"Can the projections have negative linear coefficients?"
	}, {
		type:	Float,
		data: {	f:{0,4} },
		label:	"Coefficient of big-scale penalization",
		desc:	"How much will big linear coefficients in color-value\n"
				"projections be penalized? (select zero to disable it)"
	}, {
		type:	Combo,
		data: {	text:"no\nyes" },
		label:	"Take quantization errors into account",
		desc:	"Selecting yes will result in slower but more precise encoding?"
	}, {
		type:	Float,
		data: {	f:{MaxLinCoeff_none,1.2} },
		label:	"Maximum linear coefficient",
		desc:	"The maximum absolute value of linear coefficients"
	}, {
		type:	Float,
		data: {	f:{0,1} },
		label:	"Sufficient quotient of SE",
		desc:	"After reaching this quotient of square error,\n"
				"no other domain blocks will be tried"	
	}, {
		type:	IntLog2,
		data: {	i:{2,10} },
		label:	"Quantization steps for average",
		desc:	"The number (a power of two) of possible range block\n"
				"average color values (real average will be rounded)"
	}, {
		type:	IntLog2,
		data: {	i:{2,10} },
		label:	"Quantization steps for deviation",
		desc:	"The number (a power of two) of possible range block\n"
				"standard color value deviations"
	}, {
		type:	ModuleCombo,
		data: {	compatIDs: &IIntCodec::getCompMods() },
		label:	"The codec for averages",
		desc:	"The module that will code and decode\n"
				"average color values of range blocks"
	}, {
		type:	ModuleCombo,
		data: {	compatIDs: &IIntCodec::getCompMods() },
		label:	"The codec for deviations",
		desc:	"The module that will code and decode standard\n"
				"deviations of color values of range blocks"
	})

	DECLARE_M_settings_default(
		0,//1,		// predictor - using brute-force predictor as the default one for debugging
		1,		// rotations and symmetries
		1,		// color value inversion
		1.0f,	// big-scale penalty coefficient
		1,		// take quant-errors into account
		(float)MaxLinCoeff_none, //	the maximum linear coefficient
		0.25f,	// sufficient SE quotient
		7,		// average possibilities
		7,		// deviation possibilities
		0,		// average codec
		0,		// deviation codec
	)
private:
	/** Indices for settings */
	enum Settings { ModulePredictor, AllowedRotations, AllowedInversion, BigScaleCoeff
	, AllowedQuantError, MaxLinCoeff, SufficientSEq, QuantStepLog_avg, QuantStepLog_dev
	, ModuleCodecAvg, ModuleCodecDev };
//	Settings-retieval methods
	float settingsFloat(Settings index)
		{ return settings[index].f; }
	IStdEncPredictor* modulePredictor()
		{ return debugCast<IStdEncPredictor*>(settings[ModulePredictor].m); }
	IIntCodec* moduleCodec(bool forAverage) {
		return debugCast<IIntCodec*>
		( settings[ forAverage ? ModuleCodecAvg : ModuleCodecDev ].m );
	}

	/** Information about just encoded range block */
	struct EncodingInfo;

public:
	typedef LevelPoolInfos	::value_type	PoolInfos;
	typedef ISquareRanges	::RangeList		RangeList;
	typedef ISquareDomains	::Pool			Pool;
	typedef IStdEncPredictor::Prediction	Prediction;

	/** Stores information about a range's mapping, to be pointed by ISquareRanges::RangeNode.encoderData */
	struct RangeInfo: public RangeNode::EncoderData, public Prediction {
		struct {
			Block domBlock;
			const Pool *pool;
		} decAccel;
		Real qrAvg, qrDev2; // quant-rounded values
		bool inverted;

		#ifndef NDEBUG
		struct {
			Real avg, dev2, constCoeff, linCoeff;
		} exact;
		#endif

		Prediction& prediction()
			{ return *this; }
	};

private:
//	Module's data
	const PlaneBlock *planeBlock;	///< Pointer to the block to encode/decode
	std::vector<float> stdRangeSEs;	///< Caches the result of IQuality2SquareError::completeSquareRangeErrors
	LevelPoolInfos levelPoolInfos;	///< [level][pool] -> LevelPoolInfo

	BulkAllocator<RangeInfo> rangeInfoAlloc; ///< Allocator for RangeNode::encoderData

protected:
//	Construction and destruction
	/** Only initializes #planeBlock to zero */
	MStandardEncoder(): planeBlock(0) {}

public:
/**	\name ISquareEncoder interface
 *	@{ */
	void initialize( IRoot::Mode mode, const PlaneBlock &planeBlock_ );
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
private:
	/** Builds #levelPoolInfos[\p level], uses #planeBlock->domains */
	void buildPoolInfos4aLevel(int level);
	/** Initializes decoding accelerators (in RangeInfo) for all range blocks */
	void initRangeInfoAccelerators();

	/** Considers a domain on a \p level number \p domIndex (in \p pools and \p poolInfos)
	 *	and sets \p block to the domain's block and returns a reference to its pool */
	static const Pool& getDomainData
	( const RangeNode &rangeBlock, const ISquareDomains::PoolList &pools
	, const PoolInfos &poolInfos, int domIndex, Block &block );

	/** Return iterator to the pool of domain with index \p domID */
	static PoolInfos::const_iterator getPoolFromDomID( int domID, const PoolInfos &poolInfos );
};

#endif // STDENCODER_HEADER_
