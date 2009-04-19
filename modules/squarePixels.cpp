#include "squarePixels.h"
#include "../fileUtil.h"

using namespace std;

int MSquarePixels::createJobs(const PlaneList &planes) {
	ASSERT( jobs.empty() && !planes.empty()
		&& moduleRanges() && moduleDomains() && moduleEncoder() );
	DEBUG_ONLY( planeList= planes; )

	for (PlaneList::const_iterator plane= planes.begin(); plane!=planes.end(); ++plane) {
	//	convert the Plane into a PlaneBlock (containing the whole contents)
		const IColorTransformer::PlaneSettings *plSet= plane->settings;
		Job job;
		job.width= plSet->width;
		job.height= plSet->height;
		job.pixels= plane->pixels;
		job.sumsValid= false;
		job.settings= plSet;
		DEBUG_ONLY(	job.ranges= 0; job.domains= 0; job.encoder= 0; )
	//	append the result to the jobs
		jobs.push_back(job);
	}
			
//	the zoom only affects maxPixels (zoom is assumed to be the same for all planes)
	int maxPixels= powers[maxPartSize()+2*jobs.front().settings->zoom];
//	split jobs until they're small enough
	Uint i= 0;
	while ( i < jobs.size() )
		if ( jobs[i].width*jobs[i].height <= maxPixels ) {
			++i; //	the part is small enough, move on
		} else {
			
			// divide the job
		//	splitting the longer coordinate
			bool xdiv= ( jobs[i].width >= jobs[i].height );
			int longer= ( xdiv ? jobs[i].width : jobs[i].height );
		//	get the place to split (at least one of the parts will have size 2^q)
			int bits= log2ceil(longer);
			int divSize= ( longer >= powers[bits-1]+powers[bits-2]
				? powers[bits-1]
				: powers[bits-2] );
		//	split the job (reusing the splitted-one's space and appending the second one)
			jobs.push_back(jobs[i]);
			if (xdiv) {
				jobs[i].width= divSize;				// reducing the width of the first job
				jobs.back().pixels.shiftMatrix(divSize,0);	// shifting the second job
				jobs.back().width-= divSize;		// reducing the width of the second job
			} else {
				jobs[i].height= divSize;			// reducing the height of the first job
				jobs.back().pixels.shiftMatrix(0,divSize);	// shifting the second job
				jobs.back().height-= divSize;		// reducing the height of the second job
			}
		}

//	create the modules in the jobs by cloning those from module's settings
	for (JobIterator job=jobs.begin(); job!=jobs.end(); ++job) {
		job->ranges= clone(moduleRanges());
		job->domains= clone(moduleDomains());
		job->encoder= clone(moduleEncoder());
	}

	return jobs.size();
}

void MSquarePixels::writeSettings(ostream &file) {
	ASSERT( moduleRanges() && moduleDomains() && moduleEncoder() );
//	put settings and ID's of child modules
	put<Uchar>( file, maxPartSize() );
	file_saveModuleType( file, ModuleRanges );
	file_saveModuleType( file, ModuleDomains );
	file_saveModuleType( file, ModuleEncoder );
//	put settings of child modules
	moduleRanges()->writeSettings(file);
	moduleDomains()->writeSettings(file);
	moduleEncoder()->writeSettings(file);
}

void MSquarePixels::readSettings(istream &file) {
	ASSERT( !moduleRanges() && !moduleDomains() && !moduleEncoder() );
//	get settings and create the right child modules
	maxPartSize()= get<Uchar>(file);
	file_loadModuleType( file, ModuleRanges );
	file_loadModuleType( file, ModuleDomains );
	file_loadModuleType( file, ModuleEncoder );
//	get settings of child modules
	moduleRanges()->readSettings(file);
	moduleDomains()->readSettings(file);
	moduleEncoder()->readSettings(file);
}

void MSquarePixels::writeJobs(ostream &file,int phaseBegin,int phaseEnd) {
	ASSERT( !jobs.empty() && 0<=phaseBegin && phaseBegin<phaseEnd && phaseEnd<=phaseCount() );
//	if writing phase 0, for each job: write data of domain and range modules
	if (!phaseBegin)
		for (JobIterator it=jobs.begin(); it!=jobs.end(); ++it) {
			STREAM_POS(file);
			it->ranges->writeData(file);
			STREAM_POS(file);
			it->domains->writeData(file);
		}
//	write all the requested phases of all jobs (phase-sequentially)
	for (int phase=phaseBegin; phase<phaseEnd; ++phase)
		for (JobIterator it=jobs.begin(); it!=jobs.end(); ++it)
			STREAM_POS(file), it->encoder->writeData(file,phase);
}

void MSquarePixels::readJobs(istream &file,int phaseBegin,int phaseEnd) {
	ASSERT( !jobs.empty() && 0<=phaseBegin && phaseBegin<phaseEnd && phaseEnd<=phaseCount() );
//	if reading phase 0, for each job: read data of domain and range modules
	if (!phaseBegin)
		for (JobIterator it=jobs.begin(); it!=jobs.end(); ++it) {
			STREAM_POS(file);
			it->ranges->readData_buildRanges(file,*it);
			STREAM_POS(file);
			it->domains->readData(file);
			it->encoder->initialize(IRoot::Decode,*it);
		}
//	read all the requested phases of all jobs (phase-sequentially)
	for (int phase=phaseBegin; phase<phaseEnd; ++phase)
		for (JobIterator it=jobs.begin(); it!=jobs.end(); ++it) 
			STREAM_POS(file), it->encoder->readData(file,phase);
}
