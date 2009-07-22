#ifndef INTERFACES_HEADER_
#define INTERFACES_HEADER_

/* Forwards for interfaces */
struct IRoot;
struct IQuality2SE;
struct IColorTransformer;
struct IShapeTransformer;
struct ISquareEncoder;
struct ISquareRanges;
struct ISquareDomains;
struct IStdEncPredictor;
struct IIntCodec;

/** Contains basic types frequently used in modules */
namespace MTypes {
	typedef double Real; ///< The floating-point type in which most computations are made
	typedef float SReal; ///< The floating-point type for long-term pixel-value storage
	
	typedef MatrixSlice<SReal> SMatrix;			///< Used for storing pixel matrices
	typedef SMatrix::Const CSMatrix;			///< Used for passing constant pixels
	typedef std::vector<SMatrix> MatrixList;	///< A list of pixel matrices

	enum DecodeAct { Clear, Iterate };			///< Possible decoding actions \todo name clash, etc.

	struct PlaneBlock; // declared and described later in the file
	
	typedef SummedMatrix<Real,SReal> SummedPixels;
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

	static const Uint16 Magic= 65535-4063 /// magic number - integer identifying FIC files
	, SettingsMagic= Magic^12345; ///< magic number - integer identifying settings files

	/** A status-query method */
	virtual Mode getMode() =0;
	/** Saves current decoding state into a QImage */
	virtual QImage toImage() =0;

	/** Encodes an image - returns false on exception, getMode() have to be to be Clear */
	virtual bool encode
		( const QImage &toEncode, const UpdateInfo &updateInfo=UpdateInfo::none ) =0;
	/** Performs a decoding action (e.g.\ clearing, multiple iteration) */
	virtual void decodeAct( DecodeAct action, int count=1 ) =0;

	/** Saves an encoded image into a stream, returns true on success */
	virtual bool toStream(std::ostream &file) =0;
	/** Loads image from a file - returns true on success (to be run on in Clear state),
	 *	can load in bigger size: dimension == orig_dim*2^\p zoom */
	virtual bool fromStream(std::istream &file,int zoom=0) =0;

	/** Shorthand: saves an encoded image to a file - returns the number of bytes written
	 *	or \p false on failure */
	int toFile(const char *fileName) { 
		std::ofstream file( fileName, ios_base::binary|ios_base::trunc|ios_base::out );
		if ( !toStream(file) )
			return 0;
		else
			return file.tellp();
	}
	/** Shorthand: loads image from a file - returns true on success, see ::fromStream */
	bool fromFile(const char *fileName,int zoom=0) {
		std::ifstream file( fileName, ios_base::binary|ios_base::in );
		return fromStream(file,zoom);
	}
	
	/** Saves all settings to a file (incl.\ child modules), returns true on success */
	bool allSettingsToFile(const char *fileName);
	/** Loads all settings from a file (to be run on a shallow module), returns true on success */
	bool allSettingsFromFile(const char *fileName);
}; // IRoot interface




/** Interface for modules deciding how the max.\ SE will depend on block size
 *	(parametrized by quality) */
struct IQuality2SE: public Interface<IQuality2SE> {
	/** Returns maximum SE for a [0,1] quality and range blocks with pixelCount pixels */
	virtual float rangeSE(float quality,int pixelCount) =0;
	/** Fills an array (until levelEnd) with SE's according to [0,1] quality
	 *	: on i-th index for "complete square range blocks" of level i.
	 *	Common implementation - uses ::rangeSE method */
	void regularRangeErrors(float quality,int levelEnd,float *squareErrors);
};




/** Interface for modules performing color transformations,
 *	following modules always work with single-color (greyscale) images */
struct IColorTransformer: public Interface<IColorTransformer> {
	/** Contains some setings for one color plane of an image */
	struct PlaneSettings;
	
	/** Represents a one-color image, together with some setttings.
	 *	In returned instances the pointed-to memory is always owned by this module. */
	struct Plane {
		mutable SMatrix pixels;	///< a matrix of pixels with values in [0,1]
		const PlaneSettings *settings; ///< the settings for the plane
	};
	
	/** List of planes (the pointed-to memory is owned by the module) */
	typedef std::vector<Plane> PlaneList;

	/** Splits an image into color-planes and adjusts their settings (from \p prototype).
	 *	It should call UpdateInfo::incMaxProgress with the total pixel count. */
	virtual PlaneList image2planes(const QImage &toEncode,const PlaneSettings &prototype) =0;
	/** Merges planes back into a color image (only useful when decoding) */
	virtual QImage planes2image() =0;

