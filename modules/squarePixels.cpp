#include "squarePixels.h"
#include "../fileUtil.h"

using namespace std;

int MSquarePixels::createJobs( const PlaneList &planes, int width, int height ) {
	assert( ownedMatrices.empty() && jobs.empty()
	&& moduleRanges() && moduleDomains() && moduleEncoder() );

//	create the joblist
	int maxPixels= powers[maxPartSize()];
	if ( width*height <= maxPixels )
	//	not dividing the planes -> pass them as they are
		for (PlaneList::const_iterator it=planes.begin(); it!=planes.end(); ++it)
			jobs.push_back(makeJob( *it, Block(0,0,width,height) ));
	else { // dividing the planes
	//	compute the dividing pattern
		vector<Block> pattern;
		pattern.push_back( Block(0,0,width,height) );

		size_t i= 0;
		while ( i < pattern.size() )
			if ( pattern[i].size() <= maxPixels )
			//	the part is small enough, move on
				++i;
			else { // divide the part
			//	splitting the longer coordinate
				bool xdiv= ( pattern[i].width() >= pattern[i].height() );
				int longer= ( xdiv ? pattern[i].width() : pattern[i].height() );
			//	get the place to split (at least one of the parts will have size 2^q)
				int bits= log2ceil(longer);
				int divsize= ( longer >= powers[bits-1]+powers[bits-2]
					? powers[bits-1]
					: powers[bits-2] );
			//	split the part in the pattern (reusing the splitted-one's space)
				pattern.push_back(pattern[i]);
				if (xdiv) {
					pattern[i].xend-= divsize;
					pattern.back().x0+= divsize;
				} else {
					pattern[i].yend-= divsize;
					pattern.back().y0+= divsize;
				}
			}
	//	we have the pattern, apply it to every plane (jobs sharing pixel matrices)
		vector<Block>::iterator patIt;
		for (PlaneList::const_iterator it=planes.begin(); it!=planes.end(); ++it)
			for (patIt=pattern.begin(); patIt!=pattern.end(); ++patIt)
				jobs.push_back(makeJob( *it, *patIt ));
	}

//	take the ownership of the matrices
	for (PlaneList::const_iterator it=planes.begin(); it!=planes.end(); ++it)
		ownedMatrices.push_back(it->pixels);

	return jobs.size();
}

void MSquarePixels::writeSettings(ostream &file) {
	assert( moduleRanges() && moduleDomains() && moduleEncoder() );
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
	assert( !moduleRanges() && !moduleDomains() && !moduleEncoder() );
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
	assert( !jobs.empty() && 0<=phaseBegin && phaseBegin<phaseEnd && phaseEnd<=phaseCount() );
//	if writing phase 0, for each job: write data of domain and range modules
	if (!phaseBegin)
		for (JobIterator it=jobs.begin(); it!=jobs.end(); ++it) {
			it->ranges->writeData(file);
			it->domains->writeData(file);
		}
//	write all the requested phases of all jobs (phase-sequentially)
	for (int phase=phaseBegin; phase<phaseEnd; ++phase)
		for (JobIterator it=jobs.begin(); it!=jobs.end(); ++it)
			it->encoder->writeData(file,phase);
}

void MSquarePixels::readJobs(istream &file,int phaseBegin,int phaseEnd) {
	assert( !jobs.empty() && 0<=phaseBegin && phaseBegin<phaseEnd && phaseEnd<=phaseCount() );
//	if reading phase 0, for each job: read data of domain and range modules
	if (!phaseBegin)
		for (JobIterator it=jobs.begin(); it!=jobs.end(); ++it) {
			it->ranges->readData_buildRanges(file,*it);
			it->domains->readData(file);
			it->encoder->initialize(IRoot::Decode,*it);
		}
//	read all the requested phases of all jobs (phase-sequentially)
	for (int phase=phaseBegin; phase<phaseEnd; ++phase)
		for (JobIterator it=jobs.begin(); it!=jobs.end(); ++it) 
			it->encoder->readData(file,phase);
		
}
