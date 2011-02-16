#include "diffColor.h"
#include "../fileUtil.h"
#include <QImage> // because of returning QImage type by ::planes2image() method

namespace MatrixWalkers {
	/** Matrix iterator performing a 2D-diff \todo doc */
	template < class T, class ResT, class I=PtrInt >
	struct Differ2D: public Rotation_0<T,I> { ROTBASE_INHERIT
		typedef Rotation_0<T,I> Base;
		
		Differ2D( TMatrix matrix, I x0=0, I y0=0 )
		: Base(matrix,x0,y0) {}

		ResT get() {
			T *now= current.start;
			I skip= current.colSkip;
		//	parenthesised the expression to reduce rounding errors
			return ( ResT(now[0]) - now[1] ) + ( ResT(now[skip+1]) - now[skip] );
		}
	}; // Differ2D walker
	
	/** Operator maintaining the minimum and maximum value of the second argument */
	template <class T>
	struct MinMaxSecond: public MinMax<T>, public OperatorBase {
		template<class U> void operator()(U&,T val) { apply(val); }
	};
}

MDiffColor::PlaneList MDiffColor
::image2planes( const QImage &toEncode, const PlaneSettings &prototype ) {
//	get one-color planes from the child module
	PlaneList planes= moduleColor()->image2planes(toEncode,prototype);
	free();
	ownedPlanes.clear();
	ownedPlanes.reserve( planes.size() );
	
//	differentiate the returned planes
	for (PlaneList::iterator plane=planes.begin(); plane!=planes.end(); ++plane) {	
	//	create adjusted settings of the plane, its matrix and store them as owned
		PlaneSettings *resSet= new PlaneSettings(*plane->settings);
		--resSet->width;
		--resSet->height;
		
		SMatrix resMatrix;
		resMatrix.allocate( resSet->width, resSet->height );
		
	//	prepare the walkers
		using namespace MatrixWalkers;
		Checked<SReal> resWalker( resMatrix, Block(0,0,resSet->width,resSet->height) );
		Differ2D<SReal,Real> diffWalker(plane->pixels);
		
	//	compute the minimum and maximum value after differentiation
		MinMax<Real> minMax= walkOperate( resWalker, diffWalker, MinMaxSecond<Real>() );
		minMax.symmetrize();

		// \todo adjusting the quality?  <max. SE>*= sqr(quot); -- can't do by changing plane->quality
		
	//	fill the resulting matrix with normalized diffs and push_back the resulting plane
		walkOperate( resWalker, diffWalker, minMax.get01normalizer() );
		ownedPlanes.push_back( PlaneDiff(resMatrix,resSet,plane->pixels,minMax) );
	} // for each plane
	
//	return sliced copy of ownedPlanes
	return PlaneList( ownedPlanes.begin(), ownedPlanes.end() );
}

/// \todo impl
void MDiffColor::writeData(ostream &file) {	
}

/// \todo impl
IColorTransformer::PlaneList MDiffColor
::readData( istream &file, const PlaneSettings &prototype ) {
	return PlaneList();
}




namespace NOSPACE {
	/** An iterator skipping over regular intervals of an array when iterating */
	template<class T,class I=PtrInt> struct SkipIterator {
		T *now;	///< The current position
		I skip;	///< The size of every step
		
		SkipIterator(T *start,I skip_)
		: now(start), skip(skip_) {}
		
		void operator++()	{ now+= skip; }
		T& operator*()		{ return *now; }
		
		SkipIterator operator+(I toSkip) {
			SkipIterator result(*this);
			result.now+= skip*toSkip;
			return result;
		}
		T& operator[](I index) { ASSERT( (*this+index).now == now+skip*index );
			return  /* *(*this+index)*/ now[skip*index] ; }
		
		friend inline bool operator!=(const SkipIterator &a,const SkipIterator &b) {
			ASSERT(a.skip==b.skip);
			return a.now!=b.now;
		}
		
		/** Creates a SkipIterator iterating in a matrix */
		static SkipIterator fromMatrix( MatrixSlice<T,I> matrix, I x0, I y0, I xstep, I ystep ) {
			I cs= matrix.colSkip;
			return SkipIterator( matrix.start+cs*x0+y0, cs*xstep+ystep ); 
		}
	}; // SkipIterator struct
	
	/** A simple summing functor */
	template<class T> struct Summer {
		T sum;
		
		Summer(): sum(0) {}
		template<class U> void operator()(const U &val) { sum+= val; }
	};
	
	struct NoOp {
		template<class T> T apply(T val) const { return val; }
	};

