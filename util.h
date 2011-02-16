#ifndef UTIL_HEADER_
#define UTIL_HEADER_

/// \file

/** Field containing 2^i on i-th position, defined in modules.cpp */
extern const int powers[31];

/** Helper template function computing the square of a number */
template<class T> inline T sqr(T i)		{ return i*i; }
/** Helper template function computing the cube of a number */
template<class T> inline T cube(T i)	{ return i*i*i; }

/** Returns i*2^bits */
template<class T> inline T lShift(T i,T bits)
	{ ASSERT(bits>=0); return i<<bits; }
/** Returns i/2^bits */
template<class T> inline T rShift(T i,T bits)
	{ ASSERT(bits>=0 && i>=0); return i>>bits; }
	
/** This function is missing in earlier GCC versions, here implemented via exp2 and log2 */
template<class T> inline T exp10(T val) 
	{ return exp2( val*log2(T(10)) ); }

/** Returns ceil(log2(i)) */
inline int log2ceil(int i) {
	ASSERT(i>0);
	--i;
	int result= 0;
	while (i) {
		i/= 2;
		++result;
	}
	return result;
}

/** A wrapper around isnan() because of compiler support */
template<class T> inline bool isNaN(T num) {
#ifndef __ICC
	return isnan(num);
#else
	return num!=num;
#endif
}

/** How many short intervals (shifted by density) can fit into a long interval (discrete) */
inline int getCountForDensity(int longLength,int density,int shortLength)
	{ return (longLength-shortLength)/density +1; }

/** The same as above, but in 2D for squares fitting into a rectangle */
inline int getCountForDensity2D(int width,int height,int density,int sideSize) {
	return getCountForDensity(width,density,sideSize)
	*	getCountForDensity(height,density,sideSize);
}

/** General bounds-checking template routine - returns max(low,min(value,high)) */
template<class T> inline T checkBoundsFunc(T low,T value,T high) {
	if (value<low)
		return low;
	if (value>high)
		return high;
	return value;
}

/** Struct for conversion between 0-1 Real and 0-(2^power-1) integer */
template<int power,class R> struct Float2int {
	static R convert(int i)
		{ return std::ldexp( i+R(0.5), -power ); }	
		
	static int convert(R r)
		{ return (int)trunc(std::ldexp( r, power )); }
	static int convertCheck(R r)
		{ return checkBoundsFunc( 0, convert(r), powers[power]-1 ); }
};

/** A simple for_each wrapper for iterating the whole STL-like container */
template<class C,class F> inline F for_each(C &container,F functor)
	{ return for_each( container.begin(), container.end(), functor ); }
	
/** Counts the number of '\n' characters in a C-string */
inline int countEOLs(const char *s) {
	int result= 0;
	for (; *s; ++s)
		if (*s=='\n')
			++result;
	return result;
}

/** Type convertor - NonConstType<T>::Result is a non-const variant of T or T itself if N/A */
template<class T> struct NonConstType			{ typedef T Result; };
template<class T> struct NonConstType<const T>	{ typedef T Result; };

/** Automatic version of const_cast for pointers */
template <class T> inline T* constCast(const T* toCast) { return const_cast<T*>(toCast); }
/** Automatic version of const_cast for references */
template <class T> inline T& constCast(const T& toCast)	{ return const_cast<T&>(toCast); }

/** Checking a condition - throws std::exception if false */
inline void checkThrow(bool check) { if (!check) throw std::exception(); }


/** Template object for automated deletion of pointers (useful in for_each) */
struct SingleDeleter {
	template <class T> void operator()(T *toDelete) const { delete toDelete; }
};
/** Template object for automated deletion of field pointers (useful in for_each) */
struct MultiDeleter {
	template <class T> void operator()(T *toDelete) const { delete[] toDelete; }
};


/** Deletes all pointers in a container (it has to support \c begin and \c end methods) */
template<class C> inline void clearContainer(const C &container)
	{ for_each( container, SingleDeleter() ); }	
