#ifndef INTERFACES_HEADER_
#define INTERFACES_HEADER_

namespace MTypes {
	/** Structure representing a rectangle */
	struct Block {
		short x0, y0, xend, yend;
		
		int width()	 const	{ return xend-x0; }
		int height() const	{ return yend-y0; }
		int size()	 const	{ return width()*height(); }

		Block() {}
		Block(short x0_,short y0_,short xend_,short yend_)
		: x0(x0_), y0(y0_), xend(xend_), yend(yend_) {}
	};
}

#include "modules.h"
#include "matrixUtil.h"


/** Forwards for interfaces */
struct IRoot;
struct IColorTransformer;
struct IShapeTransformer;
struct IQuality2SquareError;
struct ISquareEncoder;
struct ISquareRanges;
struct ISquareDomains;
struct IStdEncPredictor;
struct IIntCodec;

/** Contains basic types frequently used in modules */
namespace MTypes {
	typedef double Real; ///< The floating-point type in which computations are made
	typedef MatrixSummer<Real,float,short> BlockSummer; ///< Summer instanciation used for pixels
	typedef std::vector<float**> MatrixList;

	enum DecodeAct { Clear, Iterate }; ///< Possible decoding actions
	
	struct PlaneBlock;
}
namespace NOSPACE {
	using namespace std;
	using namespace MTypes;
}

/** \defgroup interfaces Interfaces for coding modules
 *	@{ */

/** Interface for root modules (with only one implementation MRoot),
 *	all modules are always owned (either directly or indirectly) by one of this type */
struct IRoot: public Interface<IRoot> {
	/** The root can be in any of these modes */
	enum Mode { 
		Clear,	///< no action has been done
		Encode,	///< contains an encoded image
		Decode	///< contains an image decoded from a file
	};

	/** Magic number - 16-bit integer identifying FIC files */
	static const Uint16 Magic= 65535-4063;

	/** A status-query method */
	virtual Mode getMode() =0;
	/** Saves current decoding state into a QImage */
	virtual QImage toImage() =0;

	/** Encodes an image - returns false on exception */
	virtual bool encode(const QImage &toEncode) =0;
	/** Performs a decoding action (e.g.\ clearing, multiple iteration) */
	virtual void decodeAct( DecodeAct action, int count=1 ) =0;

	/** Saves an encoded image to a file - returns true on success */
	virtual bool toFile(const char *fileName) =0;
	/** Loads image from a file - returns true on success (to be run on a shallow module) */
	virtual bool fromFile(const char *fileName) =0;
};

/** Interface for modules performing color transformations,
 *	following modules always work with single-color (greyscale) images */
struct IColorTransformer: public Interface<IColorTransformer> {
	/** Describes a rectangular single-color image, includes some compression setings */
	struct Plane {
		typedef IQuality2SquareError ModuleQ2SE;

		static const Plane Empty; ///< an empty instance

		float **pixels	/// a Matrix of pixels with values in [0,1], not owned (shared)
		, quality;		///< encoding quality for the plane, in [0,1] (higher is better)
		int domainCountLog2;	///< 2-logarithm of the maximum domain count
		ModuleQ2SE *moduleQ2SE;	///< pointer to the module computing maximum SE (not owned)
		
		/** A simple constructor, only initializes the values from the parameters */
		Plane(float **pixels_,float quality_,int domainCountLog2_,ModuleQ2SE *moduleQ2SE_)
		: pixels(pixels_), quality(quality_), domainCountLog2(domainCountLog2_)
		, moduleQ2SE(moduleQ2SE_) {}
	};
	/** List of planes */
	typedef std::vector<Plane> PlaneList;

	/** Splits an image into color-planes and adjusts their quality and max.\ domain count */
	virtual PlaneList image2planes(const QImage &toEncode,const Plane &prototype) =0;
	/** Merges planes back into a color image */
	virtual QImage planes2image(const MatrixList &pixels,int width,int height) =0;

	/** Writes any data needed for plane reconstruction to a stream */
	virtual void writeData(std::ostream &file) =0;
	/** Reads any data needed for plane reconstruction from a stream and creates the plane list */
	virtual PlaneList readData(std::istream &file,const Plane &prototype,int width,int height) =0;
};

/** Interface for modules handling pixel-shape changes
 *	and/or splitting the single-color planes into independently processed parts */
