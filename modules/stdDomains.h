#ifndef STDDOMAINS_HEADER_
#define STDDOMAINS_HEADER_

#include "../interfaces.h"

/** Standard domain-pool generator */
class MStdDomains: public ISquareDomains {
	DECLARE_debugModule;

	DECLARE_M_cloning_name_desc( MStdDomains, "Standard generator"
	, "Creates domains of many types (standard, horizontal, vertical, diamond),\n"
		"offers many adjustment possibilities" )

	DECLARE_M_settings_type({
		type:	IntLog2,
		data: {	i: {0,4} },
		label:	"Per-level max-domain-count divisor",
		desc:	"How many times will the maximum domain count\n"
				"decrease with every level\n"
				"(dimensions are equal to 2^level)"
	}, {
		type:	Combo,
		data: {	text:"none\nno decrease\nhalf per scale" },
		label:	"More-downscaled domain count",
		desc:	"The development of count of domains\n"
				"made from more downscaled images"
	}, {
		type:	Int,
		data: {	i: {0,8} },
		label:	"Standard domains portion",
		desc:	"How big portion from all domains\n"
				"will be of standard type"
	}, {
		type:	Int,
		data: {	i: {0,8} },
		label:	"Diamond domains portion",
		desc:	"How big portion from all domains\n"
				"will be of diamond type"
	}, {
		type:	Int,
		data: {	i: {0,8} },
		label:	"Horizontal domains portion",
		desc:	"How big portion from all domains\n"
				"will be of horizontal type"
	}, {
		type:	Int,
		data: {	i: {0,8} },
		label:	"Vertical domains portion",
		desc:	"How big portion from all domains\n"
				"will be of vertical type"
	});

	DECLARE_M_settings_default(
		0,	// per-level divisor
		0,	// more-downscaled-domain count decrease
		1,	// standard domains portion
		0,	// diamond domains portion
		0,	// horizontal domains portion
		0	// vertical domains portion
	);
private:
	/** Indices for settings */
	enum Settings { MaxDomCountLevelDivisor, MultiDownScaling
	, DomPortion_Standard, DomPortion_Diamond, DomPortion_Horiz, DomPortion_Vert  };
private:
//	Module's data
	/** The list of domain pools, pool IDs are the indices, the Pool::pixels are owned */
	PoolList pools;
	int width	/// Width of the original image
	, height;	///< Height of the original image
protected:
//	Construction and destruction
	/** Only frees the #pools */
	~MStdDomains()
		{ for_each( pools.begin(), pools.end(), mem_fun_ref(&Pool::free) ); }

public:
/**	\name ISquareDomains interface
 *	@{ */
	void initPools(int width_,int height_);
	void fillPixelsInPools(const PlaneBlock &ranges);

	const PoolList& getPools() const
		{ return pools; }
	std::vector<short> getLevelDensities(int level,int stdDomCountLog2);

	void writeSettings(std::ostream &file);
	void readSettings(std::istream &file);
	/* We have no data that need to be preserved */
	void writeData(std::ostream &) {}
	void readData(std::istream &) {}
///	@}
};

#endif // STDDOMAINS_HEADER_
