#include "vliCodec.h"

using namespace std;

/** Variable Length Integer - smaller integers are stored in fewer bits */
class VLI {
	/** Simple routine converting from all integers to non-negative integers (interleaving) */
	static int toPositive(int value)
		{ return value>=0 ? value*2 : value*(-2)-1 ; }
	/** Simple converting routine, opposite to #toPositive */
	static int fromPositive(int value) {
		int res= value/2;
		return value%2 ? -res-1 : res ;
	}
	/** Returns the level of a non-negative integer depending on the setting of expAdd */
	static int getLevel(int posValue,int expAdd)
		{ return log2ceil(posValue+powers[expAdd]+1) -expAdd -1; }

	/** Returns the lowest number of a level (works with non-negative integers only) */
    int getBase(int level) const {
    	assert( level>=0 && level<=maxLevel );
    	return powers[level+expAdd] -powers[expAdd];
	}
public:
	/** Data type for storing VLI */
    struct Type {
        int level, data;
    };

 /** VLI's settings (they're const, so it doesn't matter they're public) */
    const int expAdd, maxLevel
    #ifndef NDEBUG
    , possib
    #endif
    , maxLevel_Bits; ///< The number of bits needed to encode residues in the highest level

public:
	/** Only initializes the settings */
    VLI(int possibilities,int exponentAddition=1)
    : expAdd(exponentAddition), maxLevel(getLevel( possibilities-1 , exponentAddition ))
    #ifndef NDEBUG
    , possib(possibilities)
    #endif
    , maxLevel_Bits(log2ceil( possibilities-getBase(maxLevel) ))
		{ assert( possibilities>0 && exponentAddition>=0 ); }
	/** Gets the number of bits needed to encode the residues of numbers at a given level */
    int bitsForLevel(int level) const {
    	assert( level>=0 && level<=maxLevel );
    	return level==maxLevel ? maxLevel_Bits : level+expAdd ;
	}

	/** Converts from signed integer to VLI */
    Type toVLI(int value) const {
		value= toPositive(value);
		assert( value>=0 && value<possib );
		Type result;
		result.level= getLevel(value,expAdd);
		result.data= value -getBase(result.level);
		return result;
	}
	/** Converts from VLI to signed integer */
	int fromVLI(int level,int data) const
		{ return fromPositive( getBase(level) +data ); }
	int fromVLI(Type vli) const
		{ return fromVLI(vli.level,vli.data); }
};



void MDifferentialVLICodec::encode(vector<int> &data,ostream &file) {
//	encode data to VLI representation
	VLI vli( possib, settingsInt(VLIExponent) );
	vector<VLI::Type> vlis;
	vlis.reserve( data.size() );
	int posHalf= possib/2;
	for (vector<int>::iterator it=data.begin(); it!=data.end(); ++it) {
		int diff= *it -lastSymbol;
		if (diff < -posHalf)
			diff+= possib; else
		if (diff >= posHalf)
			diff-= possib;
		
		lastSymbol= *it;
		vlis.push_back( vli.toVLI(diff) );
	}
//	send the vli's levels and data to the stream
	BitWriter out(file);
	int bits= log2ceil(vli.maxLevel+1);
	for (vector<VLI::Type>::iterator it=vlis.begin(); it!=vlis.end(); ++it)
		out.putBits( it->level, bits );
	for (vector<VLI::Type>::iterator it=vlis.begin(); it!=vlis.end(); ++it)
		out.putBits( it->data, vli.bitsForLevel(it->level) );
}
DEBUG_CONTAINER(vector<VLI::Type>)

void MDifferentialVLICodec::decode(istream &file,int count,vector<int> &data) {
	VLI vli( possib, settingsInt(VLIExponent) );
	vector<int> levels;
	levels.resize(count);
//	read the vli levels
	BitReader in(file);
	int bits= log2ceil(vli.maxLevel+1);
	for (int i=0; i<count; ++i)
		levels[i]= in.getBits(bits);
//	read the vli data and replace levels[i] with the result
	for (int i=0; i<count; ++i) {
		int level= levels[i];
		lastSymbol+= vli.fromVLI( level, in.getBits(vli.bitsForLevel(level)) );

		if (lastSymbol<0)
			lastSymbol+= possib; else
		if (lastSymbol>=possib)
			lastSymbol-= possib;
	//	now let levels[i] mean the resulting data[i]
		levels[i]= lastSymbol;
	}
//	return the result hidden in the levels vector
	swap(levels,data);
}