struct IShapeTransformer: public Interface<IShapeTransformer> {
	typedef IColorTransformer::PlaneList PlaneList;

	/** Creates jobs from the list of color planes (takes the ownership of pixels and returns job count) */
	virtual int createJobs( const PlaneList &planes, int width, int height ) =0;
	/** Returns the number of jobs */
	virtual int jobCount() =0;
	/** Creates a plane list referencing current decoding state */
	virtual MatrixList collectJobs() =0;

	/** Starts encoding a job - thread-safe (for different jobs) */
	virtual void jobEncode(int jobIndex) =0;
	/** Performs a decoding action for a job - thread-safe (for different jobs) */
	virtual void jobDecodeAct( int jobIndex, DecodeAct action, int count=1 ) =0;

	/** Writes all settings (shared by all jobs) needed for later reconstruction */
	virtual void writeSettings(std::ostream &file) =0;
	/** Reads settings (shared by all jobs) needed for reconstruction from a stream
	 *	- needs to be done before the job creation */
	virtual void readSettings(std::istream &file) =0;

	/** Returns the number of phases (progressive encoding), only depends on settings */
	virtual int phaseCount() =0;
	/** Writes any data needed for reconstruction of every job, phase parameters
	 *	determine the interval of saved phases */
	virtual void writeJobs(std::ostream &file,int phaseBegin,int phaseEnd) =0;
	/** Reads all data needed for reconstruction of every job and prepares for encoding,
	 *	parameters like #writeJobs */
	virtual void readJobs (std::istream &file,int phaseBegin,int phaseEnd) =0;
	/** Shortcut - default parametres "phaseBegin=0,phaseEnd=phaseCount()" */
	void writeJobs(std::ostream &file,int phaseBegin=0)
		{ writeJobs( file, phaseBegin, phaseCount() ); }
	/** Shortcut - default parametres "phaseBegin=0,phaseEnd=phaseCount()" */
	void readJobs (std::istream &file,int phaseBegin=0)
		{ readJobs( file, phaseBegin, phaseCount() ); }
};

/** Interface for modules deciding how the max.\ SE will depend on block size
 *	(parametrized by quality) */
struct IQuality2SquareError: public Interface<IQuality2SquareError> {
	/** Returns maximum SE for a [0,1] quality and range blocks with pixelCount pixels */
	virtual float rangeSE(float quality,int pixelCount) =0;
	/** Fills an array (until levelEnd) with SE's according to [0,1] quality
	 *	: on i-th index for "complete square range blocks" of level i */
	void completeSquareRangeErrors(float quality,int levelEnd,float *squareErrors);
};

/** Interface for modules that control how the image
 * will be split into (mostly) square range blocks */
struct ISquareRanges: public Interface<ISquareRanges> {
	/** Structure representing a rectangular range block (usually square) */
	struct RangeNode: public Block {
		/** A common base for data stored by encoders */
		struct EncoderData {
			float bestSE;
		};

		/** Encoders can store their data here, !not deleted! (use BulkAllocator) */
		mutable EncoderData *encoderData;
		/** The level of the block, dimensions of the block are usually equal to 2^level */
		int level;

		/** Checks whether the block has regular shape (dimensions equal to 2^level) */
		bool isRegular() const
			{ return width()==powers[(int)level] && height()==powers[(int)level]; }
	protected:
		/** Constructor - initializes #encoderData to zero, to be used by derived classes */
		RangeNode(const Block &block,int level_)
		: Block(block), encoderData(0), level(level_) {}
	}; // RangeNode struct
	typedef std::vector<RangeNode*> RangeList;

	/** Starts encoding, calls modules in the passed structure */
	virtual void encode(const PlaneBlock &toEncode) =0;
	/** Returns a reference to the current range-block list */
	virtual const RangeList& getRangeList() const =0;

	/** Write all settings needed for reconstruction */
	virtual void writeSettings(std::ostream &file) =0;
	/** Read all settings needed for reconstruction */
	virtual void readSettings(std::istream &file) =0;

	/** Writes data needed for reconstruction (except for settings) */
	virtual void writeData(std::ostream &file) =0;
	/** Reads data from a stream and reconstructs range blocks.
	 *	\param file	 the stream to read from
	 *	\param block the position and size of the block in the plane to be reconstructed */
	virtual void readData_buildRanges( std::istream &file, const Block &block ) =0;
};

