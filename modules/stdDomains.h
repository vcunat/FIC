#ifndef STDDOMAINS_HEADER_
#define STDDOMAINS_HEADER_

#include "../headers.h"

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



namespace MatrixWalkers {
	/** A simple assigning operator - assigns its second argument into the first one */
	struct ReverseAssigner: public OperatorBase {
		template<class R1,class R2> void operator()(R1 &dest,R2 src) const { dest= src; }
	};
	/** Matrix iterator performing a 2x2 to 1 shrink */
	template<class T,class I=PtrInt>
	struct HalfShrinker: public Rotation_0<T,I> { ROTBASE_INHERIT
		typedef Rotation_0<T,I> Base;
		
		HalfShrinker( TMatrix matrix, I x0=0, I y0=0 )
		: Base(matrix,x0,y0) {}

		T get() {
			TMatrix &c= current;
			T *cs= c.start;
			return ldexp( cs[0] + cs[1] + cs[c.colSkip] + cs[c.colSkip+1], -2 ); 
		}
		void outerStep() { Base::outerStep(); Base::outerStep(); }
		void innerStep() { Base::innerStep(); Base::innerStep(); }
	};
	/** Matrix iterator performing a 3x3 to 1x2 shrink */
	template<class T,class I=PtrInt>
	struct HorizShrinker: public Rotation_0<T,I> { ROTBASE_INHERIT
		typedef Rotation_0<T,I> Base;
		
		bool addHalf; ///< Adds half a pixel to the current position
		
		HorizShrinker(TMatrix matrix)
		: Base(matrix), addHalf(false) {}

		T get() {
			Real groups[2]; // groups[0]= sum of a full line, groups[1]= sum of a half line
			T *cs= current.start;
			groups[addHalf]= cs[0] + cs[current.colSkip] + cs[current.colSkip*2];
			++cs;
			groups[!addHalf]= cs[0] + cs[current.colSkip] + cs[current.colSkip*2];
			return (ldexp(groups[0],1)+groups[1]) * Real(1/9);
		}
		void innerStep() {
		//	do a 1.5-pixel step
			Base::innerStep();
			if (addHalf)
				Base::innerStep();
			addHalf= !addHalf;
		}
		void outerStep() {
			for (int i=0; i<3; ++i)
				Base::outerStep();
		}
	};
	/** Matrix iterator performing a 3x3 to 2x1 shrink */
	template<class T,class I=PtrInt>
	struct VertShrinker: public Rotation_0<T,I> { ROTBASE_INHERIT
		typedef Rotation_0<T,I> Base;
		
		bool addHalf; ///< Adds half a pixel to the current position
		
		VertShrinker(TMatrix matrix)
		: Base(matrix) {}

		T get() { 
			Real groups[2]; // groups[0]= sum of a full line, groups[1]= sum of a half line
			T *cs= current.start;
			groups[addHalf]= cs[0] + cs[1] + cs[2];
			cs+= current.colSkip;
			groups[!addHalf]= cs[0] + cs[1] + cs[2];
			return (ldexp(groups[0],1)+groups[1]) * Real(1/9);
		}
		void innerStep() { 
			for (int i=0; i<3; ++i)
				Base::innerStep(); 
		}
		void outerStep() {
		//	do a 1.5-pixel step
			Base::outerStep();
			if (addHalf)
				Base::outerStep();
			addHalf= !addHalf;
		}
	};
	/** Matrix iterator performing a shrink of a diamond (rotated sub-square) 
	 *	into a square, decreasing the area of used pixels to 50% */
	template<class T,class I=PtrInt> 
	struct DiamShrinker: public Rotation_0<T,I> { ROTBASE_INHERIT
		typedef Rotation_0<T,I> Base;
		
		DiamShrinker(TMatrix matrix,I side)
		: Base( matrix, side-1, 0 ) {} // the diamond begins on top-middle

		T get() {
			TMatrix &c= current;
			T *cs= c.start;
			return ldexp( cs[0] + cs[1] + cs[c.colSkip] + cs[c.colSkip+1], -2 ); 
		}
		void outerStep() { lastStart+= current.colSkip+1; }
		void innerStep() { current.start+= 1-current.colSkip; }
	};
}//	MatrixWalkers namespace


#endif // STDDOMAINS_HEADER_
