#include "interfaces.h"
#include "fileUtil.h"

using namespace std;

const IColorTransformer::Plane IColorTransformer::Plane
::Empty(SMatrix(),-1,-1,-1,0,UpdateInfo());

void IQuality2SquareError::completeSquareRangeErrors
( float quality, int levelEnd, float *errors ) {
	ASSERT( checkBoundsFunc<float>(0,quality,1)==quality && levelEnd>2 && errors );

	float (IQuality2SquareError::*func)(float,int)= &IQuality2SquareError::rangeSE;

	for (int level=2; level<levelEnd; ++level)
		errors[level]= (this->*func)( quality, powers[level*2] );
}

bool IRoot::allSettingsToFile(const char *fileName) {
	try {
		ofstream file( fileName, ios_base::binary|ios_base::trunc|ios_base::out );
		file.exceptions( ifstream::eofbit | ifstream::failbit | ifstream::badbit );
		put( file, SettingsMagic );
		saveAllSettings(file);
		return true;
	} catch(exception &e) {
		return false;
	}
}

bool IRoot::allSettingsFromFile(const char *fileName) {
	ASSERT(getMode()==Clear);
	try {
		ifstream file( fileName, ios_base::binary|ios_base::in );
		file.exceptions( ifstream::eofbit | ifstream::failbit | ifstream::badbit );
		if (get<Uint16>(file)!=SettingsMagic)
			return false;
		loadAllSettings(file);
		return true;
	} catch(exception &e) {
		return false;
	}
}
