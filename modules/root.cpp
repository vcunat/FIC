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
	zoom= 0;
//	get the plane list, create the jobs from it
	Plane planeProto( 0, quality(), settingsInt(DomainCountLog2), 0/*zoom*/, moduleQuality() );
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

bool MRoot::toStream(std::ostream &file) {
	assert( getMode()!=Clear && settings && moduleColor() && moduleShape() );
//	an exception is thrown on write/save errors
	try {
		file.exceptions( ofstream::eofbit | ofstream::failbit | ofstream::badbit );
		
		STREAM_POS(file);
	//	put the magic, the dimensions and child-module IDs
		put<Uint16>(file,Magic);
		put<Uint16>( file, rShift(width,zoom) );
		put<Uint16>( file, rShift(height,zoom) );
		file_saveModuleType( file, ModuleColor );
		file_saveModuleType( file, ModuleShape );
		
		STREAM_POS(file);
	//	put settings common for all the jobs
		put<Uchar>( file, settingsInt(DomainCountLog2) );
		
		STREAM_POS(file);
		moduleColor()->writeData(file);
		
		STREAM_POS(file);
		moduleShape()->writeSettings(file);
		
		STREAM_POS(file);
	//	put the workers' data
		moduleShape()->writeJobs(file);
		
		STREAM_POS(file);
		return true;
	} catch(exception &e) {
		return false;
	}
}

bool MRoot::fromStream(istream &file,int newZoom) {
	assert( newZoom>=0 && getMode()==Clear && settings && !moduleColor() && !moduleShape() );
	zoom= newZoom;
//	an exception is thrown on read/load errors
	try {
		file.exceptions( ifstream::eofbit | ifstream::failbit | ifstream::badbit );
		
		STREAM_POS(file);
	//	check the magic number, load the dimensions and child-module types
		if (get<Uint16>(file) != Magic)
			return false;
		this->width= lShift( (int)get<Uint16>(file), zoom );
		this->height= lShift( (int)get<Uint16>(file), zoom );
		file_loadModuleType( file, ModuleColor );
		file_loadModuleType( file, ModuleShape );
		
		STREAM_POS(file);
	//	create the planes and get settings common for all the jobs
		Plane planeProto(Plane::Empty);
		planeProto.zoom= zoom;
		planeProto.domainCountLog2= settingsInt(DomainCountLog2)= get<Uchar>(file);
		
		STREAM_POS(file);
		PlaneList planes= moduleColor()->readData(file,planeProto,width,height);
		
		STREAM_POS(file);
		moduleShape()->readSettings(file);
		
		STREAM_POS(file);
	//	create the jobs (from the plane list) and get their data
		moduleShape()->createJobs(planes,width,height);
		moduleShape()->readJobs(file);
		
		STREAM_POS(file);
		myMode= Decode;
		return true;
	} catch(exception &e) {
		return false;
	}
}
