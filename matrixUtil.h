#ifndef MATRIXUTIL_HEADER_
#define MATRIXUTIL_HEADER_

#include "debug.h"


/** A simple structure representing a rectangle */
struct Block {
	short x0, y0, xend, yend;

	int width()	 const	{ return xend-x0; }
	int height() const	{ return yend-y0; }
	int size()	 const	{ return width()*height(); }

	bool contains(short x,short y) const
		{ return x0<=x && x<xend && y0<=y && y<yend; }

	Block() {}
	Block(short x0_,short y0_,short xend_,short yend_)
	: x0(x0_), y0(y0_), xend(xend_), yend(yend_) {}
};

////    Matrix templates

/** A simple generic template for matrices of fixed size, uses shallow copying
 *	and manual memory management */
template<class T,class I=PtrInt> struct MatrixSlice {
	typedef MatrixSlice<const T,I> Const; ///< the class is convertible to Const type

	T *start;	///< pointer to the top-left pixel
	I colSkip;	///< how many pixels to skip to get in the next column

	/** Initializes an empty slice */
	MatrixSlice(): start(0) {}

	/** Creates a shallow copy (deleting one destroys all copies) */
	MatrixSlice(const MatrixSlice &m): start(m.start), colSkip(m.colSkip) {}

	/** Converts to a matrix of constant objects (shallow copy) */
	operator Const() const {
		Const result;
		result.start= start;
		result.colSkip= colSkip;
		return result;
	}

	/** Indexing operator - returns pointer to a column */
	T* operator[](I column) {
		ASSERT( isValid() );
		return start+column*colSkip;
	}
	/** Const version of indexing operator - doesn't allow to change the elements */
	const T* operator[](I column) const {
		return constCast(*this)[column];
	}

	/** Reallocates the matrix for a new size. If \p memory parameter is given,
	 *	it is used for storage (the user is responsible that the matrix fits in it, etc.) */
	void allocate( I width, I height, T *memory=0 ) {
		ASSERT( width>0 && height>0 );
		free();
		start= memory ? memory : new T[width*height];
		colSkip= height;
	}
	/** Releases the memory */
	void free() {
		delete[] start;
		start= 0;
	}
	/** Returns whether the matrix is allocated (and thus usable for indexing) */
	bool isValid() const {
		return start;
	}

	/** Fills a submatrix of a valid matrix with a value */
	void fillSubMatrix(const Block &block,T value) {
		ASSERT( isValid() );
	//	compute begin and end column starts
		T *begin= start+block.y0+colSkip*block.x0
		, *end= start+block.y0+colSkip*block.xend;
	//	fill the columns
	    for (T *it= begin; it!=end; it+= colSkip)
			std::fill( it, it+block.height(), value );
	}
	/** Shifts the indexing of this matrix - dangerous.
	 *	After calling this, only addressing or more shifts can be done (not checked).
	 *	Also shifts out of the allocated matrix aren't detected */
	MatrixSlice& shiftMatrix(I x0,I y0) {
		ASSERT( isValid() );
		start+= x0*colSkip+y0;
		return *this;
	}
	/** Computes relative position of a pointer in the matrix (always 0 <= \p y < #colSkip) */
	void getPosition(const T *elem,int &x,int &y) const {
		ASSERT( isValid() );
		PtrInt diff= elem-start;
		if (diff>=0) {
			x= diff/colSkip;
			y= diff%colSkip;
		} else {
			x= -((-diff-1)/colSkip) -1;
			y= colSkip - (-diff-1)%colSkip -1;
		}
	}
}; // MatrixSlice class template


/** MatrixSummer objects store partial sums of a matrix, allowing quick computation
 *	of sum of any rectangle in the matrix. It's parametrized by "type of the result",
 *	"type of the input" and "indexing type" (defaults to Int) */
