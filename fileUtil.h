#ifndef FILEUTIL_HEADER_
#define FILEUTIL_HEADER_

#include "headers.h"

/** Stores a value in a stream, specialized for various types (portably) */
template <class T> inline void put(std::ostream &os,T i);
/** Reconstructs a value from a stream, see ::put function */
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
	Uint16 res= get<Uchar>(is);
	return res*256+get<Uchar>(is);
}

template<> inline void put(std::ostream &os,Uint32 i) {
	put<Uchar>(os,i/(256*256*256));
    put<Uchar>(os,i/(256*256)%256);
    put<Uchar>(os,i/256%256);
    put<Uchar>(os,i%256);
}
template<> inline Uint32 get(std::istream &is) {
	Uint32 res= 0;
	for (int i=0; i<4; ++i)
		res= res*256+get<Uchar>(is);
	return res;
}

template<> inline void put(std::ostream &os,float f) {
	ASSERT( sizeof(float)==4 );
	os.write( (const char*)&f, sizeof(float) );
}
template<> inline float get(std::istream &is) {
	ASSERT( sizeof(float)==4 );
	float result;
	is.read( (char*)&result, sizeof(float) );
	return result;
}

/** Converts a floating-point value from interval [0,1] to a bit sequence */
inline int float01ToBits(Real f,int bitCount) {
	int result= (int)std::ldexp(f,bitCount);
	return result==powers[bitCount] ? result-1 : result;
}
/** Converts a bit sequnce to a floating-point value, the inverse to ::float01ToBits function */
inline Real float01FromBits(int bits,int bitCount) {
	return std::ldexp( Real(bits)+Real(0.5), -bitCount);
}


/** Stream bit-writer - automated buffer for writing single bits */
class BitWriter {
	/** Buffered bits, number of buffered bits */
	int buffer, bufbits;
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
		ASSERT( bits>=0 && 0<=val && val<powers[bits] );
		if (!bits)
			return;
		buffer+= powers[bufbits]*val;
		bufbits+= bits;
		while (bufbits>=8) {
			bufbits-= 8;
			os.put(buffer%256);
			buffer/= 256;
		}
	}
	/** Flushes the buffer - sends it to the stream */
	void flush() {
		if (bufbits)
			os.put(buffer);
		buffer= bufbits= 0;
	}
};

/** Stream bit-reader - automated buffer for reading single bits */
class BitReader {
	/** Buffered bits, number of buffered bits */
	int buffer, bufbits;
	/** Used input (byte)stream */
	std::istream &is;
public:
	/** Constructor just associates the object with the given stream */
	BitReader(std::istream &stream)
	: buffer(0), bufbits(0), is(stream) {}
	/** Reads bits */
	int getBits(int bits) {
		ASSERT(bits>=0);
		if (!bits)
			return 0;
		while (bufbits<bits) {
			buffer+= powers[bufbits]*Uchar(is.get());
			bufbits+= 8;
		}
		int result= buffer%powers[bits];
		buffer/= powers[bits];
		bufbits-= bits;
		return result;
	}
	/** Clears buffer */
	void flush() {
		buffer= bufbits= 0;
	}
};

/** Reads a whole file into std::string (\p result), returns \p true on success */
inline bool file2string(const char *name,std::string &result) {
	using namespace std;
	ifstream file( name, ios_base::binary|ios_base::in );
	if (!file.good())
		return false;
	
	file.seekg(0,ios::end);
 	int length= file.tellg();
 	if (!length)
 		return false;
 	
 	string res;
 	res.resize(length);
 	file.seekg(0,ios::beg);
 	file.read(&res[0],length);
 	if (file.gcount()!=length)
 		return false;
 	swap(res,result);
 	return true;
}

#endif // FILEUTIL_HEADER_
