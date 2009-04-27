#include "root.h"
#include "../util.h"
#include "../fileUtil.h"

#include <QImage>
#include <QThreadPool>

using namespace std;


QImage MRoot::toImage() {
	ASSERT( getMode()!=Clear && settings && moduleColor() && moduleShape() );
	return moduleColor()->planes2image();
}

namespace NOSPACE {
	/** Represents a scheduled encoding job for use in QThreadPool */
	class ScheduledJob: public QRunnable {
		IShapeTransformer *worker;	///< the worker to perform the job
		int job;					///< the job's identifier
		volatile bool &errorFlag;	///< the flag to set in case of failure
	public:
		/** Creates a new scheduled job, failure reported in \p errorFlag_ */
		ScheduledJob( IShapeTransformer *worker_, int job_, volatile bool &errorFlag_ )
		: worker(worker_), job(job_), errorFlag(errorFlag_) {}
		
		/** Just makes #worker do the #job and sets #errorFlag in case of error (virtual method) */
		void run() {
			try {
				worker->jobEncode(job);
			} catch (exception &e) {
				errorFlag= true;
			}
		}
	}; // ScheduledJob class
}
bool MRoot::encode(const QImage &toEncode,const UpdateInfo &updateInfo) {
	ASSERT( getMode()==Clear && settings && moduleColor() && moduleShape() 
		&& maxThreads()>=1 );
//	set my zoom and dimensions
	zoom= 0;
	this->width= toEncode.width();
	this->height= toEncode.height();
//	get the plane list, create the jobs from it
	PlaneSettings planeProto( width, height, settingsInt(DomainCountLog2), 0/*zoom*/
		, quality(), moduleQuality(), updateInfo );
	PlaneList planes= moduleColor()->image2planes( toEncode, planeProto );
	int jobCount= moduleShape()->createJobs(planes);
//	process the jobs
	if (maxThreads()==1)
	//	simple one-thread mode: pocesses jobs sequentially, returns false on failure
		try {
			for (int i=0; i<jobCount; ++i)
				moduleShape()->jobEncode(i);
		} catch (exception &e) {
			return false;
		}
	else {
	//	multi-threaded mode: create ScheduleJob instances and push them into a QThreadPool
		volatile bool errorFlag= false;
		QThreadPool jobPool;
		jobPool.setMaxThreadCount( maxThreads() );
		for (int job=0; job<jobCount; ++job)
			jobPool.start( new ScheduledJob(moduleShape(),job,errorFlag) );
		jobPool.waitForDone();
		if (errorFlag)
			return false;
	}
//	encoding successful - change the mode and return true
	myMode= Encode;
	return true;
}

void MRoot::decodeAct(DecodeAct action,int count) {
	ASSERT( getMode()!=Clear && settings && moduleColor() && moduleShape() );
	int jobCount= moduleShape()->jobCount();
	ASSERT(jobCount>0);
//	there will be probably no need to parallelize decoding
	for (int i=0; i<jobCount; ++i)
		moduleShape()->jobDecodeAct(i,action,count);
}

bool MRoot::toStream(std::ostream &file) {
	ASSERT( getMode()!=Clear && settings && moduleColor() && moduleShape() );
//	an exception is thrown on write/save errors
	try {
		file.exceptions( ofstream::eofbit | ofstream::failbit | ofstream::badbit );
		
		STREAM_POS(file);
	//	put the magic, the dimensions (not zoomed) and child-module IDs
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
	ASSERT( newZoom>=0 && getMode()==Clear && settings && !moduleColor() && !moduleShape() );
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
		settingsInt(DomainCountLog2)= get<Uchar>(file);
		PlaneSettings planeProto( width, height, settingsInt(DomainCountLog2), zoom );
		
		STREAM_POS(file);
		PlaneList planes= moduleColor()->readData(file,planeProto);
		
		STREAM_POS(file);
		moduleShape()->readSettings(file);
		
		STREAM_POS(file);
	//	create the jobs (from the plane list) and get their data
		moduleShape()->createJobs(planes);
		moduleShape()->readJobs(file);
		
		STREAM_POS(file);
		myMode= Decode;
		return true;
	} catch(exception &e) {
		return false;
	}
}
