#include "stdDomains.h"
#include "../fileUtil.h"

enum { MinDomSize=8, MinRngSize=4 };

using namespace std;


/*
#include <QImage>
void debugPool(const ISquareDomains::Pool &pool) {
	//	create and fill the image
	QImage image( pool.width, pool.height, QImage::Format_RGB32 );

	for (int y=0; y<pool.height; ++y) {
		QRgb *line= (QRgb*)image.scanLine(y);
		for (int x=0; x<pool.width; ++x) {
			int c= checkBoundsFunc( 0, (int)(pool.pixels[x][y]*256), 255 );
			line[x]= qRgb(c,c,c);
		}
	}

	image.save("pool.png");

	return;
}
*/



namespace NOSPACE {
	/** Pool ordering according to Pool::type (primary key) and Pool::level (secondary) */
	struct PoolTypeLevelComparator {
		typedef MStdDomains::Pool Pool;
		bool operator()(const Pool &a,const Pool &b) {
			if (a.type!=b.type)
				return a.type<b.type;
			else
				return a.level<b.level;
		}
	};
	/* All unzoomed sizes */
	inline static int minSizeNeededForDiamond() 
		{ return 2*MinDomSize-1; }
	inline static int getDiamondSize(int fromSize)
		{ return fromSize<minSizeNeededForDiamond() ? 0 : fromSize/2; }
	inline static int getDiamondShift(int fromSize) 
		{ return ( getDiamondSize(fromSize) - MinRngSize + 1 )*2; }
}
void MStdDomains::initPools(const PlaneBlock &planeBlock) {
	zoom=	planeBlock.settings->zoom;
	width=	rShift( planeBlock.width, zoom );
	height=	rShift( planeBlock.height, zoom );
//	checks some things
	ASSERT( width>0 && height>0 && this && settings && pools.empty() );
	if ( min(width,height)/2 < MinDomSize )
	//	no domains (too small)
		return;
//	create the first set of domain pools for standard, horizontal and vertical domains
	if ( settingsInt(DomPortion_Standard) )
		pools.push_back(Pool( width/2, height/2, DomPortion_Standard, 1, 0.25, zoom ));
	if ( settingsInt(DomPortion_Horiz) && width/3<MinDomSize )
		pools.push_back(Pool( width/3, height*2/3, DomPortion_Horiz, 1, 2.0/9.0, zoom ));
	if ( settingsInt(DomPortion_Vert) && height/3<MinDomSize )
		pools.push_back(Pool( width*2/3, height/3, DomPortion_Vert, 1, 2.0/9.0, zoom ));
//	create the first set of domain pools for diamond domains
	if ( settingsInt(DomPortion_Diamond) ) {
	//	get longer and shorter dimension
		int shorter= min(width,height);
		int shift= getDiamondShift(shorter);
	//	generate the pool sizes
		for (int longer=max(width,height); longer>minSizeNeededForDiamond(); longer-=shift) {
			int side= getDiamondSize( min(longer,shorter) );
			ASSERT(side>=0);
			if (side)
				pools.push_back(Pool( side, side, DomPortion_Diamond, 1, 0.5f, zoom ));
			else
				break;
		}
	}
//	if allowed, create more downscaled domains (at most 256 pools)
	if ( settingsInt(MultiDownScaling) )
		for (Uint i=0; i<pools.size() && i<256; ++i) {
			const Pool &pool=pools[i];
		//	compute new dimensions and add the pool if it's big enough	
			int w= rShift<int>(pool.width,zoom+1);
			int h= rShift<int>(pool.height,zoom+1);
			float cf= ldexp(pool.contrFactor,-2);
			if ( min(w,h) >= MinDomSize )
				pools.push_back( Pool( w, h, pool.type, pool.level+1, cf, zoom ) );
		}
//	sort the pools according to their types and levels (stable so the diamonds can't be swapped)
	stable_sort( pools.begin(), pools.end(), PoolTypeLevelComparator() );
}

