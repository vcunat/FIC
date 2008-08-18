#ifndef NOPREDICTOR_HEADER_
#define NOPREDICTOR_HEADER_

#include "../interfaces.h"

class NoPredictor: public IStdEncPredictor {
	
	DECLARE_M_cloning_name_desc( NoPredictor, "brute force"
	, "Doesn't predict, tries all possibilities." )
	
	DECLARE_M_settings_none()
	
public:
//	IStdEncPredictor interface
	OneRangePred* newPredictor(const NewPredictorData &data) 
		{ return new OneRangePredictor( data.poolInfos->back().indexBegin, data.allowRotations ); }
	void cleanUp() {};
	
private:
	class OneRangePredictor: public OneRangePred {
		int domCount, rotations;
		
	public:
		OneRangePredictor(int domainCount,bool allowRotations)
		: domCount(domainCount), rotations( allowRotations ? 8 : 1 ) {}
	//	OneRangePred interface
		Predictions& getChunk(float /*maxPredictedSE*/,Predictions &store) {
			store.clear();
			if (domCount) {
				store.reserve(domCount*rotations);
				for (int id=0; id<domCount; ++id)
					for (int r=0; r<rotations; ++r)
						store.push_back( Prediction(id,r) );
				domCount=0;
			}
			return store;
		}
	};
};

#endif
