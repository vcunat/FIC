#ifndef FILEUTIL_HEADER_
#define FILEUTIL_HEADER_

#include "headers.h"

template <class T> inline void put(std::ostream &os,T i);
template <class T> inline T get(std::istream &is);



template<> inline void put(std::ostream &os,Uchar i) {
	os.put(i);
}
template<> inline Uchar get(std::istream &is) {
	return is.get();
}

template<> inline void put(std::ostream &os,Uint16 i) {
	put<Uchar>(os,i/256);
    put<Uchar>(os,i%256);
}
template<> inline Uint16 get(std::istream &is) {
	Uint16 res=get<Uchar>(is);
	return res*256+get<Uchar>(is);
}


/** Quantizes f that belongs to 0..possib/2^scale into 0..possib-1 */
inline int quantizeByPower(float f,int scale,int possib) {
	int result=(int)ldexp(f,scale);
	assert( result>=0 && result<=possib );
	return result<possib ? result : --result;
}
/** Performs the opposite to ::quantizeByPower */
inline float dequantizeByPower(int i,int scale,int DEBUG_ONLY(possib)) {
	assert( i>=0 && i<possib );
	return ldexp(i+0.5f,-scale);
}

inline int float01ToBits(float f,int bitCount) {
	int result=(int)std::ldexp(f,bitCount);
	return result==powers[bitCount] ? result-1 : result;
}
inline float float01FromBits(int bits,int bitCount) {
	return std::ldexp( float(bits)+float(0.5), -bitCount);
}


/** Stream bit-writer - automated buffer for writing single bits */
class BitWriter {
	/** Buffered bits, number of buffered bits */
	int buffer,bufbits;
	/** Used output (byte)stream */
	std::ostream &os;
public:
	/** Constructor just associates the object with the given stream */
	BitWriter(std::ostream &stream)
	: buffer(0), bufbits(0), os(stream) {}
	/** Destructor only flushes the buffer */
	~BitWriter()
		{ flush(); }
	/** Puts bits */
	void putBits(int val,int bits) {
		buffer+=powers[bufbits]*val;
		bufbits+=bits;
		while (bufbits>=8) {
			bufbits-=8;
			os.put(buffer%256);
			buffer/=256;
		}
	}
	/** Flushes the buffer - sends it to the stream */
	void flush() {
		if (bufbits)
			os.put(buffer);
		buffer=bufbits=0;
	}
};

/** Stream bit-reader - automated buffer for reading single bits */
class BitReader {
	/** Buffered bits, number of buffered bits */
	int buffer,bufbits;
	/** Used input (byte)stream */
	std::istream &is;
public:
	/** Constructor just associates the object with the given stream */
	BitReader(std::istream &stream)
	: buffer(0), bufbits(0), is(stream) {}
	/** Reads bits */
	int getBits(int bits) {
		while (bufbits<bits) {
			buffer+=powers[bufbits]*Uchar(is.get());
			bufbits+=8;
		}
		int result=buffer%powers[bits];
		buffer/=powers[bits];
		bufbits-=bits;
		return result;
	}
	/** Clears buffer */
	void flush() {
		buffer=bufbits=0;
	}
};

#endif // FILEUTIL_HEADER_
