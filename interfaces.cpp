#include "interfaces.h"

const IColorTransformer::Plane IColorTransformer::Plane::Empty(0,-1,-1,-1,0);

void IQuality2SquareError::completeSquareRangeErrors
( float quality, int levelEnd, float *errors ) {
	assert( checkBoundsFunc<float>(0,quality,1)==quality && levelEnd>2 && errors );

	float (IQuality2SquareError::*func)(float,int)= &IQuality2SquareError::rangeSE;

	for (int level=2; level<levelEnd; ++level)
		errors[level]= (this->*func)( quality, powers[level*2] );
}
