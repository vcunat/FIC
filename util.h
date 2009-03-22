#ifndef UTIL_HEADER_
#define UTIL_HEADER_

#include "headers.h"
#include "debug.h"

/** Field containing 2^i on i-th position, defined in modules.cpp */
extern const int powers[31];

/** Helper template function computing the square of a number */
template<typename T> inline T sqr(T i)
	{ return i*i; }
/** Helper template function computing the cube of a number */
template<typename T> inline T cube(T i)
	{ return i*i*i; }

/** Returns i*2^bits */
template<typename T> inline T lShift(T i,T bits)
	{ ASSERT(bits>=0); return i<<bits; }
	
/** Returns i/2^bits */
template<typename T> inline T rShift(T i,T bits)
	{ ASSERT(bits>=0 && i>=0); return i>>bits; }

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
	if (value>low)
		if (value<high)
			return value;
		else
			return high;
	else
		return low;
}

/** Struct for conversion between [0..1] Real and 0..(2^power-1) integer */
template<int power=8,class R=MTypes::Real> struct Float2int {
	static R convert(int i)
		{ return std::ldexp( i+R(0.5), -power ); }	
		
	static int convert(R r)
		{ return (int)trunc(std::ldexp( r, power )); }
	static int convertCheck(R r)
		{ return checkBoundsFunc( 0, convert(r), powers[power]-1 ); }
};

/** Counts the number of '\n' characters in a C-string */
inline int countEOLs(const char *s) {
	int result= 0;
	for (; *s; ++s)
		if (*s=='\n')
			++result;
	return result;
}

template<class T> inline std::string toString(const T &what) {
	std::stringstream stream;
	stream << what;
	std::string result;
	stream >> result;
	return result;
}

template<class T> struct NonConstType			{ typedef T Result; };
template<class T> struct NonConstType<const T>	{ typedef T Result; };

/** Automatic version of const_cast for pointers */
template <class T> inline T* constCast(const T* toCast) { return const_cast<T*>(toCast); }
/** Automatic version of const_cast for references */
template <class T> inline T& constCast(const T& toCast)	{ return const_cast<T&>(toCast); }

/** Automatic helper cast from "T**" to "const T**" */
template <class T> inline const T** bogoCast(T** toCast) 
	{ return const_cast<const T**>(toCast); }

/** Checking a condition - throws an exception if false */
inline void checkThrow(bool check) { if (!check) throw std::exception(); }


/* Auto-release pointer template altered to work more like an ordinary pointer *//*
template<class T> class Auto_ptr: public std::auto_ptr<T> {
public:
	Auto_ptr(T *t=0): std::auto_ptr<T>(t) {}
    operator T*() const
		{ return this->get(); }
    Auto_ptr& operator=(T *t) {
        reset(t);
        return *this;
    }
};
*/


/** Template object for automated deletion of pointers (useful in for_each) */
template <class T> struct SingleDeleter {
	void operator()(T *toDelete) { delete toDelete; }
};
/** Template object for automated deletion of field pointers (useful in for_each) */
template <class T> struct MultiDeleter {
	void operator()(T *toDelete) { delete[] toDelete; }
};

#ifndef __ICC
	/** Deletes all pointers in a container (it has to support \c begin and \c end methods) */
	template < class T, template<class> class C > inline
	void clearContainer(const C<T*> &container)
		{ for_each( container.begin(), container.end(), SingleDeleter<T>() ); }
		
#else
	#define clearContainer(cont_) \
		do { \
			typedef typeof(cont_) contType_; \
			const contType_ &c_= cont_; \
			for (contType_::const_iterator it_=c_.begin(); it_!=c_.end(); ++it_) \
				delete *it_; \
		} while (false)
#endif

template <class T,int bulkKb=64>
class BulkAllocator {
	enum { bulkCount=(bulkKb*1024)/sizeof(T) };

	std::vector<T*> pools;
	size_t nextIndex;

public:
	BulkAllocator()
	: nextIndex(bulkCount) {}
	BulkAllocator(const BulkAllocator &DEBUG_ONLY(copy))
		{ nextIndex=bulkCount; ASSERT(copy.pools.empty()); }
	~BulkAllocator()
		{ for_each( pools.begin(), pools.end(), MultiDeleter<T>() ); }

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
	T* makeField(size_t count) {
	//	check for errors
		ASSERT(nextIndex<=bulkCount);

		if (count>bulkCount/2) {
			T *result=new T[count];
			if (pools.empty())
				pools.push_back(result);
			else {
				pools.push_back(pools.back());
				*(pools.end()-2)=result;
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
};

struct UpdateInfo {
	typedef void (*IncInt)(int increment);

	volatile const bool *terminate;
	IncInt incMaxProgress, incProgress;

	static void emptyFunction(int) {}
	static const bool noTerminate= false;	///< defined in modules.cpp

	UpdateInfo( const bool &terminate_= noTerminate, IncInt incMaxProgress_=&emptyFunction
	, IncInt incProgress_=&emptyFunction )
	: terminate(&terminate_), incMaxProgress(incMaxProgress_), incProgress(incProgress_) {}
};

#endif // UTIL_HEADER_
