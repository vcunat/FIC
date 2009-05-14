#ifndef STDDOMAINS_HEADER_
#define STDDOMAINS_HEADER_

#include "../interfaces.h"

/// \ingroup modules
/** Standard domain-pool generator. Available settings:
 *	- the decrease of max. number of domains per level
 *	- how many of more-downscaled domains to use
 *	- how big portions of standard, horizontal, vertical and diamond domains to use
 */
class MStdDomains: public ISquareDomains {
	DECLARE_debugModule;

	DECLARE_TypeInfo( MStdDomains, "Standard generator"
	, "Creates domains of many types (standard, horizontal, vertical, diamond),\n"
		"offers many adjustment possibilities"
	, {
		label:	"Per-level max-domain-count divisor",
		desc:	"How many times will the maximum domain count\n"
				"decrease with every level\n"
				"(dimensions are equal to 2^level)",
		type:	settingInt(0,0,4,IntLog2)
	}, {
		label:	"More-downscaled domain count",
		desc:	"The development of count of domains\n"
				"made from more downscaled images",
		type:	settingCombo("none\nno decrease\nhalf per scale",0)
	}, {
		label:	"Standard domains portion",
		desc:	"How big portion from all domains\n"
				"will be of standard type",
		type:	settingInt(0,1,8)
	}, {
		label:	"Diamond domains portion",
		desc:	"How big portion from all domains\n"
				"will be of diamond type",
		type:	settingInt(0,0,8)
	}, {
		label:	"Horizontal domains portion",
		desc:	"How big portion from all domains\n"
				"will be of horizontal type",
		type:	settingInt(0,0,8)
	}, {
		label:	"Vertical domains portion",
		desc:	"How big portion from all domains\n"
				"will be of vertical type",
		type:	settingInt(0,0,8)
	} )

protected:
	/** Indices for settings */
	enum Settings { MaxDomCountLevelDivisor, MultiDownScaling
	, DomPortion_Standard, DomPortion_Diamond, DomPortion_Horiz, DomPortion_Vert  };

//	Module's data
	/// The list of domain pools, pool IDs are the indices, the Pool::pixels are owned
	PoolList pools;
	int width	///  Width of the original image (not zoomed)
	, height	///  Height of the original image (not zoomed)
	, zoom;		///< the zoom

//	Construction and destruction
	#ifndef NDEBUG
		MStdDomains(): width(-1), height(-1), zoom(-1) {}
	#endif
	/** Only frees the #pools */
	~MStdDomains() { 
		for (PoolList::iterator it=pools.begin(); it!=pools.end(); ++it)
			it->free(); 
	}

public:
/**	\name ISquareDomains interface
 *	@{ */
	void initPools(const PlaneBlock &planeBlock);
	void fillPixelsInPools(PlaneBlock &planeBlock);

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