template<class T,class I=PtrInt> struct MatrixSummer {
	typedef T Result;

	MatrixSlice<T,I> sums; ///< Internal matrix containing precomputed partial sums

#ifndef NDEBUG
	/** Creates an empty summer */
	MatrixSummer() {}
	/** Only empty objects are allowed to be copied (assertion) */
	MatrixSummer( const MatrixSummer &other )
		{ ASSERT( !other.isValid() ); }
	/** Only empty objects are allowed to be assigned (assertion) */
	MatrixSummer& operator=( const MatrixSummer &other )
		{ ASSERT( !other.isValid() && !isValid() ); return *this; }
#endif

	/** Returns whether the object is filled with data */
	bool isValid()	const	{ return sums.isValid(); }
	/** Clears the object */
	void free()				{ sums.free(); };

	/** Computes the sum of a rectangle (in constant time) */
	Result getSum(I x0,I y0,I xend,I yend) const {
		ASSERT( sums.isValid() );
		return sums[xend][yend] -sums[x0][yend] -sums[xend][y0] +sums[x0][y0];
	}
	/** A shortcut to get the sum of a block */
	Result getSum(const Block &b) const 
		{ return getSum( b.x0, b.y0, b.xend, b.yend ); }

	/** Prepares object to make sums for a matrix. If the summer has already been
	 *	used before, the method assumes it was for a matrix of the same size */
	template<class Input> void fill(Input inp,I width,I height) {
		if ( !sums.isValid() )
			sums.allocate(width+1,height+1);

	//	fill the edges with zeroes
		for (I i=0; i<=width; ++i)
			sums[i][0]= 0;
		for (I j=1; j<=height; ++j)
			sums[0][j]= 0;
	//	acummulate in the y-growing direction
		for (I i=1; i<=width; ++i)
			for (I j=1; j<=height; ++j)
				sums[i][j]= sums[i][j-1] + Result(inp[i-1][j-1]);
	//	acummulate in the x-growing direction
		for (I i=2; i<=width; ++i)
			for (I j=1; j<=height; ++j)
				sums[i][j]+= sums[i-1][j];
	}
}; // MatrixSummer class template

/** Helper structure for computing with value and squared sums at once */
template<class Num> struct DoubleNum {
	Num value, square;
	
	DoubleNum()	
		{ DEBUG_ONLY( value= square= std::numeric_limits<Num>::quiet_NaN(); ) }
	
	DoubleNum(Num val)
	: value(val), square(sqr(val)) {}
	
	DoubleNum(const DoubleNum &other)
	: value(other.value), square(other.square) {}
	
	void unpack(Num &val,Num &sq) const { val= value; sq= square; }
	
	DoubleNum& operator+=(const DoubleNum &other) {
		value+=	other.value;
		square+= other.square;
		return *this;
	}
	DoubleNum& operator-=(const DoubleNum &other) {
		value-=	other.value;
		square-= other.square;
		return *this;
	}
	friend DoubleNum operator+(const DoubleNum &a,const DoubleNum &b) 
		{ return DoubleNum(a)+= b; }
	friend DoubleNum operator-(const DoubleNum &a,const DoubleNum &b) 
		{ return DoubleNum(a)-= b; }
}; // DoubleNum template struct


/** Structure for a block of pixels - also contains summers and dimensions */
template< class SumT, class PixT, class I=PtrInt >
struct SummedMatrix {
	typedef DoubleNum<SumT> BSumRes;
	typedef MatrixSummer<BSumRes> BSummer;
	
	I width						///  The width of #pixels
	, height;					///< The height of #pixels
	MatrixSlice<PixT> pixels;	///< The matrix of pixels
	BSummer summer;				///< Summer for values and squares of #pixels
	bool sumsValid;				///< Indicates whether the summer values are valid
	
	/** Sets the size of #pixels, optionally allocates memory */
	void setSize( I width_, I height_ ) {
		free();
		width= width_;
		height= height_;
		pixels.allocate(width,height);
	}
	/** Frees the memory */
	void free(bool freePixels=true) {
		if (freePixels)
			pixels.free();
		else
			pixels.start= 0;
		summer.free();
		sumsValid= false;
	}
	