	/** Writes any data needed for plane reconstruction to a stream */
	virtual void writeData(std::ostream &file) =0;
	/** Reads any data needed for plane reconstruction from a stream and creates the plane list */
	virtual PlaneList readData( std::istream &file, const PlaneSettings &prototype ) =0;
}; // IColorTransformer interface

struct IColorTransformer::PlaneSettings {
	int width			///  the width of the image (zoomed)
	, height			///  the height of the image (zoomed)
	, domainCountLog2	///  2-logarithm of the maximum domain count
	, zoom;				///< the zoom (dimensions multiplied by 2^zoom)
	SReal quality;		///< encoding quality for the plane, in [0,1] (higher is better)
	IQuality2SE *moduleQ2SE;		///< pointer to the module computing maximum SE (never owned)
	const UpdateInfo &updateInfo;	///< structure for communication with user
	
	/** A simple constructor, only initializes the values from the parameters */
	PlaneSettings( int width_, int height_, int domainCountLog2_, int zoom_
	, SReal quality_=numeric_limits<SReal>::quiet_NaN()
	, IQuality2SE *moduleQ2SE_=0, const UpdateInfo &updateInfo_=UpdateInfo::none )
		: width(width_), height(height_)
		, domainCountLog2(domainCountLog2_), zoom(zoom_)
		, quality(quality_), moduleQ2SE(moduleQ2SE_), updateInfo(updateInfo_) {}
}; // PlaneSettings struct




/** Interface for modules handling pixel-shape changes
 *	and/or splitting the single-color planes into independently processed parts */
struct IShapeTransformer: public Interface<IShapeTransformer> {
	typedef IColorTransformer::PlaneList PlaneList;

	/** Creates jobs from the list of color planes, returns job count */
	virtual int createJobs(const PlaneList &planes) =0;
	/** Returns the number of jobs */
	virtual int jobCount() =0;

	/** Starts encoding a job - thread-safe (for different jobs) */
	virtual void jobEncode(int jobIndex) =0;
	/** Performs a decoding action for a job - thread-safe (for different jobs) */
	virtual void jobDecodeAct( int jobIndex, DecodeAct action, int count=1 ) =0;

	/** Writes all settings (shared by all jobs) needed for later reconstruction */
	virtual void writeSettings(std::ostream &file) =0;
	/** Reads settings (shared by all jobs) needed for reconstruction from a stream -
	 *	needs to be done before the job creation */
	virtual void readSettings(std::istream &file) =0;

	/** Returns the number of phases (progressive encoding), only depends on settings */
	virtual int phaseCount() =0;
	/** Writes any data needed for reconstruction of every job, phase parameters
	 *	determine the range of saved phases */
	virtual void writeJobs(std::ostream &file,int phaseBegin,int phaseEnd) =0;
	/** Reads all data needed for reconstruction of every job and prepares for encoding,
	 *	parameters like ::writeJobs */
	virtual void readJobs(std::istream &file,int phaseBegin,int phaseEnd) =0;
	
	/** Shortcut - default parameters "phaseBegin=0,phaseEnd=phaseCount()" */
	void writeJobs(std::ostream &file,int phaseBegin=0)
		{ writeJobs( file, phaseBegin, phaseCount() ); }
	/** Shortcut - default parameters "phaseBegin=0,phaseEnd=phaseCount()" */
	void readJobs(std::istream &file,int phaseBegin=0)
		{ readJobs( file, phaseBegin, phaseCount() ); }
}; // IShapeTransformer interface


/// @}


namespace MTypes {
	/** Represents a rectangular piece of single-color image to be encoded/decoded.
	 *	It is common structure for ISquare{Range,Domain,Encoder}.
	 *	Inherits a pixel-matrix, its dimensions and block-summers and adds
	 *	pointers to settings and modules handling square ranges, domains and encoding */
	struct PlaneBlock: public SummedPixels {
		typedef IColorTransformer::PlaneSettings PlaneSettings;
		
		const PlaneSettings *settings; ///< the settings for the plane
		ISquareRanges  *ranges; ///< module for range blocks generation
		ISquareDomains *domains;///< module for domain blocks generation
		ISquareEncoder *encoder;///< module for encoding (maintaining domain-range mappings)
	
		/** A simple integrity test - needs nonzero modules and pixel-matrix */
		bool isReady() const
			{ return this && ranges && domains && encoder && pixels.isValid(); }
	}; // PlaneBlock struct
}


/** \addtogroup interfaces
 *	@{ */


/** Interface for modules that control how the image
 * will be split into rectangular (mostly square) range blocks */
