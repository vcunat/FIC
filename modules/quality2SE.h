#ifndef QUALITY2SE_HEADER_
#define QUALITY2SE_HEADER_

#include "../interfaces.h"

/** Standard quality-to-SE module - uses fixed SE for all block sizes */
class MQuality2SE_std: public IQuality2SquareError {

	DECLARE_TypeInfo_noSettings( MQuality2SE_std, "Constant square error"
	, "Holds the same <b>square error</b> (SE) for all block sizes" )

public:
/**	\name IQuality2SquareError interface
 *	@{ */
	float rangeSE(float quality,int /*pixelCount*/) {
		ASSERT( quality>=0 && quality<=1 );
		return 1-cbrt(quality);
	}
///	@}
};


#endif // QUALITY2SE_HEADER_
