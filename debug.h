#ifndef DEBUG_HEADER_
#define DEBUG_HEADER_

/** A shortcut for things only to be compiled in debugging mode */
#ifdef NDEBUG
	#define DEBUG_ONLY(expr)
#else
	#define DEBUG_ONLY(expr) expr
#endif


/** Defining my own ASSERT macro to pause execution in the debugger */
#ifdef NDEBUG
	#define DEBUG_ONLY(expr)
	#define ASSERT(expr) void(0)
#else
	inline void assert_fail_() {
 		int *i= 0, j= *i;
		*i= j;
	}
	#define DEBUG_ONLY(expr) expr
	#define ASSERT(expr) ( (expr) ? void(0) : assert_fail_() )
#endif


/** The name for anonymous namespaces (to unconfuse some IDEs) */
#define NOSPACE /* empty */


/** Cast that is "dynamic" (with ASSERT) in debug mode and "static" in release mode */
template <class T,class U> inline T debugCast(U toCast) {
	#ifdef NDEBUG
		return static_cast<T>(toCast);
	#else
		if (!toCast)
			return 0; // skip the assertion
		T result= dynamic_cast<T>(toCast);
		ASSERT(result);
		return result;
	#endif
}


/** A shortcut declaration and empty implementation for modules */
#ifdef NDEBUG
	#define DECLARE_debugModule
	#define DECLARE_debugModule_empty
#else
	#define DECLARE_debugModule public: \
		virtual QWidget* debugModule(QPixmap &pixmap,const QPoint &click)
	#define DECLARE_debugModule_empty public: \
		virtual QWidget* debugModule(QPixmap &,const QPoint &) { return 0; }
#endif


/** A debugging utility - stores the position in a stream into ::pos (a global variable) */
#ifndef NDEBUG
	extern int pos;
	inline void STREAM_POS(std::istream &file) { pos= file.tellg(); }
	inline void STREAM_POS(std::ostream &file) { pos= file.tellp(); }
#else
	inline void STREAM_POS(std::ios&) {}
#endif


#endif // DEBUG_HEADER_
