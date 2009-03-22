#ifndef MATRIXUTIL_HEADER_
#define MATRIXUTIL_HEADER_

#include "debug.h"


/** Structure representing a rectangle */
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

/** A simple generic template for matrices of fixed size, uses shallow copying */
template<class T,class I=size_t> class Matrix {
public:
	friend class Matrix< typename NonConstType<T>::Result, I >; ///< because of conversion
	typedef Matrix<const T,I> ConstMatrix; ///< the class is convertible to ConstMatrix

protected:
	T *start;	///< pointer to the top-left pixel
	I colSkip;	///< how many pixels to skip to get in the next column
	DEBUG_ONLY(I width;);
public:
	/** Initializes an empty matrix */
	Matrix()
	: start(0) {}

	/** Allocates a matrix of given size via #allocate (see for details) */
	Matrix( I width_, I height_, T *memory=0 )
	: start(0) {
		allocate(width_,height_,memory);
	}

	/** Creates a shallow copy */
	Matrix(const Matrix &m)
	: start(m.start), colSkip(m.colSkip) {
		DEBUG_ONLY( width= m.width; );
	}

	/** Converts to a matrix of constant objects (shallow copy) */
	operator ConstMatrix() const {
		ConstMatrix result;
		result.start= start;
		result.colSkip= colSkip;
		DEBUG_ONLY( result.width= width; )
		return result;
	}

	/** Indexing operator - returns pointer to a column */
	T* operator[](I column) {
		ASSERT( start && /*column>=0 &&*/ column<width );
		return start+column*colSkip;
	}
	/** Const version of indexing operator - doesn't allow to change the elements */
	const T* operator[](I column) const {
		return constCast(*this)[column];
	}

	/** Reallocates the matrix for a new size. If \p memory parameter is given,
	 *	it is used for storage (the user is responsible that the matrix fits in it, etc.) */
	void allocate( I width_, I height_, T *memory=0 ) {
		free();
		start= memory ? memory : new T[width_*height_];
		colSkip= height_;
		DEBUG_ONLY( width= width_; )
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
		ASSERT( isValid() && T(block.xend)<=width );
	//	compute begin and end column starts
		T *begin= start+block.y0+colSkip*block.x0
		, *end= start+block.y0+colSkip*block.xend;
	//	fill the columns
	    for (T *it= begin; it!=end; it+= colSkip)
			std::fill( it, it+block.height(), value );
	}
/** \name Low-level functions
 *	mainly for use by image iterators, etc.
 *	@{ */
	/** Shifts the indexing of this matrix - dangerous.
	 *	After calling this, only addressing or more shifts can be done (not checked).
	 *	Also shifts out of the allocated matrix aren't detected */
	void shiftMatrix(I x0,I y0) {
		ASSERT( isValid() );
		start+= x0+y0*colSkip;
		DEBUG_ONLY( width-= x0; );
		ASSERT(width>0);
	}
	/** Returns internal pointer #start */
	T* getStart()			{ return start; }
	/** Returns internal value #colSkip */
	I  getColSkip()	const	{ return colSkip; }
///	@}
}; // Matrix class template



/** MatrixSummer objects store partial sums of a matrix, allowing quick computation
 *	of sum of any rectangle in the matrix */
