#include "stdDomains.h"
#include "../fileUtil.h"

enum { MinRangeSize=4, DiamondOverlay=MinRangeSize-1 };
using namespace std;


namespace NOSPACE {
	struct PoolTypeLevelComparator {
		typedef MStandardDomains::Pool Pool;
		bool operator()(const Pool &a,const Pool &b) {
			if (a.type!=b.type)
				return a.type<b.type;
			else
				return a.level<b.level;
		}
	};
}
void MStandardDomains::initPools(int width_,int height_) {
	width=width_;
	height=height_;
//	checks some things
	assert( width>0 &&height>0 && this && settings && pools.empty() );
	if ( width<MinRangeSize*2 || height<MinRangeSize*2 )
	//	no domains possible
		return;
//	create the first set of domain pools for standard, horizontal and vertical domains
	if ( settingsInt(DomPortion_Standard) )
		pools.push_back(Pool( width/2, height/2, DomPortion_Standard, 1, 0.5f ));
	if ( settingsInt(DomPortion_Horiz) )
		pools.push_back(Pool( width/2, height, DomPortion_Horiz, 1, M_SQRT1_2 ));
	if ( settingsInt(DomPortion_Vert) )
		pools.push_back(Pool( width, height/2, DomPortion_Vert, 1, M_SQRT1_2 ));
//	create the first set of domain pools for diamond domains
	if ( settingsInt(DomPortion_Diamond) ) {
	//	get longer and shorter dimension
		int longer,shorter;
		if (width>=height) {
			longer=width/2;
			shorter=height/2;
		} else {
			longer=height/2;
			shorter=width/2;
		}
	//	generate the pools
		while (longer>=MinRangeSize) {
			int side=std::min(longer,shorter);
			pools.push_back(Pool( side, side, DomPortion_Diamond, 1, M_SQRT1_2 ));
			longer-= side-DiamondOverlay;
		}
	}
//	if allowed, create more downscaled domains (at most 256 pools)
	if ( !settingsInt(MultiDownScaling) )
		return;
	for (size_t i=0; i<pools.size() && i<256; ++i) {
		const Pool &pool=pools[i];
		if ( pool.width>=MinRangeSize*2 && pool.height>=MinRangeSize*2 )
			pools.push_back(
				Pool( pool.width/2, pool.height/2, pool.type
				, pool.level+1, ldexp(pool.contrFactor,-1) )
			);
	}
//	sort the pools according to their types and levels
	sort( pools.begin(), pools.end(), PoolTypeLevelComparator() );
}

namespace NOSPACE {
	typedef MStandardDomains::Pools::const_iterator PoolIt;
	static inline bool halfShrinkOK(PoolIt src,PoolIt dest) {
		return src->level+1==dest->level
		&& src->width/2==dest->width && src->height/2==dest->height;
	}
//	forwards, implemeted and described at the end of the file
	static void shrinkToHalf
	( const float **src, int srcYadd, float **dest, int width, int height );
	static void shrinkHorizontally
	( const float **src, int srcYadd, float **dest, int width, int height );
	static void shrinkVertically
	( const float **src, int srcYadd, float **dest, int width, int height );
	static void shrinkToDiamond( const float **src, float **dest, int side, int sx0, int sy0 );
}
void MStandardDomains::fillPixelsInPools(const PlaneBlock &ranges) {
	assert( !pools.empty() );
//	iterate over pool types
	for ( Pools::iterator end=pools.begin(); end!=pools.end(); ) {
		Pools::iterator begin=end;
		char type=begin->type;
	//	find the end of the same-pool-type block and invalidate the summers on the way
		while ( end!=pools.end() && end->type==type ) {
			end->summers[0].invalidate();
			end->summers[1].invalidate();
			++end;
		}

	//	we've got the interval, find out about the type
		if ( type!=DomPortion_Diamond ) {
		//	non-diamond domains all behave similarly
			void (*shrinkProc)(const float**,int,float**,int,int);
			if (type==DomPortion_Standard)
				shrinkProc=&shrinkToHalf; else
			if (type==DomPortion_Horiz)
				shrinkProc=&shrinkHorizontally; else
			if (type==DomPortion_Vert)
				shrinkProc=&shrinkVertically;
			else
				assert(false),shrinkProc=0;
		//	we have the right procedure -> fill the first pool
			assert( begin->level==1 );
			shrinkProc( (const float**)ranges.pixels+ranges.x0, ranges.y0
			, begin->pixels, begin->width, begin->height );
		//	fill the rest (in the same-type interval)
			while ( ++begin!=end ) {
				assert( halfShrinkOK(begin-1,begin) );
				shrinkToHalf( (const float**)(begin-1)->pixels, 0
				, begin->pixels, begin->width, begin->height );
			}

		} else { //	handle diamond-type domains
		//	fill the first set of diamond-type domain pools
			Pools::iterator it=begin;
			if (width>=height) {
				int xEnd=width-2*MinRangeSize;
				int xStep=height-2*DiamondOverlay;
				for (int x=0; x<=xEnd; x+=xStep) {
					assert( it!=end && it->level==1 );
					shrinkToDiamond( (const float**)ranges.pixels, it->pixels, it->width
					, x+ranges.x0, 0+ranges.y0 );
					++it;
				}
			} else {
				int yEnd=height-2*MinRangeSize;
				int yStep=width-2*DiamondOverlay;
				for (int y=0; y<=yEnd; y+=yStep) {
					assert( it!=end && it->level==1 );
					shrinkToDiamond( (const float**)ranges.pixels, it->pixels, it->width
					, 0+ranges.x0, y+ranges.y0 );
					++it;
				}
			}
		//	now fill the multiscaled diamond pools
			while ( it!=end ) {
			//	skip too small pools
				while ( begin->width<2*MinRangeSize || begin->height<2*MinRangeSize )
					++begin;
				assert( halfShrinkOK(begin,it) );
				shrinkToHalf( (const float**)begin->pixels, 0
				, it->pixels, it->width, it->height );
				++it;
			}
		}//	if non-diamond else diamond

	//	we just handled the whole interval (of diamond or other type)

	}//	for (iterate over single-type intervals)

//	we filled all pools

}//	::fillPixelsInPools

