#ifndef MATRIXUTIL_HEADER_
#define MATRIXUTIL_HEADER_

////    Matrix templates
/** Initializes column pointers so a linear field acts like a matrix */
template<class T> inline void
initMatrixPointers(int width,int height,T *linear,T **matrix) {
    for (T **mend=matrix+width; matrix!=mend; ++matrix,linear+=height)
        *matrix= linear;
}
/** Allocates a new matrix of type T with given width and height */
template<class T> T** newMatrix(int width,int height) {
	assert( width>0 && height>0 );
    T **result= new T*[width];
    T *linear= new T[width*height];
    initMatrixPointers(width,height,linear,result);
    return result;
}
/** Releases a matrix of type T (zero is ignored) */
template<class T> inline void delMatrix(T **m) {
    if (!m)
        return;
	delete[] m[0];
    delete[] m;
}
/** Fills a submatrix with a value */
template<class T,class Block> void fillSubMatrix(T **m,const Block &block,T value) {
	T **mend= m+block.xend;
    for (m+=block.x0; m!=mend; ++m)
		std::fill( *m+block.y0, *m+block.yend, value );
}

/** MatrixSummer objects store partial sums of a matrix, allowing quick computation
 *	of sum of any rectangle in the matrix */
template<class T,class U,class I> class MatrixSummer {
public:
	typedef T result_type;
	/** The type of filling the summer - normal sums, sums of squares */
	enum SumType { Values=0, Squares=1 };

private:
	result_type **sums; ///< Internal matrix containing precomputed partial sums

public:
	/** Creates an empty summer */
	MatrixSummer()
	: sums(0) {}
	/** Creates a summer filled from a matrix */
	MatrixSummer( const U **m, SumType sumType, I width, I height )
	: sums(0)
		{ fill(m,sumType,width,height); }
	/** Frees the resources */
	~MatrixSummer()
		{ free(); }

	/** Only empty objects are allowed to be copied (assertion) */
	MatrixSummer( const MatrixSummer &DEBUG_ONLY(other) )
	: sums(0)
		{ assert(!other.sums); }
	/** Only empty objects are allowed to be assigned (assertion) */
	MatrixSummer& operator=( const MatrixSummer &DEBUG_ONLY(other) )
		{ return assert( !other.sums && !sums ), *this; }

	/** Returns whether the object is filled with data */
	bool isValid() const
		{ return sums; }
	/** Clears the object */
	void invalidate()
		{ free(); };

	/** Prepares object to make sums for a matrix. If the summer has already been used before,
	 *	the method assumes it was for a matrix with the same size */
	void fill(const U **m,SumType sumType,I width,I height,I x0=0,I y0=0) {
		assert( m && (sumType==Values || sumType==Squares) );
		m+= x0;
		if (!sums)
			allocate(width,height);
		I yEnd= height+y0;
	//	fill the edges with zeroes
		for (I i=0; i<=width; ++i)
			sums[i][0+y0]= 0;
		for (I j=1+y0; j<=yEnd; ++j)
			sums[0][j]= 0;
	//	acummulate in the y-growing direction
		if (sumType==Values)
			for (I i=1; i<=width; ++i)
				for (I j=1+y0; j<=yEnd; ++j)
					sums[i][j]= sums[i][j-1]+m[i-1][j-1];
		else // sumType==Squares
			for (I i=1; i<=width; ++i)
				for (I j=1+y0; j<=yEnd; ++j)
					sums[i][j]= sums[i][j-1]+sqr<result_type>(m[i-1][j-1]);
	//	acummulate in the x-growing direction
		for (I i=2; i<=width; ++i)
			for (I j=1+y0; j<=yEnd; ++j)
				sums[i][j]+= sums[i-1][j];
	}
	/** Just a shortcut for filling from a non-const matrix */
	void fill(U **m,SumType sumType,I width,I height,I x0=0,I y0=0)
		{ fill( bogoCast(m), sumType, width, height, x0, y0 ); }

	/** Computes the sum of a rectangle (in constant time) */
	result_type getSum(I x0,I y0,I xend,I yend) const {
		assert( sums && x0>=0 && y0>=0 && xend>=0 && yend>=0 && xend>x0 && yend>y0 );
		return sums[xend][yend] -sums[x0][yend] -sums[xend][y0] +sums[x0][y0];
	}
private:
	/** Allocates internal matrix */
	void allocate(I width,I height) {
		assert( width>0 && height>0 );
		delMatrix(sums);
		sums= newMatrix<T>(width+1,height+1);
	}
	/** Frees all resources */
	void free() {
		delMatrix(sums);
		sums= 0;
	}
};

