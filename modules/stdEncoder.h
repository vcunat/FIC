#ifndef STDENCODER_HEADER_
#define STDENCODER_HEADER_

#include "../interfaces.h"

class MStandardEncoder: public ISquareEncoder {
public:
	static const float MaxLinCoeff_none=0;

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
		data: {	f:{0,1} },
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
		1,		// predictor - using brute-force predictor as the default one for debugging 
		0,		// rotations and symmetries
		1,		// color value inversion
		0.35,	// big-scale penalty coefficient
		1,		// take quant-errors into account
		MaxLinCoeff_none, //	the maximum linear coefficient
		7,		// average possibilities
		8,		// deviation possibilities
		0,		// average codec
		0,		// deviation codec
	)
private:
	enum Settings { ModulePredictor, AllowedRotations, AllowedInversion, BigScaleCoeff
	, AllowedQuantError, MaxLinCoeff, QuantStepLog_avg, QuantStepLog_dev
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

public:
	typedef LevelPoolInfos::value_type PoolInfos;
	typedef ISquareRanges::RangeList RangeList;

	struct RangeInfo: public RangeNode::EncoderData, public IStdEncPredictor::Prediction {
		struct {
			Block domBlock;
			const ISquareDomains::Pool *pool;
		} decAccel;
		Real rAvg, rDev2; // quant-rounded values
		bool inverted;

		IStdEncPredictor::Prediction& prediction()
			{ return *this; }
	};

private:
//	Module's data
	const PlaneBlock *planeBlock;
	std::vector<float> stdRangeSEs;
	LevelPoolInfos levelPoolInfos;

	BulkAllocator<RangeInfo> rangeInfoAlloc;

protected:
//	Construction and destruction
	MStandardEncoder(): planeBlock(0) {}

public:
//	ISquareEncoder interface
	void initialize( IRoot::Mode mode, const PlaneBlock &planeBlock_ );
	float findBestSE(const RangeNode &range);
	void finishEncoding() {
		initRangeInfoAccelerators();
		modulePredictor()->cleanUp();
	}
	void decodeAct( DecodeAct action, int count=1 );

	void writeSettings(std::ostream &file);
	void readSettings(std::istream &file);

	int phaseCount() const
		{ return 3; }
	void writeData(std::ostream &file,int phase);
	void readData(std::istream &file,int phase);

private:
	void buildPoolInfos4aLevel(int level);
	void initRangeInfoAccelerators();
};

#endif // STDENCODER_HEADER_
