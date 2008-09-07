#include "root.h"
#include "../util.h"
#include "../fileUtil.h"

#include <QImage>

using namespace std;


QImage MRoot::toImage() {
	assert( getMode()!=Clear && settings && moduleColor() && moduleShape() );
	return moduleColor()->planes2image( moduleShape()->collectJobs() , width, height );
}

bool MRoot::encode(const QImage &toEncode) {
	assert( getMode()==Clear && settings && moduleColor() && moduleShape() );
//	get the plane list, create the jobs from it
	Plane planeProto( 0, quality(), settingsInt(DomainCountLog2), moduleQuality() );
	PlaneList planes= moduleColor()->image2planes( toEncode, planeProto );
	this->width= toEncode.width();
	this->height= toEncode.height();
	int jobCount= moduleShape()->createJobs( planes, width, height );
//	process the jobs
	if ( maxThreads() == 1 )
		try {
			for (int i=0; i<jobCount; ++i)
				moduleShape()->jobEncode(i);
		} catch (exception &e) {
			return false;
		}
	else
	//	multithreaded
	// TODO (admin#7#): Multi-threaded encoding
		return assert(false),false;

	myMode= Encode;
	return true;
}

void MRoot::decodeAct(DecodeAct action,int count) {
	assert( getMode()!=Clear && settings && moduleColor() && moduleShape() );
	int jobCount= moduleShape()->jobCount();
	assert(jobCount>0);
//	there will be probably no need to parallelize decoding
	for (int i=0; i<jobCount; ++i)
		moduleShape()->jobDecodeAct(i,action,count);
}

bool MRoot::toFile(const char *fileName) {
	assert( getMode()==Encode && settings && moduleColor() && moduleShape() );
//	open the file and check the successfullness
	ofstream file( fileName, ios_base::binary|ios_base::trunc|ios_base::out );
	if ( !file.good() )
		return false;
	try {
	//	put the magic, the dimensions and child-module IDs
		put<Uint16>(file,Magic);
		put<Uint16>(file,width);
		put<Uint16>(file,height);
		file_saveModuleType( file, ModuleColor );
		file_saveModuleType( file, ModuleShape );
	//	put settings common for all the jobs
		put<Uchar>( file, settingsInt(DomainCountLog2) );
		moduleColor()->writeData(file);
		moduleShape()->writeSettings(file);
	//	put the workers' data
		moduleShape()->writeJobs(file);
		return true;
	} catch(exception &e) {
		return false;
	}
}

bool MRoot::fromFile(const char *fileName) {
	assert( getMode()==Clear && settings && !moduleColor() && !moduleShape() );
//	open the file and check the successfullness
	ifstream file( fileName, ios_base::binary|ios_base::in );
	if ( !file.good() )
		return false;
	try {
	//	check the magic number, load the dimensions and child-module types
		if (get<Uint16>(file) != Magic)
			return false;
		this->width= get<Uint16>(file);
		this->height= get<Uint16>(file);
		file_loadModuleType( file, ModuleColor );
		file_loadModuleType( file, ModuleShape );
	//	create the planes and get settings common for all the jobs
		Plane planeProto(Plane::Empty);
		planeProto.domainCountLog2= settingsInt(DomainCountLog2)= get<Uchar>(file);
		PlaneList planes= moduleColor()->readData(file,planeProto,width,height);
		moduleShape()->readSettings(file);
	//	create the jobs (from the plane list) and get their data
		moduleShape()->createJobs(planes,width,height);
		moduleShape()->readJobs(file);
		myMode= Decode;
		return true;
	} catch(exception &e) {
		return false;
	}
}