/** Interface for modules deciding what will square domain blocks look like */
struct ISquareDomains: public Interface<ISquareDomains> {
	/** Describes one domain pool */
	struct Pool {
		short width	///		The width of the pool
		, height; ///<		The height of the pool
		float **pixels; ///<The matrix of pixels (of the pool)
		char type ///		The pool-type identifier (like diamond, module-specific)
		, level; ///<		The count of down-scaling steps (1 for basic domains)
		float contrFactor; ///< The contractive factor (0,1) - square root of quotient of areas
		BlockSummer summers[2]; ///< Domain summers for quick computations

	public:
		/** Constructor allocating a matrix with correct dimensions */
		Pool(short width_,short height_,char type_,char level_,float cFactor)
		: width(width_), height(height_), pixels(newMatrix<float>(width_,height_))
		, type(type_), level(level_), contrFactor(cFactor) {}
		/** Only deletes #pixels matrix */
		void free()
			{ delMatrix(pixels); }
			
		/** Makes both summers ready for work */
		void summers_makeValid() {
			for (int i=0; i<2; ++i)
				summers[i].fill( pixels, BlockSummer::SumType(i), width, height );
		}
		/** A shortcut for getting both sums for a block */
		void getSums( const Block &block
		, BlockSummer::result_type &sum, BlockSummer::result_type &sum2 ) const	{
			sum=	summers[0].getSum(block.x0,block.y0,block.xend,block.yend);
			sum2=	summers[1].getSum(block.x0,block.y0,block.xend,block.yend);
		}
	};
	/** List of pools */
	typedef std::vector<Pool> PoolList;

	/** Initializes the pools for given dimensions */
	virtual void initPools(int width,int height) =0;
	/** Prepares domains in already initialized pools (and fills summers, etc.) */
	virtual void fillPixelsInPools(const PlaneBlock &ranges) =0;

	/** Returns a reference to internal list of domain pools */
	virtual const PoolList& getPools() const =0;
	/** Gets densities for all domain pools on a particular level (with size 2^level) */
	virtual std::vector<short> getLevelDensities(int level,int stdDomCountLog2) =0;

	/** Writes all data needed for reconstruction that don't depend on the input (=settings) */
	virtual void writeSettings(std::ostream &file) =0;
	/** Reads all settings (like #writeSettings) */
	virtual void readSettings(std::istream &file) =0;

	/** Writes all input-dependent data */
	virtual void writeData(std::ostream &file) =0;
	/** Reads all data, assumes the settings have already been read */
	virtual void readData(std::istream &file) =0;
};

struct ISquareEncoder: public Interface<ISquareEncoder> {
	typedef IColorTransformer::Plane Plane;
	typedef ISquareRanges::RangeNode RangeNode;

	/** Used by encoders, represents information about a domain pool on a level */
	struct LevelPoolInfo {
		int indexBegin ///	the beginning of domain indices in "this pool" on "this level"
		, density; ///<		the domain density (step size) in "this pool" on "this level"
	};
	typedef std::vector< std::vector<LevelPoolInfo> > LevelPoolInfos;

	/** Initializes the module for encoding or decoding */
	virtual void initialize( IRoot::Mode mode, const PlaneBlock &planeBlock ) =0;
	/** Finds a mapping with the best square error for a range (returns the SE) */
	virtual float findBestSE(const RangeNode &range) =0;
	/** Finishes encoding - to be ready for saving or decoding (can do some cleanup) */
	virtual void finishEncoding() =0;
	/** Performs a decoding action */
	virtual void decodeAct( DecodeAct action, int count=1 ) =0;

	/** Write/read all settings needed for reconstruction (don't depend on encoded thing) */
	virtual void writeSettings(std::ostream &file) =0;
	virtual void readSettings(std::istream &file) =0;

	/** Returns the number of phases (progressive encoding), only depends on settings */
	virtual int phaseCount() const =0;
	/** Write/read all data needed for reconstruction (do depend...) */
	virtual void writeData(std::ostream &file,int phase) =0;
	virtual void readData(std::istream &file,int phase) =0;
};

