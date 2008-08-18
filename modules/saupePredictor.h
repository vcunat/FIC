#ifndef SAUPEPREDICTOR_HEADER_
#define SAUPEPREDICTOR_HEADER_

#include "../interfaces.h"
namespace NOSPACE {
	#include "../kdTree.h"
}

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
		8, //	chunk size
	)
private:
	enum Settings { ChunkSize };
public:
	typedef KDTree<float> Tree;
private:
//	Module's data
	std::vector<Tree*> levelTrees;

protected:
//	Construction and destruction
	~MSaupePredictor()
		{ cleanUp(); }

public:
//	IStdEncPredictor interface
	OneRangePred* newPredictor(const NewPredictorData &data);
	void cleanUp();
private:
	Tree* createTree(const NewPredictorData &data);

private:
	class OneRangePredictor: public OneRangePred {
		struct HeapInfo {
			int index;
			float bestError;

			HeapInfo(int index_,float bestError_)
			: index(index_), bestError(bestError_) {}

			bool operator<(const HeapInfo &other) const
				{ return bestError>=other.bestError; }
		};

		std::vector<Tree::PointHeap*> heaps;
		std::vector<HeapInfo> infoHeap;
		float *points;
		int chunkSize, heapCount;
		Real errorConvAccel;
		bool firstChunk;

		Real normalizeSE(Real error) const
			{ return std::ldexp( 1+sqrt(1-error*errorConvAccel), 1 ); }
	public:
		OneRangePredictor(const NewPredictorData &data,int chunkSize_,const Tree &tree);
	//	OneRangePred interface
		Predictions& getChunk(float maxPredictedSE,Predictions &store);
		~OneRangePredictor() {
// TODO (admin#1#): uncomment this
			clearContainer(heaps);
			delete[] points;
		}
	};
};

#endif // SAUPEPREDICTOR_HEADER_
