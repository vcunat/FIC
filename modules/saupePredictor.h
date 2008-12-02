#ifndef SAUPEPREDICTOR_HEADER_
#define SAUPEPREDICTOR_HEADER_

#include "../interfaces.h"
namespace NOSPACE {
	#include "../kdTree.h"
}

/** Predictor for MStandardEncoder based on a theorem proven in Saupe's work */
class MSaupePredictor: public IStdEncPredictor {
	DECLARE_debugModule;

	DECLARE_TypeInfo( MSaupePredictor, "Saupe predictor"
	, "Predictor for standard encoder using multi-dimensional nearest neighbour search"
	, {
		label:	"Prediction chunk size",
		desc:	"The number of predicted domains in a chunk",
		type:	settingInt(1,8,32)
	}, {
		label:	"Max. predictions (%)",
		desc:	"The maximal percentage of domains predicted for a range block",
		type:	settingFloat(0,5,100)
	} )

private:
	/** Indices for settings */
	enum Settings { ChunkSize, MaxPredPercent };

	/** maxChunkCoeff() * <the number of domains> == <max. number of chunks> */
	Real maxChunkCoeff()
		{ return settings[MaxPredPercent].val.f / Real( 100*settingsInt(ChunkSize) ); }

public:
	typedef float KDReal;		///< The floating point type used in the KD-tree
	typedef KDTree<KDReal> Tree;///< The version of KDTree in use
private:
//	Module's data
	std::vector<Tree*> levelTrees; ///< The predicting Tree for every level (can be missing)
	#ifndef NDEBUG
	long predicted, maxpred;
	#endif

protected:
//	Construction and destruction
	#ifndef NDEBUG
	MSaupePredictor(): predicted(0), maxpred(0) {}
	#endif
	~MSaupePredictor()
		{ cleanUp(); }

public:
/**	\name IStdEncPredictor interface
 *	@{ */
	OneRangePred* newPredictor(const NewPredictorData &data);
	void cleanUp();
///	@}

private:
	/** Builds a new tree for one level of range blocks using passed domain blocks */
	Tree* createTree(const NewPredictorData &data);

private:
	/** Implementation of the one-range-predictor from IStdEncPredictor interface */
	class OneRangePredictor: public OneRangePred {
		/** Struct representing one node of the KD-tree and its distance form the range block */
		struct HeapInfo {
			int index;		///< The index of the node in the tree
			float bestError;///< The SE-distance of the node from the range's point

			/** Only initializes members from the parameters */
			HeapInfo(int index_,float bestError_)
			: index(index_), bestError(bestError_) {}

			/** Reverse comparison operator according to the #bestError (lowest first) */
			bool operator<(const HeapInfo &other) const
				{ return bestError>other.bestError; }
		};

		class SEnormalizator {
			Real errorAccel; /// Accelerator for conversion of SEs to the real values
		public:
			/** Initializes the normalizator for a range block, needed to call #normSE */
			void initialize(const NewPredictorData &data) {
				errorAccel= data.pixCount / data.rnDev2;
			}
			/** Computes normalized-tree-error from real SE (#initialize has been called) */
			Real normSE(Real error) const {
				Real result= std::ldexp( 1-sqrt(1-error*errorAccel), 1 );
				if ( isnan(result) )
					result= numeric_limits<Real>::max();
				return result;
			}
		} normalizator;

		std::vector<Tree::PointHeap*> heaps; ///< Pointers to the heaps for every rotation and inversion
		std::vector<HeapInfo> infoHeap; ///< Heap built from #heaps according to their best SEs
		KDReal *points; 	///< Normalized range rotations and inversions used by the heaps
		int chunkSize		///  The suggested count for predicted ranges returned at once
		, chunksRemain		///  Max. remaining count of chunks to be returned
		, heapCount;		///< The number of heaps
		bool firstChunk 	///  True if nothing has been predicted yet, false otherwise
		, allowRotations;	///< NewPredictorData::allowRotations
	#ifndef NDEBUG
	public: long *predicted;
	#endif

	public:
		/** Creates a new predictor for a range block (prepares tree-heaps, etc.) */
		OneRangePredictor( const NewPredictorData &data, int chunkSize_
		, const Tree &tree, int maxChunks );

	/**	\name OneRangePred interface
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
