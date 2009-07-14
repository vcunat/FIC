#ifndef NOPREDICTOR_HEADER_
#define NOPREDICTOR_HEADER_

#include "../headers.h"

/** Predictor that doesn't predict, just tries all the domains */
class MNoPredictor: public IStdEncPredictor {

	DECLARE_TypeInfo_noSettings( MNoPredictor, "brute force"
	, "Doesn't predict, tries all possibilities." )

public:
/**	\name IStdEncPredictor interface
 *	@{ */
	IOneRangePredictor* newPredictor(const NewPredictorData &data)
		{ return new OneRangePredictor( data.poolInfos->back().indexBegin, data.allowRotations ); }
	void cleanUp() {} // nothing to clean up
///	@}

protected:
	/** Predictor class returned when calling #newPredictor
	 *	- returns all domains in all rotations in one chunk */
	class OneRangePredictor: public IOneRangePredictor {
		int domCount /// the domain count
		, rotations; ///< the number of rotations used

	public:
		OneRangePredictor(int domainCount,bool allowRotations)
		: domCount(domainCount), rotations( allowRotations ? 8 : 1 ) {}
	/**	\name OneRangePred interface
	 *	@{ */
		Predictions& getChunk(float /*maxPredictedSE*/,Predictions &store) {
			store.clear();
			if (domCount) {
				store.reserve(domCount*rotations);
				for (int id=0; id<domCount; ++id)
					for (int r=0; r<rotations; ++r)
						store.push_back( Prediction(id,r) );
				domCount= 0;
			}
			return store;
		}
	///	@}
	}; // MNoPredictor::OneRangePredictor class

}; // MNoPredictor class

#endif