namespace NOSPACE {
	typedef MStdDomains::PoolList::const_iterator PoolIt;
	/** To be called before creating shrinked domains to check the shrink is OK (debug only) */
	static inline bool halfShrinkOK(PoolIt src,PoolIt dest,int zoom) {
		return src->level+1 == dest->level && src->type == dest->type
			&& rShift<int>(src->width,zoom+1) == rShift<int>(dest->width,zoom)
			&& rShift<int>(src->height,zoom+1) == rShift<int>(dest->height,zoom);
	}
//	forwards, implemeted and described at the end of the file
	static void shrinkToHalf		( CSMatrix src, SMatrix dest, int width, int height );
	static void shrinkHorizontally	( CSMatrix src, SMatrix dest, int width, int height );
	static void shrinkVertically	( CSMatrix src, SMatrix dest, int width, int height );
	static void shrinkToDiamond		( CSMatrix src, SMatrix dest, int side );
}
void MStdDomains::fillPixelsInPools(PlaneBlock &planeBlock) {
	ASSERT( !pools.empty() ); // assuming initPools has already been called
//	iterate over pool types
	PoolList::iterator end= pools.begin();
	while ( end != pools.end() ) {
		PoolList::iterator begin= end;
		char type= begin->type;
	//	find the end of the same-pool-type block and invalidate the summers on the way
		while ( end!=pools.end() && end->type==type ) {
			end->summers_invalidate();
			++end;
		}

	//	we've got the interval, find out about the type
		if (type!=DomPortion_Diamond) {
		//	non-diamond domains all behave similarly
			void (*shrinkProc)( CSMatrix , SMatrix , int, int );
			switch (type) {
				case DomPortion_Standard:	shrinkProc= &shrinkToHalf;		break;
				case DomPortion_Horiz:		shrinkProc= &shrinkHorizontally;break;
				case DomPortion_Vert:		shrinkProc= &shrinkVertically;	break;
				default: ASSERT(false), shrinkProc=0;
			}
		//	we have the right procedure -> fill the first pool
			ASSERT( begin->level == 1 );
			shrinkProc( planeBlock.pixels, begin->pixels
			, begin->width, begin->height );
		//	fill the rest (in the same-type interval)
			while (++begin != end) {
				ASSERT( halfShrinkOK(begin-1,begin,zoom) );
				shrinkToHalf( (begin-1)->pixels, begin->pixels, begin->width, begin->height );
			}

		} else { //	handle diamond-type domains
			PoolList::iterator it= begin; //< the currently filled domain pool
		//	fill the first set of diamond-type domain pools
			bool horiz= width>=height;
			int shift= lShift( getDiamondShift(min(width,height)), zoom );
			int longerEnd= lShift( max(width,height)-minSizeNeededForDiamond(), zoom );
			CSMatrix source= planeBlock.pixels; 
			
			for (int l=0; l<=longerEnd; ++it,l+=shift) {
				ASSERT( it!=end && it->level==1 && it->width==it->height );
				shrinkToDiamond( planeBlock.pixels, it->pixels, it->width );
				source.shiftMatrix( (horiz?shift:0), (horiz?0:shift) );
			}
		//	now fill the multiscaled diamond pools
			while (it!=end) {
			//	too small pools are skipped
				while ( min(begin->width,begin->height) < 2*MinDomSize )
					++begin;
				ASSERT( halfShrinkOK(begin,it,zoom) );
				shrinkToHalf( begin->pixels, it->pixels, it->width, it->height );
			//	move on
				++it;
				++begin;
			}
		}//	if non-diamond else diamond

	//	we just handled the whole interval (of diamond or other type)

	}//	for (iterate over single-type intervals)

//	we filled all pools, let's prepare the summers
	for_each( pools.begin(), pools.end(), mem_fun_ref(&Pool::summers_makeValid) );
}//	::fillPixelsInPools

