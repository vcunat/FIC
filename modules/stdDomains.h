#ifndef STDDOMAINS_HEADER_
#define STDDOMAINS_HEADER_

#include "interfaces.h"

class MStandardDomains: public ISquareDomains {

	DECLARE_M_cloning_name_desc( MStandardDomains, "Standard generator"
	, "Creates domains of many types (standard, horizontal, vertical, diamond),\n"
		"offers many adjustment possibilities" )

	DECLARE_M_settings_type({
		type:	IntLog2,
		data: {	i: {1,4} },
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
		1,	// per-level divisor
		0,	// more-downscaled-domain count decrease
		4,	// standard domains portion
		1,	// diamond domains portion
		1,	// horizontal domains portion
		1	// vertical domains portion
	);
private:
	enum Settings { MaxDomCountLevelDivisor, MultiDownScaling
	, DomPortion_Standard, DomPortion_Diamond, DomPortion_Horiz, DomPortion_Vert  };
private:
//	Module's data
	/** The list of domain pools, pool IDs are the indices, the pixels are owned */
	Pools pools;
	int width, height;
protected:
//	Construction and destruction
	~MStandardDomains()
		{ for_each( pools.begin(), pools.end(), mem_fun_ref(&Pool::free) ); }

public:
//	ISquareDomains interface
	void initPools(int width_,int height_);
	void fillPixelsInPools(const PlaneBlock &ranges);

	const Pools& getPools() const
		{ return pools; }
	std::vector<short> getLevelDensities(int level,int stdDomCountLog2);

	void writeSettings(std::ostream &file);
	void readSettings(std::istream &file);
	/** We have no data that need to be preserved */
	void writeData(std::ostream &) {}
	void readData(std::istream &) {}
};

#endif // STDDOMAINS_HEADER_