	/** Just validates both summers (if needed) */
	void summers_makeValid() const {
		ASSERT(pixels.isValid());
		if (!sumsValid) {
			constCast(summer).fill(pixels,width,height);
			constCast(sumsValid)= true;
		}
	}
	/** Justs invalidates both summers (to be called after changes in the pixel-matrix) */
	void summers_invalidate() 
		{ sumsValid= false; }
	/** A shortcut for getting sums of a block */
	BSumRes getSums(const Block &block) const 
		{ return getSums( block.x0, block.y0, block.xend, block.yend ); }	
	/** Gets both sums of a nonempty rectangle in #pixels, the summer isn't validated */
	BSumRes getSums( I x0, I y0, I xend, I yend ) const {
		ASSERT( sumsValid && x0>=0 && y0>=0 && xend>x0 && yend>y0 
			&& xend<=width && yend<=height );
		return summer.getSum(x0,y0,xend,yend);
	}
}; // SummedPixels template struct



/** Contains various iterators for matrices (see Matrix)
 *	to be used in walkOperate() and walkOperateCheckRotate() */
namespace MatrixWalkers {

	/** Iterates two matrix iterators and performs an action.
	 *	The loop is controled by the first iterator (\p checked) 
	 *	and on every corresponding pair (a,b) \p oper(a,b) is invoked. Returns \p oper. */
	template < class Check, class Unchecked, class Operator >
	Operator walkOperate( Check checked, Unchecked unchecked, Operator oper ) {
	//	outer cycle start - to be always run at least once
		ASSERT( checked.outerCond() );
		do {
		//	inner initialization
			checked.innerInit();
			unchecked.innerInit();
		// inner cycle start - to be always run at least once
			ASSERT( checked.innerCond() );
			do {
			//	perform the operation and do the inner step for both iterators
				oper( checked.get(), unchecked.get() );
				checked.innerStep();
				unchecked.innerStep();
			} while ( checked.innerCond() );

		//	signal the end of inner cycle to the operator and do the outer step for both iterators
			oper.innerEnd();
			checked.outerStep();
			unchecked.outerStep();

		} while ( checked.outerCond() );

		return oper;
	}


	/** Base structure for walkers */
	template<class T,class I> struct RotBase {
	public:
		typedef MatrixSlice<T,I> TMatrix;
	protected:
		TMatrix current;	///< matrix starting on the current element
		T *lastStart;		///< the place of the last enter of the inner loop

	public:
		RotBase( TMatrix matrix, int x0, int y0 )
		: current( matrix.shiftMatrix(x0,y0) ), lastStart(current.start) { 
			DEBUG_ONLY( current.start= 0; )
			ASSERT( matrix.isValid() );
		}

		void innerInit()	{ current.start= lastStart; }
		T& get()			{ return *current.start; }
	}; // RotBase class template

	#define ROTBASE_INHERIT \
		typedef typename RotBase<T,I>::TMatrix TMatrix; \
		using RotBase<T,I>::current; \
		using RotBase<T,I>::lastStart;

	/** No rotation: x->, y-> */
	template<class T,class I> struct Rotation_0: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_0( TMatrix matrix, const Block &block )
		: RotBase<T,I>( matrix, block.x0, block.y0 ) {}