struct ISquareRanges: public Interface<ISquareRanges> {
	/** Structure representing a rectangular range block (usually square) */
	struct RangeNode;
	/** List of range nodes (pointers) */
	typedef std::vector<RangeNode*> RangeList;

	/** Starts encoding, calls modules in the passed structure.
	 *	It should update UpdateInfo continually by the count of encoded pixels.
	 *	\throws std::exception when cancelled via UpdateInfo or on other errors */
	virtual void encode(const PlaneBlock &toEncode) =0;
	/** Returns a reference to the current range-block list */
	virtual const RangeList& getRangeList() const =0;

	/** Write all settings needed for reconstruction */
	virtual void writeSettings(std::ostream &file) =0;
	/** Read all settings needed for reconstruction */
	virtual void readSettings(std::istream &file) =0;

	/** Writes data needed for reconstruction (except for settings) */
	virtual void writeData(std::ostream &file) =0;
	/** Reads data from a stream and reconstructs the positions of range blocks.
	 *	\param file	 the stream to read from
	 *	\param block contains the properties of the block to be reconstructed */
	virtual void readData_buildRanges( std::istream &file, const PlaneBlock &block ) =0;
}; // ISquareRanges interface

struct ISquareRanges::RangeNode: public Block {
	/** A common base type for data stored by encoders */
	struct EncoderData {
		float bestSE; ///< the best square error found until now
	};

	/// Encoders can store their data here, deleted in ::~RangeNode
	mutable EncoderData *encoderData;
	/// The smallest integer such that the block fits into square with side of length 2^level
	int level;

	/** Checks whether the block has regular shape (dimensions equal to 2^level) */
	bool isRegular() const 
		{ return width()==powers[level] && height()==powers[level]; }
protected:
	/** Constructor - initializes ::encoderData to zero, to be used by derived classes */
	RangeNode(const Block &block,int level_)
	: Block(block), encoderData(0), level(level_) {}
	
	/** Only deletes ::encoderData */
	~RangeNode()
		{ delete encoderData; }
}; // ISquareRanges::RangeNode struct




/** Interface for modules deciding what will square domain blocks look like */
struct ISquareDomains: public Interface<ISquareDomains> {
	/** Describes one domain pool */
	struct Pool;
	/** List of pools */
	typedef std::vector<Pool> PoolList;

	/** Initializes the pools for given PlaneBlock, assumes settings are OK */
	virtual void initPools(const PlaneBlock &planeBlock) =0;
	/** Prepares domains in already initialized pools (and invalidates summers, etc.\ ) */
	virtual void fillPixelsInPools(PlaneBlock &planeBlock) =0;

	/** Returns a reference to internal list of domain pools */
	virtual const PoolList& getPools() const =0;
	/** Gets densities for all domain pools on a particular level (with size 2^level - zoomed),
	 *	returns unzoomed densities */
	virtual std::vector<short> getLevelDensities(int level,int stdDomCountLog2) =0;

	/** Writes all settings (data needed for reconstruction that don't depend on the input) */
	virtual void writeSettings(std::ostream &file) =0;
	/** Reads all settings (like ::writeSettings) */
	virtual void readSettings(std::istream &file) =0;

	/** Writes all input-dependent data */
	virtual void writeData(std::ostream &file) =0;
	/** Reads all data, assumes the settings have already been read */
	virtual void readData(std::istream &file) =0;
}; // ISquareDomains interface

struct ISquareDomains::Pool: public SummedPixels {
	char type			///  The pool-type identifier (like diamond, module-specific)
	, level;			///< The count of down-scaling steps (1 for basic domains)
	float contrFactor;	///< The contractive factor (0,1) - the quotient of areas
	
	/** Constructor allocating the parent SummedPixels with correct dimensions 
	 *	(increased according to \p zoom) */
	Pool(short width_,short height_,char type_,char level_,float cFactor,short zoom)
	: type(type_), level(level_), contrFactor(cFactor)
		{ setSize( lShift(width_,zoom), lShift(height_,zoom) ); }
};




/** Interface for square encoders - maintaining mappings from square domains to square ranges */
struct ISquareEncoder: public Interface<ISquareEncoder> {
	typedef IColorTransformer::Plane Plane;
	typedef ISquareRanges::RangeNode RangeNode;

	/** Used by encoders, represents information about a domain pool on a level */
	struct LevelPoolInfo {
		int indexBegin	///  the beginning of domain indices in "this pool" on "this level"
		, density;		///< the domain density (step size) in "this pool" on "this level"
	};
	/** [level][pool] -> LevelPoolInfo (the levels are zoomed). For every used level
	 *	contains for all domain pools precomputed densities and the domain-ID boundaries */
	typedef std::vector< std::vector<LevelPoolInfo> > LevelPoolInfos;