template<class T,class U,class I=size_t> class MatrixSummer {
public:
	typedef T Result;
	typedef Matrix<const U,I> InpMatrix;
	/** The type of filling the summer - normal sums, sums of squares */
	enum SumType { Values=0, Squares=1 };

private:
	Matrix<T,I> sums; ///< Internal matrix containing precomputed partial sums

public:
	/** Creates an empty summer */
	MatrixSummer() {}
	/** Creates a summer filled from a matrix */
	MatrixSummer( InpMatrix m, SumType sumType, I width, I height )
		{ fill(m,sumType,width,height); }
	/** Frees the resources */
	~MatrixSummer()
		{ sums.free(); }

	/** Only empty objects are allowed to be copied (assertion) */
	MatrixSummer( const MatrixSummer &DEBUG_ONLY(other) )
		{ ASSERT( !other.isValid() ); }
	/** Only empty objects are allowed to be assigned (assertion) */
	MatrixSummer& operator=( const MatrixSummer &DEBUG_ONLY(other) )
		{ return ASSERT( !other.isValid() && !isValid() ), *this; }

	/** Returns whether the object is filled with data */
	bool isValid() const
		{ return sums.isValid(); }
	/** Clears the object */
	void invalidate()
		{ sums.free(); };

	/** Prepares object to make sums for a matrix. If the summer has already been used before,
	 *	the method assumes it was for a matrix of the same size */
	void fill(InpMatrix m,SumType sumType,I width,I height,I x0=0,I y0=0) {
		ASSERT( m.isValid() && (sumType==Values || sumType==Squares) );

		m.shiftMatrix(x0,y0);

		if ( !sums.isValid() )
			sums.allocate(width+1,height+1);

		I yEnd= height+y0;
	//	fill the edges with zeroes
		for (I i=0; i<=width; ++i)
			sums[i][0]= 0;
		for (I j=1; j<=height; ++j)
			sums[0][j]= 0;
	//	acummulate in the y-growing direction
		if (sumType==Values)
			for (I i=1; i<=width; ++i)
				for (I j=1; j<=yEnd; ++j)
					sums[i][j]= sums[i][j-1]+m[i-1][j-1];
		else // sumType==Squares
			for (I i=1; i<=width; ++i)
				for (I j=1+y0; j<=yEnd; ++j)
					sums[i][j]= sums[i][j-1]+sqr<Result>(m[i-1][j-1]);
	//	acummulate in the x-growing direction
		for (I i=2; i<=width; ++i)
			for (I j=1+y0; j<=yEnd; ++j)
				sums[i][j]+= sums[i-1][j];
	}

	/** Computes the sum of a rectangle (in constant time) */
	Result getSum(I x0,I y0,I xend,I yend) const {
		ASSERT( sums.isValid() && xend>x0 && yend>y0 ); /*x0>=0 && y0>=0 && xend>=0 && yend>=0 &&*/
		return sums[xend][yend] -sums[x0][yend] -sums[xend][y0] +sums[x0][y0];
	}

};

namespace MatrixWalkers {

	/** Performs manipulations with rotation codes 0..7 (dihedral group of order eight) */
	struct Rotation {
		/** Asserts the parameter is within 0..7 */
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
	};

	/** Checked iterator for a rectangle in a matrix, no rotation */
	template<class T,class I=size_t> class Checked {
	public:
		typedef Matrix<T,I> TMatrix;
	protected:
		I colSkip			///  offset of the "next row"
		, height;			///< height of the iterated rectangle
		T *col				///  pointer the current column
		, *colEnd			///  pointer to the end column
		, *elem				///  pointer to the current element
		, *elemsEnd;		///< pointer to the end element of the current column

	public:
		Checked( TMatrix pixels, const Block &block )
		: colSkip	( pixels.getColSkip() )
		, height	( block.height() )
		, col		( pixels.getStart()+block.y0+colSkip*block.x0 )
		, colEnd	( pixels.getStart()+block.y0+colSkip*block.xend ) {
			DEBUG_ONLY( elem= elemsEnd= 0; )
			ASSERT( block.xend>block.x0 && block.yend>block.y0 );
		}

		bool outerCond()	{ return col!=colEnd; }
		void outerStep()	{ col+= colSkip; }

		void innerInit()	{ elem= col; elemsEnd= col+height; }
		bool innerCond()	{ return elem!=elemsEnd; }
		void innerStep()	{ ++elem; }

		T& get() 			{ return *elem; }
	}; // Checked class template

	/** Checked iterator for a whole QImage; no rotation, but always transposed */
	template<class T,class U> struct CheckedImage {
		typedef T QImage;
		typedef U QRgb;
	protected:
		QImage &img;	///< reference to the image
		int lineIndex;	///< index of the current line
		QRgb *line		///  pointer to the current pixel
		, *lineEnd;		///< pointer to the end of the line

	public:
		CheckedImage(QImage &image)
		: img(image), lineIndex(0)
			{ DEBUG_ONLY(line=lineEnd=0;) }

		bool outerCond()	{ return lineIndex<img.height(); }
		void outerStep()	{ ++lineIndex; }

		void innerInit()	{ line= (QRgb*)img.scanLine(lineIndex); lineEnd= line+img.width(); }
		bool innerCond()	{ return line!=lineEnd; }
		void innerStep()	{ ++line; }

		QRgb& get()			{ return *line; }
	};


	/** Base structure for walkers changing 'x' in the outer and 'y' in the inner loop */
	template<class T,class I=size_t> struct RotBase {
	public:
		typedef Matrix<T,I> MType;
	protected:
		const I colSkip;	///< the number of elements to skip to get on the next column
		T *list				///  points to the beginning of the current column/row
		, *elem;			///< points to the current element

	public:
		RotBase( MType matrix, I x0, I y0 )
		: colSkip( matrix.getColSkip() ), list( matrix.getStart()+y0+colSkip*x0 )
			{ DEBUG_ONLY( elem= 0; ) }

		void innerInit()	{ elem= list; }
		T& get()			{ return *elem; }
	}; // RotBase class template