namespace NOSPACE {
	/** Computes the ideal domain density for pool, level and max.\ domain count,
	 *	the density is push_back-ed, returns the generated domain count (used once)
	 *	\relates MStandardDomains */
	inline static int bestDomainDensity( PoolIt pool, int level, int maxCount, int zoom
	, vector<short> &result) {
		level-= zoom;
		ASSERT( maxCount>=0 && level>0 && zoom>=0 );
		int wms= rShift<int>(pool->width,zoom) -powers[level];
		int hms= rShift<int>(pool->height,zoom) -powers[level];
	//	check whether any domain can fit and whether we should generate any more
		if ( wms<0 || hms<0 || !maxCount ) {
			result.push_back(0);
			return 0;
		}
	/*
		int dens= 1+(int)floor(sqrt( float(wms*hms)/maxCount ));
		int wdd= wms/dens, hdd= hms/dens;
		int count= (wdd+1)*(hdd+1);
		if (count>maxCount) {
			int wdiff= wms-wdd*dens, hdiff= hms-hdd*dens;
			dens+= 1+(int)(min( wdiff/float(wdd), hdiff/float(hdd) ));
			count= (wms/dens+1)*(hms/dens+1);
			ASSERT(count<=maxCount)x;
		}
		result.push_back(dens);
		return count;
	*/
	/*
		int xLowCount= (int)floor( sqrt(maxCount*wms/(double)hms) -1 );
		int yLowCount= (int)floor( sqrt(maxCount*hms/(double)wms) -1 );
		int dens= (int)ceil(max( wms/(double)xLowCount, hms/(double)yLowCount ));

		if (count<=maxCount) {
			dens= (short)floor(min( wms/xHighCount, hms/yHighCount ));
			ASSERT( wms/dens==xHighCount && hms/dens==yHighCount );
		} else {
			dens= (short)floor(max( wms/xHighCount, hms/yHighCount ));
			count= (wms/dens+1)*(hms/dens+1);

			if (count>maxCount) {
				dens= (short)floor(min( wms/xLowCount, hms/yLowCount ));
				count= (xLowCount+1)*(yLowCount+1);
				ASSERT( wms/dens==xLowCount && hms/dens==yLowCount );
			}
		}
	*/
		//int dens= (int)ceil(sqrt( wms*hms/(double)maxCount ));
		int dens;
		if (maxCount>1) {
			double temp= (  wms+hms+sqrt( sqr<double>(wms+hms) +double(4*wms*hms)*(maxCount-1) )  )
				/ ( 2*(maxCount-1) );
			dens= (int)ceil(temp);
		} else
			dens= 1+max(wms,hms);
		
		if (!dens)
			dens= 1;
		ASSERT(dens>0);
		int count= (wms/dens+1)*(hms/dens+1);
		ASSERT(count<=maxCount);

		result.push_back(dens);
		return count;
	}
	/** Generates (\p results.push_back) densities for domains on level \p level for
	 *	all pools of one type. It distributes at most \p maxCount domains among
	 *	the pools' scale-levels in one of three ways (\p divType)
	 *	and returns the number of generated domains \relates MStandardDomains */
	static int divideDomsInType( PoolIt begin, PoolIt end, int maxCount, int level
	, char divType, int zoom, vector<short> &results ) {
		ASSERT( begin!=end && divType>=0 && divType<=2 );
		int scaleLevels= (end-1)->level - begin->level + 1;
		int genCount= 0;
	//	iterate over same-scaleLevel intervals
		for (; begin!=end; --scaleLevels) {
			PoolIt it= begin+1;
			while ( it!=end && it->level==begin->level )
				++it;
		//	we have the same-scale interval, find out how many domains to generate for it
			int toGenerate= divType==2
			//	half per scale level
				? (maxCount-genCount)/2
			//	uniform dividing or no multiscaling (then scaleLevels==1)
				: (maxCount-genCount)/scaleLevels ;
		//	distribute it uniformly among the interval
		//	(there are more than one for diamond-type only)
			genCount+= toGenerate;	// genCount: "assume" we generate exactly toGenerate
			for (; begin!=it; ++begin)
				toGenerate-=
					bestDomainDensity( begin, level, toGenerate/(it-begin), zoom, results );
			genCount-= toGenerate;	// genCount: correct the count
		}
		return genCount;
	}
}
vector<short> MStdDomains::getLevelDensities(int level,int stdDomCountLog2) {
	ASSERT(level>=2);
//	compute the sum of shares, check for no-domain situations
	int totalShares= settingsInt(DomPortion_Standard) + settingsInt(DomPortion_Horiz)
	+ settingsInt(DomPortion_Vert) + settingsInt(DomPortion_Diamond);
	stdDomCountLog2-= (level-zoom-2)*settingsInt(MaxDomCountLevelDivisor);

	if ( pools.empty() || !totalShares || stdDomCountLog2<0 )
		return vector<short>( pools.size(), 0 );
//	get the real domain count for this level
	int domCountLeft= powers[stdDomCountLog2];

	vector<short> result;
	result.reserve(pools.size());
	PoolList::iterator begin, end;
	end= pools.begin();
//	iterate over single-type domain-pool intervals
	while ( end!=pools.end() ) {
		begin= end;
	//	find the end of the current interval
		while ( end!=pools.end() && begin->type==end->type )
			++end;
	//	we've got a single-type interval
		int share= settingsInt( (Settings)begin->type );
		domCountLeft-= divideDomsInType( begin, end, domCountLeft*share/totalShares
			, level, settingsInt(MultiDownScaling), zoom, result );
		totalShares-= share;
	}
//	check we created the correct number of densities
	ASSERT( result.size() == pools.size() );
	return result;
}