		void outerStep() { lastStart+= current.colSkip; }
		void innerStep() { ++current.start; }
	};

	/** Rotated 90deg\. cw\., transposed: x<-, y-> */
	template<class T,class I> struct Rotation_1_T: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_1_T( TMatrix matrix, const Block &block )
		: RotBase<T,I>( matrix, block.xend-1, block.y0 ) {}

		void outerStep() { lastStart-= current.colSkip; }
		void innerStep() { ++current.start; }
	};

	/** Rotated 180deg\. cw\.: x<-, y<- */
	template<class T,class I> struct Rotation_2: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_2( TMatrix matrix, const Block &block )
		: RotBase<T,I>( matrix, block.xend-1, block.yend-1 ) {}

		void outerStep() { lastStart-= current.colSkip; }
		void innerStep() { --current.start; }
	};

	/** Rotated 270deg\. cw\., transposed: x->, y<- */
	template<class T,class I> struct Rotation_3_T: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_3_T( TMatrix matrix, const Block &block )
		: RotBase<T,I>( matrix, block.x0, block.yend-1 ) {}

		void outerStep() { lastStart+= current.colSkip; }
		void innerStep() { --current.start; }
	};

	/** No rotation, transposed: y->, x-> */
	template<class T,class I> struct Rotation_0_T: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_0_T( TMatrix matrix, const Block &block )
		: RotBase<T,I>( matrix, block.x0, block.y0 ) {}

		void outerStep() { ++lastStart; }
		void innerStep() { current.start+= current.colSkip; }
	};

	/** Rotated 90deg\. cw\.: y->, x<- */
	template<class T,class I> struct Rotation_1: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_1( TMatrix matrix, const Block &block )
		: RotBase<T,I>( matrix, block.xend-1, block.y0 ) {}

		void outerStep() { ++lastStart; }
		void innerStep() { current.start-= current.colSkip; }
	};

	/** Rotated 180deg\. cw\., transposed: y<-, x<- */
	template<class T,class I> struct Rotation_2_T: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_2_T( TMatrix matrix, const Block &block )
		: RotBase<T,I>( matrix, block.xend-1, block.yend-1 ) {}

		void outerStep() { --lastStart; }
		void innerStep() { current.start-= current.colSkip; }
	};

	/** Rotated 270deg\. cw\.: y<-, x-> */
	template<class T,class I> struct Rotation_3: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_3( TMatrix matrix, const Block &block )
		: RotBase<T,I>( matrix, block.x0, block.yend-1 ) {}

		void outerStep() { --lastStart; }
		void innerStep() { current.start+= current.colSkip; }
	};

	
	/** A flavour of walkOperate() choosing the right Rotation_* iterator based on \p rotation */
	template<class Check,class U,class Operator> 
	inline Operator walkOperateCheckRotate( Check checked, Operator oper
	, MatrixSlice<U> pixels2, const Block &block2, char rotation) {
		typedef PtrInt I;
		switch (rotation) {
			case 0: return walkOperate( checked, Rotation_0  <U,I>(pixels2,block2) , oper );
			case 1: return walkOperate( checked, Rotation_0_T<U,I>(pixels2,block2) , oper );
			case 2: return walkOperate( checked, Rotation_1  <U,I>(pixels2,block2) , oper );
			case 3: return walkOperate( checked, Rotation_1_T<U,I>(pixels2,block2) , oper );
			case 4: return walkOperate( checked, Rotation_2  <U,I>(pixels2,block2) , oper );
			case 5: return walkOperate( checked, Rotation_2_T<U,I>(pixels2,block2) , oper );
			case 6: return walkOperate( checked, Rotation_3  <U,I>(pixels2,block2) , oper );
			case 7: return walkOperate( checked, Rotation_3_T<U,I>(pixels2,block2) , oper );
			default: ASSERT(false); return oper;
		}
	}
	
	
	/** Performs manipulations with rotation codes 0-7 (dihedral group of order eight) */
	struct Rotation {
		/** Asserts the parameter is within 0-7 */
		static void check(int DEBUG_ONLY(r)) {
			ASSERT( 0<=r && r<8 );
		}
		/** Returns inverted rotation (the one that takes this one back to identity) */
		static int invert(int r) {
			check(r);
			return (4-r/2)%4 *2 +r%2;
		}
		/** Computes rotation equal to projecting at first with \p r1 and the result with \p r2 */
		static int compose(int r1,int r2) {
			check(r1); check(r2);
			if (r2%2)
				r1= invert(r1);
			return (r1/2 + r2/2) %4 *2+ ( (r1%2)^(r2%2) );
		}
	}; // Rotation struct
	
	/** Checked_ iterator for a rectangle in a matrix, no rotation */
	template<class T,class I=PtrInt> struct Checked: public Rotation_0<T,I> {
		typedef MatrixSlice<T,I> TMatrix;
		typedef Rotation_0<T,I> Base;
		using Base::current;
		using Base::lastStart;
		
		T *colEnd			///  the end of the current column
		, *colsEndStart;	///< the start of the end column

		/** Initializes a new iterator for a \p block of \p pixels */
		Checked( TMatrix pixels, const Block &block )
		: Base( pixels, block ), colEnd( pixels.start+pixels.colSkip*block.x0+block.yend )
		, colsEndStart( pixels.start+pixels.colSkip*block.xend+block.y0 ) {
			ASSERT( block.xend>block.x0 && block.yend>block.y0 );
		}

		bool outerCond() {
			ASSERT(lastStart<=colsEndStart); 
			return lastStart!=colsEndStart; 
		}
		void outerStep() {
			colEnd+= current.colSkip; 
			Base::outerStep();
		}
		bool innerCond() {
			ASSERT(current.start<=colEnd);
			return current.start!=colEnd; 
		}
	}; // Checked class template

	/** Checked_ iterator for a whole QImage; no rotation, but always transposed */
	template<class T,class U> struct CheckedImage {
		typedef T QImage;
		typedef U QRgb;
	protected:
		QImage &img;	///< reference to the image
		int lineIndex	///  index of the current line
		, width			///  the width of the image
		, height;		///< the height of the image
		QRgb *line		///  pointer to the current pixel
		, *lineEnd;		///< pointer to the end of the line

	public:
		/** Initializes a new iterator for an instance of QImage (Qt class) */
		CheckedImage(QImage &image)
		: img(image), lineIndex(0), width(image.width()), height(image.height())
			{ DEBUG_ONLY(line=lineEnd=0;) }

		bool outerCond()	{ return lineIndex<height; }
		void outerStep()	{ ++lineIndex; }

		void innerInit()	{ line= (QRgb*)img.scanLine(lineIndex); lineEnd= line+width; }
		bool innerCond()	{ return line!=lineEnd; }
		void innerStep()	{ ++line; }

		QRgb& get()			{ return *line; }
	}; // CheckedImage class template
	

	/** A convenience base type for operators to use with walkOperate() */
	struct OperatorBase {
		void innerEnd() {}
	};

	/** An operator computing the sum of products */
	template<class TOut,class TIn> struct RDSummer: public OperatorBase {
		TOut totalSum, lineSum;

		RDSummer()
		: totalSum(0), lineSum(0) {}
		void operator()(const TIn &num1,const TIn& num2)
			{ lineSum+= TOut(num1) * TOut(num2); }
		void innerEnd()
			{ totalSum+= lineSum; lineSum= 0; }
		TOut result()	///< returns the result
			{ ASSERT(!lineSum); return totalSum; }
	};

	/** An operator performing a= (b+::toAdd)*::toMul */
	template<class T> struct AddMulCopy: public OperatorBase {
		const T toAdd, toMul;

		AddMulCopy(T add,T mul)
		: toAdd(add), toMul(mul) {}

		template<class R1,class R2> void operator()(R1 &res,R2 f) const
			{ res= (f+toAdd)*toMul; }
	};

	/** An operator performing a= b*::toMul+::toAdd
	 *	and moving the result into [::min,::max] bounds */
	template<class T> struct MulAddCopyChecked: public OperatorBase {
		const T toMul, toAdd, min, max;

		MulAddCopyChecked(T mul,T add,T minVal,T maxVal)
		: toMul(mul), toAdd(add), min(minVal), max(maxVal) {}
		
		template<class R1,class R2> void operator()(R1 &res,R2 f) const
			{ res= checkBoundsFunc( min, f*toMul+toAdd, max ); }
	};
} // MatrixWalkers namespace

#endif // MATRIXUTIL_HEADER_