namespace MatrixWalkers {
	using MTypes::Block;
	
	/** Performs manipulations with rotation codes 0..7 (dihedral group of order eight) */
	struct Rotation {
		/** Asserts the parameter is within 0..7 */
		static void check(int DEBUG_ONLY(r)) {
			assert( 0<=r && r<8 );
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
	template<class T> class Checked {	
	protected:
		T **list			///  pointer the current column
		, **listsEnd;		///< pointer to the end column
		size_t addElem		///  offset of the first row from the top
		, addElemsEnd;		///< offset of the end row from the top
		T *elem				///  pointer to the current element
		, *elemsEnd;		///< pointer to the end element of the current column
		
	public:
		Checked( T **pixels, const Block &block )
		: list(pixels+block.x0), listsEnd(pixels+block.xend)
		, addElem(block.y0), addElemsEnd(block.yend) { 
			DEBUG_ONLY( elem=elemsEnd=0; ) 
			assert( block.xend>block.x0 && block.yend>block.y0 ); 
		}

		bool outerCond()	{ return list!=listsEnd; }
		void outerStep()	{ ++list; }
		
		void innerInit()	{ elem= (*list)+addElem; elemsEnd= (*list)+addElemsEnd; }
		bool innerCond()	{ return elem!=elemsEnd; }
		void innerStep()	{ ++elem; }
		
		T& get() 			{ return *elem; }
	};
	
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

	namespace NOSPACE {
		/** Base structure for walkers changing 'x' in the outer and 'y' in the inner loop */
		template<class T> class RBase_it {
		protected:
			T **list; ///< points to the beginning of the current column
			const size_t addElem; ///< position of the first-to-be-walked elements from *#list[i]
			T *elem; ///< points to the current element

		public:
			RBase_it( T **listsBegin, size_t addElem_ )
			: list(listsBegin), addElem(addElem_)
				{ DEBUG_ONLY(elem=0;) }

			void innerInit()	{ elem= *list+addElem; }
			T& get()			{ return *elem; }
		};
	}
	
	/** No rotation: x->, y-> */
	template<class T> struct Rotation_0: public RBase_it<T> {
		using RBase_it<T>::list; using RBase_it<T>::elem;

		Rotation_0( T **pixels, const Block &block )
		: RBase_it<T>( pixels+block.x0, block.y0 ) {}

		void outerStep() { ++list; }
		void innerStep() { ++elem; }
	};
	
	/** Rotated 90deg. cw., transposed: x<-, y-> */
	template<class T> struct Rotation_1_T: public RBase_it<T> {
		using RBase_it<T>::list; using RBase_it<T>::elem;

		Rotation_1_T( T **pixels, const Block &block )
		: RBase_it<T>( pixels+block.xend-1, block.y0 ) {}

		void outerStep() { --list; }
		void innerStep() { ++elem; }
	};
	
	/** Rotated 180deg. cw.: x<-, y<- */
	template<class T> struct Rotation_2: public RBase_it<T> {
		using RBase_it<T>::list; using RBase_it<T>::elem;

		Rotation_2( T **pixels, const Block &block )
		: RBase_it<T>( pixels+block.xend-1, block.yend-1 ) {}

		void outerStep() { --list; }
		void innerStep() { --elem; }
	};
	
	/** Rotated 270deg. cw., transposed: x->, y<- */
	template<class T> struct Rotation_3_T: public RBase_it<T> {
		using RBase_it<T>::list; using RBase_it<T>::elem;

		Rotation_3_T( T **pixels, const Block &block )
		: RBase_it<T>( pixels+block.x0, block.yend-1 ) {}

		void outerStep() { ++list; }
		void innerStep() { --elem; }
	};
	
	
	namespace NOSPACE {	
		/** Base structure for walkers changing 'y' in the outer and 'x' in the inner loop */
		template<class T> class RBase_ix {
		protected:
			T *lineBegin; ///< points to the first element of the current line (first to be walked)
			const size_t elemStep; ///< the number of elements to get the next column (positive)
			T *elem; ///< points to the current element

		public:
			RBase_ix( T **pixels, size_t x_start, size_t y_start )
			: lineBegin(pixels[x_start]+y_start), elemStep(pixels[1]-pixels[0]) {
				DEBUG_ONLY(elem=0;)
				assert( size_t(pixels[2]-pixels[1]) == elemStep );
			}

			void innerInit()	{ elem= lineBegin; }
			T& get()			{ return *elem; }
		};
	}

	/** No rotation, transposed: y->, x-> */
	template<class T> struct Rotation_0_T: public RBase_ix<T> {
		using RBase_ix<T>::lineBegin; using RBase_ix<T>::elemStep; using RBase_ix<T>::elem;

		Rotation_0_T( T **pixels, const Block &block )
		: RBase_ix<T>( pixels, block.x0, block.y0 ) {}

		void outerStep() { ++lineBegin; }
		void innerStep() { elem+= elemStep; }
	};

	/** Rotated 90deg. cw.: y->, x<- */
	template<class T> struct Rotation_1: public RBase_ix<T> {
		using RBase_ix<T>::lineBegin; using RBase_ix<T>::elemStep; using RBase_ix<T>::elem;

		Rotation_1( T **pixels, const Block &block )
		: RBase_ix<T>( pixels, block.xend-1, block.y0 ) {}

		void outerStep() { ++lineBegin; }
		void innerStep() { elem-= elemStep; }
	};

	/** Rotated 180deg. cw., transposed: y<-, x<- */
	template<class T> struct Rotation_2_T: public RBase_ix<T> {
		using RBase_ix<T>::lineBegin; using RBase_ix<T>::elemStep; using RBase_ix<T>::elem;

		Rotation_2_T(T **pixels,const Block &block)
		: RBase_ix<T>( pixels, block.xend-1, block.yend-1 ) {}

		void outerStep() { --lineBegin; }
		void innerStep() { elem-= elemStep; }
	};

	/** Rotated 270deg. cw.: y<-, x-> */
	template<class T> struct Rotation_3: public RBase_ix<T> {
		using RBase_ix<T>::lineBegin; using RBase_ix<T>::elemStep; using RBase_ix<T>::elem;

		Rotation_3( T **pixels, const Block &block )
		: RBase_ix<T>( pixels, block.x0, block.yend-1 ) {}

		void outerStep() { --lineBegin; }
		void innerStep() { elem+= elemStep; }
	};
	

	template < class Check, class Unchecked, class Operator >
	Operator walkOperate( Check checked, Unchecked unchecked, Operator oper ) {
	//	outer cycle start - to be always run at least once
		assert( checked.outerCond() );
		do {
		//	inner initialization
			checked.innerInit();
			unchecked.innerInit();
		// inner cycle start - to be always run at least once	
			assert( checked.innerCond() ); 
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
	, U **pixels2, const Block &block2, char rotation ) {
		switch (rotation) {
			case 0: return walkOperate( checked, Rotation_0  <U>(pixels2,block2) , oper );
			case 1: return walkOperate( checked, Rotation_0_T<U>(pixels2,block2) , oper );
			case 2: return walkOperate( checked, Rotation_1  <U>(pixels2,block2) , oper );
			case 3: return walkOperate( checked, Rotation_1_T<U>(pixels2,block2) , oper );
			case 4: return walkOperate( checked, Rotation_2  <U>(pixels2,block2) , oper );
			case 5: return walkOperate( checked, Rotation_2_T<U>(pixels2,block2) , oper );
			case 6: return walkOperate( checked, Rotation_3  <U>(pixels2,block2) , oper );
			case 7: return walkOperate( checked, Rotation_3_T<U>(pixels2,block2) , oper );
			default: assert(false); return oper;
		}
	}


	template<class Real> struct RDSummer {
		Real totalSum, lineSum;

		RDSummer()
		: totalSum(0), lineSum(0) {}
		void operator()(const float &num1,const float& num2)
			{ lineSum+= Real(num1) * Real(num2); }
		void innerEnd()
			{ totalSum+= lineSum; lineSum= 0; }
		Real result()
			{ assert(!lineSum); return totalSum; }
	};

	template<class T> struct AddMulCopy {
		const T toAdd, toMul;

		AddMulCopy(T add,T mul)
		: toAdd(add), toMul(mul) {}

		void operator()(float &res,float f) const
			{ res= (f+toAdd)*toMul; }
	};

	template<class T> struct MulAddCopyChecked {
		const T toMul, toAdd, min, max;

		MulAddCopyChecked(T mul,T add,T minVal,T maxVal)
		: toMul(mul), toAdd(add), min(minVal), max(maxVal) {}
		void operator()(float &res,float f) const
			{ res= checkBoundsFunc( min, f*toMul+toAdd, max ); }
		void innerEnd() const {};
	};
}

#endif // MATRIXUTIL_HEADER_