void MStdDomains::writeSettings(ostream &file) {
	ASSERT( this && settings );
//	all settings are small integers a need to be preserved
	int setLength= info().setLength;
	for (int i=0; i<setLength; ++i)
		put<Uchar>( file, settingsInt(i) );
}

void MStdDomains::readSettings(istream &file) {
	ASSERT( this && settings );
//	all settings are small integers a need to be preserved
	int setLength= info().setLength;
	for (int i=0; i<setLength; ++i)
		settingsInt(i)= get<Uchar>(file);
}





namespace MatrixWalkers {
	struct ReverseAssigner: public OperatorBase {
		template<class R1,class R2> void operator()(R1 &dest,R2 src) const { dest= src; }
	};

	template<class T,class I=PtrInt>
	struct HalfShrinker: public Rotation_0<T,I> { ROTBASE_INHERIT
		typedef Rotation_0<T,I> Base;
		
		HalfShrinker(TMatrix matrix)
		: Base( matrix, Block(0,0,0,0) ) {}

		T get() {
			TMatrix &c= current;
			T *cs= c.start;
			return ldexp( cs[0] + cs[1] + cs[c.colSkip] + cs[c.colSkip+1], -2 ); 
		}
		void outerStep() { Base::outerStep(); Base::outerStep(); }
		void innerStep() { Base::innerStep(); Base::innerStep(); }
	};
	
	template<class T,class I=PtrInt>
	struct HorizShrinker: public Rotation_0<T,I> { ROTBASE_INHERIT
		typedef Rotation_0<T,I> Base;
		
		bool addHalf; ///< Adds half a pixel to the current position
		
		HorizShrinker(TMatrix matrix)
		: Base( matrix, Block(0,0,0,0) ), addHalf(false) {}

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
	
	template<class T,class I=PtrInt>
	struct VertShrinker: public Rotation_0<T,I> { ROTBASE_INHERIT
		typedef Rotation_0<T,I> Base;
		
		bool addHalf; ///< Adds half a pixel to the current position
		
		VertShrinker(TMatrix matrix)
		: Base( matrix, Block(0,0,0,0) ) {}

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
	
	template<class T,class I=PtrInt> 
	struct DiamShrinker: public Rotation_0<T,I> { ROTBASE_INHERIT
		typedef Rotation_0<T,I> Base;
		
		DiamShrinker(TMatrix matrix,I side)
		: Base( matrix, Block(side-1,0,0,0) ) {} // the diamond begins on top-middle

		T get() {
			TMatrix &c= current;
			T *cs= c.start;
			return ldexp( cs[0] + cs[1] + cs[c.colSkip] + cs[c.colSkip+1], -2 ); 
		}
		void outerStep() { lastStart+= current.colSkip+1; }
		void innerStep() { current.start+= 1-current.colSkip; }
	};
}//	MatrixWalkers namespace


/** Implementations of forward-declared functions */
namespace NOSPACE {
	using namespace MatrixWalkers;

	/** Performs a simple 50\%^2 image shrink (dimensions belong to the destination)
	 *	\relates MStandardDomains */
	void shrinkToHalf( CSMatrix src, SMatrix dest, int width, int height ) {
		walkOperate( Checked<SReal>(dest,Block(0,0,width,height))
		, HalfShrinker<const SReal>(src), ReverseAssigner() );
	}
	
	/** Performs a simple 50\% image horizontal shrink (dimensions belong to the destination)
	 *	\relates MStandardDomains */
	void shrinkHorizontally( CSMatrix src, SMatrix dest, int width, int height ) {
		walkOperate( Checked<SReal>(dest,Block(0,0,width,height))
		, HorizShrinker<const SReal>(src), ReverseAssigner() );
	}
	
	/** Performs a simple 50\% image vertical shrink (dimensions belong to the destination)
	 *	\relates MStandardDomains */
	void shrinkVertically( CSMatrix src, SMatrix dest, int width, int height ) {
		walkOperate( Checked<SReal>(dest,Block(0,0,width,height))
		, VertShrinker<const SReal>(src), ReverseAssigner() );
	}
	
	/** Performs 50\% shrink with 45-degree anticlockwise rotation (\p side - the length of
	 *	the destination square; \p sx0, \p sy0 - the top-left of the enclosing source square)
	 *	\relates MStandardDomains */
	void shrinkToDiamond( CSMatrix src, SMatrix dest, int side ) {
		walkOperate( Checked<SReal>(dest,Block(0,0,side,side))
		, DiamShrinker<const SReal>(src,side), ReverseAssigner() );
	}//	shrinkToDiamond
}
