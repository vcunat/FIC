#ifndef QUALITY2SE_HEADER_
#define QUALITY2SE_HEADER_

#include "../interfaces.h"

/** Standard quality-to-SE module - uses fixed SE for all block sizes */
class MQuality2SE_std: public IQuality2SE {

	DECLARE_TypeInfo_noSettings( MQuality2SE_std, "Constant square error"
	, "Holds the same <b>square error</b> (SE) for all block sizes" )

public:
/**	\name IQuality2SE interface
 *	@{ */
	float rangeSE(float quality,int /*pixelCount*/) {
		ASSERT( quality>=0 && quality<=1 );
		float maxSE= 4	///  approximate SE for quality=0
		, doubles= 6;	///< how many times the SE doubles
		return maxSE/exp2(doubles) * ( exp2((1-quality)*doubles) - 1 );
	}
///	@}
};


#endif // QUALITY2SE_HEADER_
