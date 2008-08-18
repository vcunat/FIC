#ifndef QUALITY2SE_HEADER_
#define QUALITY2SE_HEADER_

#include "../interfaces.h"

class MQuality2SE_std: public IQuality2SquareError {

	DECLARE_M_cloning_name_desc( MQuality2SE_std, "Constant square error"
	, "Holds the same <b>square error</b> (SE) for all block sizes" )

	DECLARE_M_settings_none()
public:
//	IQuality2SquareError interface
	float rangeSE(float quality,int /*pixelCount*/) {
		assert( quality>=0 && quality<=1 );
		return 1-sqr(quality);
	}
};


#endif // QUALITY2SE_HEADER_