namespace NOSPACE {
	/** Computes the ideal domain density for pool, level and max. domain count,
	 *	the density is push_back-ed, returns the generated domain count (used once) */
	inline static int bestDomainDensity( PoolIt pool, int level, int maxCount
	, std::vector<short> &result ) {
		int wms=pool->width-powers[level];
		int hms=pool->height-powers[level];
	//	check whether any domain can fit
		if ( wms<0 || hms<0 ) {
			result.push_back(0);
			return 0;
		}
		int dens=1+(int)floor(sqrt( float(wms*hms)/maxCount ));
		int wdd=wms/dens, hdd=hms/dens;
		int count=(wdd+1)*(hdd+1);
		if (count>maxCount) {
			int wdiff=wms-wdd*dens, hdiff=hms-hdd*dens;
			dens+= 1+(int)(min( wdiff/float(wdd), hdiff/float(hdd) ));
			count=(wms/dens+1)*(hms/dens+1);
			//assert(count<=maxCount);
		}
		result.push_back(dens);
		return count;
	}
	/** Generates (results.push_back) densities for same-scale domain pools (interval),
	 *	supports 2+1 ways of dividing (divType), returns the number of generated domains */
	static int divideDomsInType( PoolIt begin, PoolIt end, int maxCount, int level
	, char divType, std::vector<short> &results ) {
		assert( begin!=end && divType>=0 && divType<=2 );
		int scaleLevels= (end-1)->level - begin->level + 1;
		int genCount=0;
	//	iterate over same-scaleLevel intervals
		for (; begin!=end; --scaleLevels) {
			PoolIt it=begin+1;
			while ( it!=end && it->level==begin->level )
				++it;
		//	we have the same-scale interval, find out how many domains to generate for it
			int toGenerate= divType==2
			//	half per scale
				? (maxCount-genCount)/2
			//	uniform dividing or no multiscaling (then scaleLevels==1)
				: (maxCount-genCount)/scaleLevels ;
		//	distribute it uniformly among the interval (there are more for diamonds only)
			genCount+=toGenerate;
			for (; begin!=it; ++begin)
				toGenerate-=
					bestDomainDensity( begin, level, toGenerate/(it-begin), results );
			genCount-=toGenerate;
		}
		return genCount;
	}
}
std::vector<short> MStandardDomains::getLevelDensities(int level,int stdDomCountLog2) {
	assert(level>=2);
//	compute the sum of shares, check for no-domain situations
	int totalShares= settingsInt(DomPortion_Standard) + settingsInt(DomPortion_Horiz)
	+ settingsInt(DomPortion_Vert) + settingsInt(DomPortion_Diamond);
	stdDomCountLog2-=(level-2)*settingsInt(MaxDomCountLevelDivisor);

	if ( pools.empty() || !totalShares || stdDomCountLog2<0 )
		return std::vector<short>( pools.size(), 0 );
//	get the real domain count for this level
	int domCountLeft=powers[stdDomCountLog2];

	std::vector<short> result;
	result.reserve(pools.size());
	Pools::iterator begin,end;
	end=pools.begin();
//	iterate over single-type domain-pool intervals
	while ( end!=pools.end() ) {
		begin=end;
		while ( end!=pools.end() && begin->type==end->type )
			++end;
	//	we've got a single-type interval
		int share=settingsInt( (Settings)begin->type );
		domCountLeft-=divideDomsInType( begin, end, domCountLeft*share/totalShares
		, level, settingsInt(MultiDownScaling), result );
		totalShares-=share;
	}
//	check we created the correct number of densities (at least)
	assert( result.size()==pools.size() );
	return result;
}

