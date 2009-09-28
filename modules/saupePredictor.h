#ifndef SAUPEPREDICTOR_HEADER_
#define SAUPEPREDICTOR_HEADER_

#include "../headers.h"
#include "../kdTree.h"

/// \ingroup modules
/** Predictor for MStdEncoder based on a theorem proven in Saupe's work.
 *	It resizes the blocks to 4x4 and normalizes them.
 *	Domains for every level are stored in a KDTree instance and searched. 
 *	The user can set the size of returned chunks of the blocks
 *	and the maximal part of domains returned. */
class MSaupePredictor: public IStdEncPredictor {
	DECLARE_debugModule;

	DECLARE_TypeInfo( MSaupePredictor, "Saupe predictor"
	, "Predictor for standard encoder using multi-dimensional nearest neighbour search"
	, {
		label:	"Prediction chunk size",
		desc:	"The number of predicted domains in a chunk",
		type:	settingInt(1,16,64)
	}, {
		label:	"Max. predicted part",
		desc:	"The maximal part of domains predicted for a range block",
		type:	settingInt(-20,-8,0,IntLog2)
	} )

protected:
	/** Indices for settings */
	enum Settings { ChunkSize, MaxPredPart };

	/**  maxPredCoeff() * "the number of domains" == "max. number of predictions" */
	Real maxPredCoeff()	{ return ldexp( Real(1), settingsInt(MaxPredPart) ); }

public:
	typedef float KDReal;		///< The floating point type used in the KD-tree
	typedef KDTree<KDReal> Tree;///< The version of KDTree in use
protected:
//	Module's data
	std::vector<Tree*> levelTrees; ///< The predicting Tree for every level (can be missing)
	#ifndef NDEBUG // the stats about the domain counts predicted
	long predicted, maxpred;
	#endif

protected:
//	Construction and destruction
	#ifndef NDEBUG
	MSaupePredictor(): predicted(0), maxpred(0) {}
	#endif
	/** Only call ::cleanUp */
	~MSaupePredictor() { cleanUp(); }

public:
/**	\name IStdEncPredictor interface
 *	@{ */
	IOneRangePredictor* newPredictor(const NewPredictorData &data);
	
	void cleanUp() {
		clearContainer(levelTrees);
		levelTrees.clear();
	}
///	@}

protected:
	/** Computes the level for predictions based on the actual level */
	int getPredLevel(int /*realLevel*/) const 
		{ return 2; }

	/** Builds a new tree for one level of range blocks using passed domain blocks */
	Tree* createTree(const NewPredictorData &data);

	/** Normalizes and possibly shrinks a domain block */
	static void refineDomain( const SummedPixels &pixMatrix, int x0, int y0
		, bool allowInversion, int realLevel, int predLevel, SReal *pixelResult );
	/** Normalizes and possibly shrinks a range block, returns true on success.
	 *	Failure occurs when the shrunken block contains at most one pixel (pointless to predict). */
	static bool refineRange
		( const NewPredictorData &data, int predLevel, SReal *pixelResult );


protected:
	/** Implementation of the one-range-predictor from IStdEncPredictor interface */
	class OneRangePredictor: public IOneRangePredictor {
		friend class MSaupePredictor;
		
		/** Struct representing one node of the KD-tree and its distance form the range block */
		struct HeapInfo {
			int index;		///< The index of the node in the tree
			float bestError;///< The SE-distance of the node from the range's point

			/** Only initializes members from the parameters */
			HeapInfo(int index_,float bestError_)
			: index(index_), bestError(bestError_) {}

			/** Reverse comparison operator according to the ::bestError (lowest first) */
			bool operator<(const HeapInfo &other) const
				{ return bestError>other.bestError; }
		};

		/** A convertor from SE in regular space to SE in normalized space */
		class SEnormalizator {
			Real errorAccel; ///< Accelerator for conversion of SEs to the real values
		public:
			/** Initializes the normalizator for a range block, needed to call ::normSE */
			void initialize(const NewPredictorData &data,int predLevel) {
				int shift= 2 * (predLevel - data.rangeBlock->level);
				errorAccel= ldexp( data.pixCount/data.rnDev2, shift );
				//errorAccel= data.pixCount/data.rnDev2;
			}
			/** Computes normalized-tree-error from real SE (::initialize has been called) */
			Real normSE(Real error) const {
				Real result= std::ldexp( 1-sqrt(1-error*errorAccel), 1 );
				if ( isNaN(result) )
					result= numeric_limits<Real>::infinity();
				return result;
			}
		} errorNorm; ///< used to compute SE in the normalized space (from normal SE)
		
		typedef Tree::PointHeap PointHeap;
	protected:
		std::vector<PointHeap*> heaps;	///< Pointers to the heaps for every rotation and inversion
		std::vector<HeapInfo> infoHeap;	///< Heap built from ::heaps according to their best SEs
		KDReal *points; ///< Normalized range rotations and inversions used by the heaps (owned)
		int chunkSize	///  The suggested count for predicted ranges returned at once
		, predsRemain	///  Max.\ remaining count of predictions to be returned
		, heapCount;	///< The number of heaps
		bool firstChunk ///  True if nothing has been predicted yet, false otherwise
		, allowRotations///  Like NewPredictorData::allowRotations
		, isRegular;	///< Indicates regularity of the range block (see RangeNode::isRegular)
	
	DEBUG_ONLY( public: long *predicted; )

	protected:
		/** Creates a new predictor for a range block (prepares tree-heaps, etc.) */
		OneRangePredictor( const NewPredictorData &data, int chunkSize_
			, const Tree &tree, int maxPredicts );
	public:
	/**	\name IOneRangePredictor interface
	 *	@{ */
		Predictions& getChunk(float maxPredictedSE,Predictions &store);
		~OneRangePredictor() {
			clearContainer(heaps);
			delete[] points;
		}
	///	@}
	}; // OneRangePredictor class
};

#endif // SAUPEPREDICTOR_HEADER_
