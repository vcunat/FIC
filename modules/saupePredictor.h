#ifndef SAUPEPREDICTOR_HEADER_
#define SAUPEPREDICTOR_HEADER_

#include "../interfaces.h"
namespace NOSPACE {
	#include "../kdTree.h"
}

/** Predictor for MStandardEncoder based on a theorem proven in Saupe's work */
class MSaupePredictor: public IStdEncPredictor {

	DECLARE_M_cloning_name_desc( MSaupePredictor, "Saupe predictor"
	, "Predictor for standard encoder using multi-dimensional nearest neighbour search" )

	DECLARE_M_settings_type({
		type:	Int,
		data: {	i:{1,32} },
		label:	"Prediction chunk size",
		desc:	"The number of predicted domains in a chunk"
	})

	DECLARE_M_settings_default(
		8 //	chunk size
	)
private:
	/** Indices for settings */
	enum Settings { ChunkSize };

public:
	typedef float KDReal;		///< The floating point type used in the KD-tree
	typedef KDTree<KDReal> Tree;///< The version of KDTree in use
private:
//	Module's data
	std::vector<Tree*> levelTrees; ///< The predicting Tree for every level (can be missing)

protected:
//	Construction and destruction
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

		std::vector<Tree::PointHeap*> heaps; ///< Pointers to the heaps for every rotation and inversion
		std::vector<HeapInfo> infoHeap; ///< Heap built from #heaps according to their best SEs
		KDReal *points; 	///< Normalized range rotations and inversions used by the heaps
		int chunkSize		///  The suggested count for predicted ranges returned at once
		, heapCount;		///< The number of heaps
		Real errorConvAccel;///< Accelerator for conversion of SEs to the real values = 1/range.rnDev2
		bool firstChunk; 	///< True if nothing has been predicted yet, false otherwise

		/** Computes normalized-tree-error from real SE (slow - uses one sqrt) */
		Real normalizeSE(Real error) const
			//{ return std::ldexp( sqrt(error*errorConvAccel)-1, 1 ); }
			{ return error*errorConvAccel; }

	public:
		/** Creates a new predictor for a range block (prepares tree-heaps, etc.) */
		OneRangePredictor(const NewPredictorData &data,int chunkSize_,const Tree &tree);

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