void MStandardDomains::writeSettings(std::ostream &file) {
	assert( this && settings );
//	all settings are form int:{0..8}
	for (int i=0; i<settingsLength_; ++i)
		put<Uchar>( file, settings[i].i );
}

void MStandardDomains::readSettings(std::istream &file) {
	assert( this && settings );
//	all settings are form int:{0..8}
	for (int i=0; i<settingsLength_; ++i)
		settings[i].i=get<Uchar>(file);
}


/** Implementations of forward-declared functions */
namespace NOSPACE {
	/** Performs a simple 50%^2 image shrink (dimensions belong to the destination) */
	void shrinkToHalf( const float **src, int srcYadd, float **dest, int width, int height ) {
	//	iterate over columns of the destination matrix
		for (float **destEnd=dest+width; dest!=destEnd; ++dest ) {
		//	get two source columns (and increment the source)
			const float *srcCol1= (*src++) + srcYadd;
			const float *srcCol2= (*src++) + srcYadd;
		//	get begin and end of the destination column
			float *destCol=*dest;
			float *destColEnd=*dest+height;
		//	iterate over the destination column and average the source pixels
			for (; destCol!=destColEnd; ++destCol ) {
				*destCol= ldexp( *srcCol1 + *(srcCol1+1) + *srcCol2 + *(srcCol2+1), -2 );
				srcCol1+=2;
				srcCol2+=2;
			}
		}
	}//	shrinkToHalf
	/** Performs a simple 50% image horizontal shrink (dimensions... destination) */
	void shrinkHorizontally( const float **src, int srcYadd, float **dest, int width, int height ) {
	//	iterate over columns of the destination matrix
		for (float **destEnd=dest+width; dest!=destEnd; ++dest ) {
		//	get two source columns (and increment the source)
			const float *srcCol1= (*src++) + srcYadd;
			const float *srcCol2= (*src++) + srcYadd;
		//	get begin and end of the destination column
			float *destCol=*dest;
			float *destColEnd=*dest+height;
		//	iterate over the destination column and average the source pixels
			for (; destCol!=destColEnd; ++destCol )
				*destCol= ldexp( (*srcCol1++)+(*srcCol2++), -1 );
		}
	}//	shrinkHorizontally
	/** Performs a simple 50% image vertical shrink (dimensions... destination) */
	void shrinkVertically( const float **src, int srcYadd, float **dest, int width, int height ) {
	//	iterate over columns of the destination matrix
		for (float **destEnd=dest+width; dest!=destEnd; ++dest ) {
		//	get one source column (and increment the source)
			const float *srcCol= (*src++) + srcYadd;
		//	get begin and end of the destination column
			float *destCol=*dest;
			float *destColEnd=*dest+height;
		//	iterate over the destination column and average the source pixels
			for (; destCol!=destColEnd; ++destCol ) {
				*destCol= ldexp( *srcCol + *(srcCol+1), -1 );
				srcCol+=2;
			}
		}
	}//	shrinkVertically
	/** Performs 50% shrink with 45-degree anticlockwise rotation (side - the length of
	 *	the destination square; sx0,sy0 - the top-left of the enclosing source square) */
	void shrinkToDiamond( const float **src, float **dest, int side, int sx0, int sy0 ) {
	//	the diamond begins on top-middle
		sx0+=side-1;
	//	iterate over the columns of the destination matrix (both source coords. increment)
		for (float **destEnd=dest+side; dest!=destEnd; ++dest,++sx0,++sy0) {
			int sx=sx0, sy=sy0;
			float *destCol=*dest;
		//	in a column, the source-x decreases and source-y grows
			for (float *destColEnd=destCol+side; destCol!=destColEnd; ++destCol,--sx,++sy)
				*destCol=
					ldexp( src[sx][sy]+src[sx][sy+1]+src[sx+1][sy]+src[sx+1][sy+1], -2 );
		}
	}//	shrinkToDiamond
}
