#ifndef UTIL_HEADER_
#define UTIL_HEADER_

#include "headers.h"
#include "debug.h"

/** Field containing 2^i on i-th position, defined in modules.cpp */
extern const int powers[];

/** Helper template function computing the square of a number */
template<typename T> inline T sqr(T i)
	{ return i*i; }
/** Helper template function computing the cube of a number */
template<typename T> inline T cube(T i)
	{ return i*i*i; }

/** Returns ceil(log2(i)) */
inline int log2ceil(int i) {
	assert(i>0);
	--i;
	int result=0;
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
/** \overload */
inline void checkByte(int &b)
	{ b= checkBoundsFunc(0,b,255); }
/*
inline void checkFloat(float &f) {
    f=checkBoundsFunc<float>(0,f,1);
}
*/

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

/** Automatic version of const_cast for pointers */
template <class T> inline T* constCast(const T* toCast) { return const_cast<T*>(toCast); }
/** Automatic version of const_cast for references */
template <class T> inline T& constCast(const T& toCast)	{ return const_cast<T&>(toCast); }

/** Checking a condition - throws an exception if false */
inline void checkThrow(bool check) { if (check) throw std::exception(); }


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

template <class T,int bulkKb=8>
class BulkAllocator {
	enum { bulkCount=(bulkKb*1024)/sizeof(T) };

	std::vector<T*> pools;
	size_t nextIndex;

public:
	BulkAllocator()
	: nextIndex(bulkCount) {}
	BulkAllocator(const BulkAllocator &DEBUG_ONLY(copy))
		{ nextIndex=bulkCount; assert(copy.pools.empty()); }
	~BulkAllocator()
		{ for_each( pools.begin(), pools.end(), MultiDeleter<T>() ); }

	T* make() {
	//	check for errors
		assert(nextIndex<=bulkCount);
	//	allocate a new bulk if needed
		if (nextIndex==bulkCount) {
			nextIndex=0;
			pools.push_back( new T[bulkCount] );
		}
		return & (pools.back()[nextIndex++]);
	}
	T* makeField(size_t count) {
	//	check for errors
		assert(nextIndex<=bulkCount);

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

struct UpdatingInfo {
	typedef void (*SetMaxProgress)(int maxProgress);
	typedef void (*IncProgress)(int increase);

	volatile const bool &terminate;
	const SetMaxProgress setMaxProgress;
	const IncProgress incProgress;

	static void emptyFunction(int);

	UpdatingInfo( const bool &terminate_, SetMaxProgress setMaxProgress_=&emptyFunction
	, IncProgress incProgress_=&emptyFunction )
	: terminate(terminate_), setMaxProgress(setMaxProgress_), incProgress(incProgress_) {}
private:
	UpdatingInfo& operator=(const UpdatingInfo&);
};

#endif // UTIL_HEADER_