	#define ROTBASE_INHERIT \
		using RotBase<T,I>::colSkip; \
		using RotBase<T,I>::list; \
		using RotBase<T,I>::elem; \
		typedef Matrix<T,I> MType;


	/** No rotation: x->, y-> */
	template<class T,class I> struct Rotation_0: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_0( MType matrix, const Block &block )
		: RotBase<T,I>( matrix, block.x0, block.y0 ) {}

		void outerStep() { list+= colSkip; }
		void innerStep() { ++elem; }
	};

	/** Rotated 90deg. cw., transposed: x<-, y-> */
	template<class T,class I> struct Rotation_1_T: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_1_T( MType matrix, const Block &block )
		: RotBase<T,I>( matrix, block.xend-1, block.y0 ) {}

		void outerStep() { list-= colSkip; }
		void innerStep() { ++elem; }
	};

	/** Rotated 180deg. cw.: x<-, y<- */
	template<class T,class I> struct Rotation_2: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_2( MType matrix, const Block &block )
		: RotBase<T,I>( matrix, block.xend-1, block.yend-1 ) {}

		void outerStep() { list-= colSkip; }
		void innerStep() { --elem; }
	};

	/** Rotated 270deg. cw., transposed: x->, y<- */
	template<class T,class I> struct Rotation_3_T: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_3_T( MType matrix, const Block &block )
		: RotBase<T,I>( matrix, block.x0, block.yend-1 ) {}

		void outerStep() { list+= colSkip; }
		void innerStep() { --elem; }
	};

	/** No rotation, transposed: y->, x-> */
	template<class T,class I> struct Rotation_0_T: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_0_T( MType matrix, const Block &block )
		: RotBase<T,I>( matrix, block.x0, block.y0 ) {}

		void outerStep() { ++list; }
		void innerStep() { elem+= colSkip; }
	};

	/** Rotated 90deg. cw.: y->, x<- */
	template<class T,class I> struct Rotation_1: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_1( MType matrix, const Block &block )
		: RotBase<T,I>( matrix, block.xend-1, block.y0 ) {}

		void outerStep() { ++list; }
		void innerStep() { elem-= colSkip; }
	};

	/** Rotated 180deg. cw., transposed: y<-, x<- */
	template<class T,class I> struct Rotation_2_T: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_2_T( MType matrix, const Block &block )
		: RotBase<T,I>( matrix, block.xend-1, block.yend-1 ) {}

		void outerStep() { --list; }
		void innerStep() { elem-= colSkip; }
	};

	/** Rotated 270deg. cw.: y<-, x-> */
	template<class T,class I> struct Rotation_3: public RotBase<T,I> { ROTBASE_INHERIT
		Rotation_3( MType matrix, const Block &block )
		: RotBase<T,I>( matrix, block.x0, block.yend-1 ) {}

		void outerStep() { --list; }
		void innerStep() { elem+= colSkip; }
	};

	//#undef ROTBASE_INHERIT

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

	template<class Check,class U,class Operator>// inline
	Operator walkOperateCheckRotate( Check checked, Operator oper
	, Matrix<U> pixels2, const Block &block2, char rotation) {
		typedef size_t I; //XXX
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

	struct OperatorBase {
		typedef MTypes::SReal SReal;

		void innerEnd() {}
	};

	template<class CReal> struct RDSummer: public OperatorBase {
		CReal totalSum, lineSum;

		RDSummer()
		: totalSum(0), lineSum(0) {}
		void operator()(const SReal &num1,const SReal& num2)
			{ lineSum+= CReal(num1) * CReal(num2); }
		void innerEnd()
			{ totalSum+= lineSum; lineSum= 0; }
		CReal result()
			{ ASSERT(!lineSum); return totalSum; }
	};

	template<class T> struct AddMulCopy: public OperatorBase {
		const T toAdd, toMul;

		AddMulCopy(T add,T mul)
		: toAdd(add), toMul(mul) {}

		template<class R1,class R2> void operator()(R1 &res,R2 f) const
			{ res= (f+toAdd)*toMul; }
	};

	template<class T> struct MulAddCopyChecked: public OperatorBase {
		const T toMul, toAdd, min, max;

		MulAddCopyChecked(T mul,T add,T minVal,T maxVal)
		: toMul(mul), toAdd(add), min(minVal), max(maxVal) {}
		template<class R1,class R2> void operator()(R1 &res,R2 f) const
			{ res= checkBoundsFunc( min, f*toMul+toAdd, max ); }
	};
}

#endif // MATRIXUTIL_HEADER_