struct IStdEncPredictor: public Interface<IStdEncPredictor> {
	/** Contains information about one predicted domain block */
	struct Prediction {
		explicit Prediction( int domainID_=-1, char rotation_=-1 )
		: domainID(domainID_), rotation(rotation_) {}

		int domainID;
		char rotation; ///<	the rotation of the domain
	};
	typedef std::vector<Prediction> Predictions;

	/** %Interface for object that make predictions for a concrete range block */
	class OneRangePred {
	public:
		virtual ~OneRangePred() {}
		/** Makes several predictions at once. */
		virtual Predictions& getChunk(float maxPredictedSE,Predictions &store) =0;
	};

	/** Holds information neccesary to create a new predictor */
	struct NewPredictorData {
		const ISquareRanges::RangeNode *rangeBlock;
		const float **rangePixels;
		const ISquareDomains::PoolList *pools;
		const ISquareEncoder::LevelPoolInfos::value_type *poolInfos;

		bool allowRotations;
		bool quantError, allowInversion, isRegular;
		float maxLinCoeff2, bigScaleCoeff;

		Real rSum, r2Sum, pixCount
		, rAvg, rDev2; // quant-rounded values
		#ifndef NDEBUG
		NewPredictorData()
		: rangeBlock(0), rangePixels(0), pools(0), poolInfos(0) {}
		#endif
	};

	/** Creates a predictor (passing the ownership) for a range block */
	virtual OneRangePred* newPredictor(const NewPredictorData &data) =0;
	/** Releases the resources (called when encoding is complete) */
	virtual void cleanUp() =0;
};

/** Integer sequences (de)coder interface */
struct IIntCodec: public Interface<IIntCodec> {
	/** Sets the number of possible symbols to work with from now on data: [0..possib-1] */
	virtual void setPossibilities(int possib) =0;
	/** Codes data and sends them into a stream */
	virtual void encode(std::vector<int> &data,std::ostream &file) =0;
	/** Reads \c count symbols from \c file, decodes them and fills in \c data */
	virtual void decode(std::istream &file,int count,std::vector<int> &data) =0;

	/** Write all settings needed (doesn't include possibilities set) */
	virtual void writeSettings(std::ostream &file) =0;
	/** Read all settings needed (doesn't include possibilities set) */
	virtual void readSettings(std::istream &file) =0;
};

///	@} - interfaces defgroup
namespace MTypes {
	/** Represents a rectangle in a color plane (using square pixels) */
	struct PlaneBlock: public ISquareEncoder::Plane, public Block {
		BlockSummer summers[2];	///< normal and squares summers for the pixels
		ISquareRanges  *ranges;	///< module for range blocks generation
		ISquareDomains *domains;///< module for domain blocks generation
		ISquareEncoder *encoder;///< module for encoding (maintaining range-domain mappings)

		/** Simple constructor, just initializes members with parameters and checks for zeroes */
		PlaneBlock( const Plane &plane, const Block &range
		, ISquareRanges *ranges_, ISquareDomains *domains_, ISquareEncoder *encoder_ )
			: Plane(plane), Block(range), ranges(ranges_), domains(domains_), encoder(encoder_)
				{ assert( ranges && domains && encoder ); }
		/** Just deletes the modules (the destructor is empty,
		 *	the method has to be called explicitly if needed) */
		void free() {
			delete ranges;
			delete domains;
			delete encoder;
		}
		
		/** A simple integrity test - needs nonzero modules and pixel-matrix */
		bool isReady() const
			{ return ranges && domains && encoder && pixels && moduleQ2SE; }
		/** Just validates both summers (if needed) */
		void summers_makeValid() const {
			for (int i=0; i<2; ++i)
				if ( !summers[i].isValid() )
					constCast(summers[i]).fill
					( pixels, (BlockSummer::SumType)i, width(), height(), x0, y0 );
		}
		/** Justs invalidates both summers (to be called after changes in the pixel-matrix) */
		void summers_invalidate() const {
			constCast(summers[0]).invalidate();
			constCast(summers[1]).invalidate();
		}
		/** A shortcut for getting sums from summers relative to the block's coordinates */
		BlockSummer::result_type getSum( const Block &block, BlockSummer::SumType type ) const {
			return summers[type].getSum( block.x0-x0, block.y0-y0, block.xend-x0, block.yend-y0 );
		}
	};// PlaneBlock struct
}


#endif // INTERFACES_HEADER_
