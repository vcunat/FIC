#ifndef QUALITY2SE_HEADER_
#define QUALITY2SE_HEADER_

#include "../headers.h"


/// \ingroup modules
/** Standard quality-to-SE module - uses fixed SE for all block sizes */
class MQuality2SE_std: public IQuality2SE {

	DECLARE_TypeInfo_noSettings( MQuality2SE_std, "Constant square error"
	, "Holds the same <b>square error</b> (SE) for all block sizes" )

public:
/**	\name IQuality2SE interface
 *	@{ */
	float rangeSE(float quality,int /*pixelCount*/) {
		ASSERT( quality>=0 && quality<=1 );
		float maxSE= 4	//  approximate SE for quality=0
		, doubles= 6;	//< how many times the SE doubles
		return maxSE/exp2(doubles) * ( exp2((1-quality)*doubles) - 1 );
	}
///	@}
};


/// \ingroup modules
/** Alternate quality-to-SE module - uses fixed MSE for all block sizes */
class MQuality2SE_alt: public IQuality2SE {

	DECLARE_TypeInfo_noSettings( MQuality2SE_alt, "Constant mean square error"
	, "Holds the same <b>mean square error</b> (MSE) for all block sizes" )

public:
/**	\name IQuality2SE interface
 *	@{ */
	float rangeSE(float quality,int pixelCount) {
		ASSERT( quality>=0 && quality<=1 );
		float maxSE= 4	//  approximate SE for quality=0
		, doubles= 6;	//< how many times the SE doubles
		float sizeQuot= pixelCount/sqr(9.0);
		return maxSE/exp2(doubles) * ( exp2((1-quality)*doubles) - 1 ) * sizeQuot;
	}
///	@}
};


#endif // QUALITY2SE_HEADER_
