

/** Defining my own assert macro */
#undef assert
#ifdef NDEBUG
	#define DEBUG_ONLY(expr)
	#define assert(expr) void(0)
#else
	inline void assert_fail_() {
 		int *i= 0, j=*i;
		*i=j;
	}
	#define DEBUG_ONLY(expr) expr
	#define assert(expr) ( expr ? void(0) : assert_fail_() )
#endif

#define NOSPACE /* empty */


/** Cast that is "dynamic" (with assert) in debug mode and "static" in release mode */
template <class T,class U> inline T debugCast(U toCast) {
#ifdef NDEBUG
	return static_cast<T>(toCast);
#else
	if (!toCast)
		return 0; // skip the assertion
	T result= dynamic_cast<T>(toCast);
	assert(result);
	return result;
#endif
}

/** Instanciates some methods of a container - simplifies debugging */
#ifdef NDEBUG
	#define DEBUG_CONTAINER(cont_)
#else
	#define DEBUG_CONTAINER(cont_) template<> void debugContainer_< cont_ >();
	
	template<class T> void debugContainer_() {
		T c;
		int i= c.size();
		i= c.end() - c.begin();
		c[i]= c[0];
		*( c.begin() + i )= *( c.end() - i )
		
		assert(false); // instanciation function - not to be executed
	}
#endif
