#include "headers.h"
#include "fileUtil.h"

using namespace std;

bool IRoot::allSettingsToFile(const char *fileName) {
	try {
		ofstream file( fileName, ios_base::binary|ios_base::trunc|ios_base::out );
		file.exceptions( ifstream::eofbit | ifstream::failbit | ifstream::badbit );
		put( file, SettingsMagic );
		file_saveAllSettings(file);
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
		file_loadAllSettings(file);
		return true;
	} catch(exception &e) {
		return false;
	}
}


void IQuality2SE::regularRangeErrors( float quality, int levelEnd, float *errors ) {
	ASSERT( checkBoundsFunc<float>(0,quality,1)==quality && levelEnd>2 && errors );

	float (IQuality2SE::*func)(float,int)= &IQuality2SE::rangeSE;

	for (int level=0; level<levelEnd; ++level)
		errors[level]= (this->*func)( quality, powers[level*2] );
}