	/** Reconstructs an "array" of values from an "array" of differences and sum of the values.
	 *	The "arrays" are specified by common length \p n 
	 *	and two iterators for diffs and the result. */
	template < class R, class Denorm, class IterD, class IterV, class I > inline
	void diffs2vals( I n, R avg, Denorm denorm, IterD diffIt, IterV valIt ) {
		Real bwCums[n]; 
	//	set bwCums[j]= \sum_{i=1}^{n-1-j}{i*diff[n-1-i]}
		Real acc= 0;
		bwCums[n-1]= acc;
		for (I i=1; i<n; ++i) {
			acc+= Real(i) * denorm.apply(diffIt[n-1-i]);
			bwCums[n-1-i]= acc;
		}
		
	//	imagine (in ::acc) fwCums[j]= \sum_{i=1}^j{i*diff[i-1]} 
	//	and have val[j]= fwCums[j]-bwCums[j] /n +avg
		Real nInv= 1/Real(n);
		acc= 0;
		valIt[0]= (acc-bwCums[0]) * nInv;
		for (I i=1; i<n; ++i) {
			acc+= Real(i) * denorm.apply(diffIt[i-1]);
			valIt[i]= (acc-bwCums[i]) * nInv + avg;
		}
	} // diff2vals
	
	void test() {
		const int n= 5*5*5 *2 +1;
		Real a0=1, b0=2, c0=2, d0=1, e0=0;
		Real a=b0-a0, b=c0-b0, c=d0-c0, d=e0-d0, e=a0-e0;
		Real avg= (a0+b0+c0+d0+e0)/5;
		Real diffs[n-1]= {	a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e,  
							a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e,  
							a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e,  
							a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e,  
							a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e,  
							
							a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e,  
							a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e,  
							a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e,  
							a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e,  
							a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e, a,b,c,d,e,  
							};
		Real vals[n];
		diffs2vals( n, avg, NoOp(), diffs, vals );
	}
	
} // anonymous namespace


QImage MDiffColor::planes2image() {
	ensureAvgs();
	vector<PlaneDiff>::iterator plane;
	for (plane=ownedPlanes.begin(); plane!=ownedPlanes.end(); ++plane) {
	//	compute some shortcuts
		int origW= plane->settings->width+1;
		int origH= plane->settings->height+1;
	//	undifferentiate rows
		for (int y=0; y<origH-1; ++y)
			diffs2vals( origW, plane->horizAvgDiffs()[y]
				, plane->diffBounds.get01denormalizer()
				, SkipIterator<SReal>::fromMatrix(plane->pixels, 0,y,  1,0)
				, SkipIterator<SReal>::fromMatrix(plane->origPix,0,y+1,1,0) );
	//	undifferentiate columns
		for (int x=0; x<origW; ++x)
			diffs2vals( origH, plane->vertAvgs()[x]
				, NoOp()
				, SkipIterator<SReal>::fromMatrix(plane->origPix,x,1,0,1)
				, SkipIterator<SReal>::fromMatrix(plane->origPix,x,0,0,1) );
					//< there we rewrite the matrix a bit wildly under our hands
	}
	return moduleColor()->planes2image();
}

void MDiffColor::ensureAvgs() {
//	prepare every plane
	vector<PlaneDiff>::iterator plane;
	for (plane=ownedPlanes.begin(); plane!=ownedPlanes.end(); ++plane) {
	//	allocate memory or find it's already been done	
		if ( !plane->allocAvgs() )
			continue; //< we could even return
		
	//	compute some shortcuts
		int origW= plane->settings->width+1;
		int origH= plane->settings->height+1;
		Real hInv= 1/Real(origH);
		Real wInv= 1/Real(origW);
		
	//	compute the vertical averages
	/*
		Real *vAvg= plane->vertAvgs();
		for (int x=0; x<origW; ++x) {
			const SReal *line= plane->pixels[x];
			vAvg[x]= hInv * for_each( line, line+origH, Summer<Real>() ).sum;
		}
	*/
	
		{
			Real sumAvgs= 0;
			Real lastAvg, diffAvg[origW-1];
		//	compute the diffs of vertical averages
			for (int x=0; x<origW; ++x) {
				const SReal *column= plane->origPix[x];
				Real newAvg= hInv * for_each( column, column+origH, Summer<Real>() ).sum;
				sumAvgs+= newAvg;
				if (x>0)
					diffAvg[x-1]= newAvg-lastAvg; // \todo quantization
				lastAvg= newAvg;
			}
			// \todo quantizing sumAvgs
		//	compute the appropriate vertical averages (to be the same as when decoding)
			diffs2vals( origW, sumAvgs*wInv, NoOp(), diffAvg, plane->vertAvgs() );
		} {
		//	compute the diffs of horizontal averages
			Real lastAvg, *diffAvg= plane->horizAvgDiffs();
			for (int y=0; y<origH; ++y) {
				SkipIterator<SReal> row= 
					SkipIterator<SReal>::fromMatrix( plane->origPix, 0, y, 1, 0 );
				Real newAvg= wInv * for_each( row, row+origW, Summer<Real>() ).sum;
				if (y>0)
					diffAvg[y-1]= newAvg-lastAvg; // \todo quantization
				lastAvg= newAvg;
			}
		}
		
	} // for every owned plane
	
} // computeAvgs method


