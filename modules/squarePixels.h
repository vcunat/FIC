#ifndef SQUAREPIXELS_HEADER_
#define SQUAREPIXELS_HEADER_

#include "../interfaces.h"
#include "../fileUtil.h"

/// \ingroup modules
/** A simple shape transformer using square pixels. It allows to choose modules of types
 *	ISquareRanges, ISquareDomains and ISquareEncoder for subsequent coding.
 *	It can split the planes	into rectangles - max.\ size is a parameter. */
class MSquarePixels: public IShapeTransformer {
	DECLARE_debugModule;

	DECLARE_TypeInfo( MSquarePixels, "Square-pixel"
	, "Continues to work with square-shaped pixels, can divide the image"
	, {
		label:	"Max. part size",
		desc:	"The maximum number of pixels that can be in one part\n"
				"(parts are encoded separately, possibly in different threads)",
		type:	settingInt(12,20,24,IntLog2)
	}, {
		label:	"Range divider",
		desc:	"The module that decides the way a single color-plane part"
				" is split into into Range blocks",
		type:	settingModule<ISquareRanges>()
	}, {
		label:	"Domain pool creator",
		desc:	"The module that decides which block will be used for Domains",
		type:	settingModule<ISquareDomains>()
	}, {
		label:	"Encoder",
		desc:	"The module that will find the best Domain-Range mappings",
		type:	settingModule<ISquareEncoder>()
	} )

protected:
	/** Indices for settings */
	enum Settings { MaxPartSize, ModuleRanges, ModuleDomains, ModuleEncoder };
//	Settings-retrieval methods
	int& maxPartSize()
		{ return settingsInt(MaxPartSize); }
	ISquareRanges* moduleRanges()
		{ return debugCast<ISquareRanges*>(settings[ModuleRanges].m); }
	ISquareDomains* moduleDomains()
		{ return debugCast<ISquareDomains*>(settings[ModuleDomains].m); }
	ISquareEncoder* moduleEncoder()
		{ return debugCast<ISquareEncoder*>(settings[ModuleEncoder].m); }

	typedef IColorTransformer::Plane Plane;
	typedef MTypes::PlaneBlock PlaneBlock;

	typedef std::vector<PlaneBlock>::iterator JobIterator;

protected:
//	Module's data
	std::vector<PlaneBlock> jobs; ///< Encoding jobs - one part of one color plane makes one job
	DEBUG_ONLY( PlaneList planeList; ) //< needed for creating debug info

protected:
//	Construction and destruction
	/** Only frees #jobs */
	~MSquarePixels() {
	//	can't free the whole jobs, because the pixels are NOT owned (can be only a part)
		for (JobIterator it=jobs.begin(); it!=jobs.end(); ++it) {
			it->free(false);
			delete it->ranges;
			delete it->domains;
			delete it->encoder;
		}
	}
public:
/**	\name IShapeTransformer interface
 *	@{ */
	int createJobs(const PlaneList &planes);

	int jobCount() {
		return jobs.size();
	}
	
	void jobEncode(int jobIndex) {
		ASSERT( jobIndex>=0 && jobIndex<jobCount() );
		PlaneBlock &job= jobs[jobIndex];
		job.encoder->initialize( IRoot::Encode, job );
		job.ranges->encode(job);
	}
	void jobDecodeAct( int jobIndex, DecodeAct action, int count=1 ) {
		ASSERT( jobIndex>=0 && jobIndex<jobCount() );
		jobs[jobIndex].encoder->decodeAct(action,count);
	}

	void writeSettings(std::ostream &file);
	void readSettings(std::istream &file);

	int phaseCount() {
		ASSERT( moduleRanges() && moduleDomains() && moduleEncoder() );
		return moduleEncoder()->phaseCount();
	}
	void writeJobs(std::ostream &file,int phaseBegin,int phaseEnd);
	void readJobs(std::istream &file,int phaseBegin,int phaseEnd);
///	@}
};

#endif // SQUAREPIXELS_HEADER_