	/** Initializes the module for encoding or decoding of a PlaneBlock */
	virtual void initialize( IRoot::Mode mode, PlaneBlock &planeBlock ) =0;
	/** Finds mapping with the best square error for a range (returns the SE),
	 *	data neccessary for decoding are stored in RangeNode.encoderData */
	virtual float findBestSE(const RangeNode &range,bool allowHigherSE=false) =0;
	/** Finishes encoding - to be ready for saving or decoding (can do some cleanup) */
	virtual void finishEncoding() =0;
	/** Performs a decoding action */
	virtual void decodeAct( DecodeAct action, int count=1 ) =0;

	/** Write all settings needed for reconstruction (don't depend on encoded thing) */
	virtual void writeSettings(std::ostream &file) =0;
	/** Read all settings, opposite to ::writeSettings */
	virtual void readSettings(std::istream &file) =0;

	/** Returns the number of phases (progressive encoding), only depends on settings */
	virtual int phaseCount() const =0;
	/** Write one phase of  data needed for reconstruction (excluding settings) */
	virtual void writeData(std::ostream &file,int phase) =0;
	/** Reads one phase of data needed for recostruction,
	 *	assumes the settings have been read previously via ::readSettings */
	virtual void readData(std::istream &file,int phase) =0;
}; // ISquareEncoder interface




/** Interface for domain-range mapping predictor generator for MStdEncoder */
struct IStdEncPredictor: public Interface<IStdEncPredictor> {
	/** Contains information about one predicted domain block */
	struct Prediction {
		explicit Prediction( int domainID_=-1, char rotation_=-1 )
		: domainID(domainID_), rotation(rotation_) {}

		int domainID;	///< domain's identification number
		char rotation;	///< the rotation of the domain
	};
	/** List of predictions (later often reffered to as a chunk) */
	typedef std::vector<Prediction> Predictions;

	/** %Interface for objects that predict domains for a concrete range block */
	struct IOneRangePredictor {
		/** Virtual destructor needed for safe deletion of derived classes */
		virtual ~IOneRangePredictor() {}
		/** Makes several predictions at once, returns \p store reference */
		virtual Predictions& getChunk(float maxPredictedSE,Predictions &store) =0;
	};

	/** Holds plenty precomputed information about the range block to be predicted for */
	struct NewPredictorData;

	/** Creates a predictor (passing the ownership) for a range block */
	virtual IOneRangePredictor* newPredictor(const NewPredictorData &data) =0;
	/** Releases common resources (to be called when encoding is complete) */
	virtual void cleanUp() =0;
}; // IStdEncPredictor interface

struct IStdEncPredictor::NewPredictorData {
	const ISquareRanges::RangeNode *rangeBlock;	///< Pointer to the range block
	const SummedPixels *rangePixels;			///< Pointer to range's pixels
	const ISquareDomains::PoolList *pools;		///< Pointer to the domain pools
	const ISquareEncoder::LevelPoolInfos::value_type *poolInfos;
		///< Pointer to LevelPoolInfos for all pools (for this level)

	bool allowRotations	/// Are rotations allowed?
	, quantError		///	Should quantization errors be taken into account?
	, allowInversion	/// Are mappings with negative linear coefficients allowed?
	, isRegular;		///< Is this range block regular? (see RangeNode::isRegular)

	Real maxLinCoeff2	/// The maximum linear coefficient squared (or <0 if none)
	, bigScaleCoeff;	///< The coefficient of big-scaling penalization

	Real rSum	/// The sum of range block's all pixels
	, r2Sum		/// The sum of squares of range block's pixels
	, pixCount	///	The number of range block's pixels
	, rnDev2	/// Precomputed = ( ::pixCount*::r2Sum - sqr(::rSum) )
	, rnDev		/// Precomputed = sqrt(::rnDev2)
	, qrAvg		///	The average of range's pixels rounded by the selected quantizer
	, qrDev		/// ::rnDev rounded by the selected quantizer
	, qrDev2; 	///< sqr(::qrDev)
	#ifndef NDEBUG
	NewPredictorData()
	: rangeBlock(0), rangePixels(), pools(0), poolInfos(0) {}
	#endif
}; // NewPredictorData struct




/** Integer sequences (de)coder interface */
struct IIntCodec: public Interface<IIntCodec> {
	/** Sets the number of possible symbols to work with from now on data: [0,possib-1] */
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

#endif // INTERFACES_HEADER_
