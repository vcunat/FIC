#ifndef SQUAREPIXELS_HEADER_
#define SQUAREPIXELS_HEADER_

#include "../interfaces.h"
#include "../fileUtil.h"

class MSquarePixels: public IShapeTransformer {

	DECLARE_M_cloning_name_desc( MSquarePixels, "Square-pixel"
	, "Continues to work with square-shaped pixels, can divide the image" );

	DECLARE_M_settings_type({
		type:	IntLog2,
		data: {	i:{12,24} },
		label:	"Max. part size",
		desc:	"The maximum number of pixels that can be in one part\n"
				"(parts are encoded separately, possibly in different threads)"
	}, {
		type:	ModuleCombo,
		data: {	compatIDs: &ISquareRanges::getCompMods() },
		label:	"Range divider",
		desc:	"The module that decides the way a single color-plane part"
				" is split into into Range blocks"
	}, {
		type:	ModuleCombo,
		data: {	compatIDs: &ISquareDomains::getCompMods() },
		label:	"Domain pool creator",
		desc:	"The module that decides which block will be used for Domains"
	}, {
		type:	ModuleCombo,
		data: {	compatIDs: &ISquareEncoder::getCompMods() },
		label:	"Encoder",
		desc:	"The module that will find the best Domain-Range mappings"
	});

	DECLARE_M_settings_default(
		20,	// Max. part size
		0,	// Range divider
		0,	// Domain pool creator
		0	// Encoder
	);
private:
	enum Settings { MaxPartSize, ModuleRanges, ModuleDomains, ModuleEncoder };
//	Settings-retrieval methods
	int& maxPartSize()
		{ return settings[MaxPartSize].i; }
	ISquareRanges* moduleRanges()
		{ return debugCast<ISquareRanges*>(settings[ModuleRanges].m); }
	ISquareDomains* moduleDomains()
		{ return debugCast<ISquareDomains*>(settings[ModuleDomains].m); }
	ISquareEncoder* moduleEncoder()
		{ return debugCast<ISquareEncoder*>(settings[ModuleEncoder].m); }

	typedef IColorTransformer::Plane Plane;
	typedef MTypes::PlaneBlock Job;

	typedef std::vector<Job>::iterator JobIterator;

private:
//	Module's data
	std::vector<Job> jobs;
	MatrixList ownedMatrices;

private:
	Job makeJob(const Plane &plane,const Block &block) {
		return Job( plane, block, clone(moduleRanges()), clone(moduleDomains())
			, clone(moduleEncoder()) );
	}
protected:
//	Construction and destruction
	~MSquarePixels() {
		for_each( ownedMatrices.begin(), ownedMatrices.end(), delMatrix<float> );
		for_each( jobs.begin(), jobs.end(), mem_fun_ref(&Job::free) );
	}
public:
//	IShapeTransformer interface
	int createJobs( const PlaneList &planes, int width, int height );

	int jobCount() {
		return jobs.size();
	}
	MatrixList collectJobs() {
		assert( !ownedMatrices.empty() );
		return ownedMatrices;
	}

	void jobEncode(int jobIndex) {
		assert( jobIndex>=0 && jobIndex<jobCount() );
		Job &job=jobs[jobIndex];
		job.encoder->initialize( IRoot::Encode, job );
		job.ranges->encode(job);
	}
	void jobDecodeAct( int jobIndex, DecodeAct action, int count=1 ) {
		assert( jobIndex>=0 && jobIndex<jobCount() );
		jobs[jobIndex].encoder->decodeAct(action,count);
	}

	void writeSettings(std::ostream &file);
	void readSettings(std::istream &file);

	int phaseCount() {
		assert( moduleRanges() && moduleDomains() && moduleEncoder() );
		return moduleEncoder()->phaseCount();
	}
	void writeJobs(std::ostream &file,int phaseBegin,int phaseEnd);
	void readJobs(std::istream &file,int phaseBegin,int phaseEnd);
};

#endif // SQUAREPIXELS_HEADER_