/** Clears a QList of pointers (and deletes the pointers) */
template<class C> inline void clearQtContainer(C container)
	{ while (!container.isEmpty()) delete container.takeFirst(); }	

/** General stuff for quantizing real numbers into integers */
namespace Quantizer {
	/** Quantizes f that belongs to [0,\p possib/2^\p scale] into [0,\p possib-1] */
	template<class R> inline
	int quantizeByPower(R f,int scale,int possib) {
		ASSERT( f>=0 && f<=possib/(R)powers[scale] );
		int result= (int)trunc(ldexp(f,scale));
		ASSERT( result>=0 && result<=possib );
		return result<possib ? result : --result;
	}
	/** Performs the opposite to ::quantizeByPower */
	template<class R> inline
	R dequantizeByPower(int i,int scale,int DEBUG_ONLY(possib)) {
		ASSERT( i>=0 && i<possib );
		R result= ldexp(i+R(0.5),-scale);
		ASSERT( result>=0 && result<= possib/(R)powers[scale] );
		return result;
	}

	template<class R> class QuantBase {
	protected:
		int scale	///  how much should the values be scaled (like in ::quantizeByPower)
		, possib;	///< the number of possibilities for quantization
	public:
		/** Quantizes a value */
		int quant(R val) const
			{ return quantizeByPower<R>(val,scale,possib); }
		/** Dequantizes a value */
		R dequant(int i) const
			{ return dequantizeByPower<R>(i,scale,possib); }
		/** Quant-rounds a value - returns its state after quantization and dequantization */
		R qRound(R val) const
			{ return dequant(quant(val)); }
	};
} // Quantizer namespace


template <class T,int bulkKb=64>
class BulkAllocator {
	enum { bulkCount=(bulkKb*1024)/sizeof(T) };

	std::vector<T*> pools;
	PtrInt nextIndex;

public:
	BulkAllocator()
	: nextIndex(bulkCount) {}
	BulkAllocator(const BulkAllocator &DEBUG_ONLY(copy))
		{ nextIndex=bulkCount; ASSERT(copy.pools.empty()); }
	~BulkAllocator()
		{ for_each( pools, MultiDeleter() ); }

	T* make() {
	//	check for errors
		ASSERT(nextIndex<=bulkCount);
	//	allocate a new bulk if needed
		if (nextIndex==bulkCount) {
			nextIndex= 0;
			pools.push_back( new T[bulkCount] );
		}
		return & (pools.back()[nextIndex++]);
	}
	T* makeField(PtrInt count) {
	//	check for errors
		ASSERT(nextIndex<=bulkCount);

		if (count>bulkCount/2) {
			T *result= new T[count];
			if (pools.empty())
				pools.push_back(result);
			else {
				pools.push_back(pools.back());
				*(pools.end()-2)= result;
			}
			return result;
		}

		if (nextIndex+count>bulkCount) {
		//	some space will be wasted
			nextIndex=0;
			pools.push_back(new T[bulkCount]);
		}
		T *result=&pools.back()[nextIndex];
		nextIndex+=count;
		return result;
	}
}; // BulkAllocator class

/** Structure providing support for progress update and interruption (used for encoding) */
struct UpdateInfo {
	typedef void (*IncInt)(int increment); ///< Type for used functions -> more readable code
	
	static void emptyFunction(int) {}		///< does nothing, default for IncInt functions
	static const bool noTerminate= false;	///< default for #terminate, defined in modules.cpp
	static const UpdateInfo none;			///< empty UpdateInfo instance, in modules.cpp

	volatile const bool *terminate;	///< true if the action should be terminated
	IncInt incMaxProgress			///  function for increasing the maximum progress (100%)
	, incProgress;					///< function for increasing the current progress

	/** Initializes the structure from supplied parametres */
	UpdateInfo( const bool &terminate_, IncInt incMaxProgress_, IncInt incProgress_ )
	: terminate(&terminate_), incMaxProgress(incMaxProgress_), incProgress(incProgress_) 
		{ ASSERT(isValid()); }
		
	bool isValid() const { return terminate && incMaxProgress && incProgress; }
};

#endif // UTIL_HEADER_
