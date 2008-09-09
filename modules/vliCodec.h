#ifndef VLICODEC_HEADER_
#define VLICODEC_HEADER_

#include "../interfaces.h"
#include "../fileUtil.h"

class VLI;

/** Variable-length-integer codec optimized for encoding little-changing sequences */
class MDifferentialVLICodec: public IIntCodec {

	DECLARE_M_cloning_name_desc( MDifferentialVLICodec, "Differential VLI"
	, "Variable-length-integer differential encoder with adjustable distribution" )

	DECLARE_M_settings_type({
		type:	IntLog2,
		data: {	i:{0,8} },
		label:	"First level symbols",
		desc:	"The number of possibilities\n"
				"that will occupy least space"
	})

	DECLARE_M_settings_default(
		1 //	first level symbols
	)

	/** Indices for settings */
	enum Settings { VLIExponent };
private:
	int possib		/// the number of possibilities set by #setPossibilities
	, lastSymbol;	///< the last encoded symbol (#possib/2 at the beginning)
public:
/**	\name IIntCodec interface
 *	@{ */
	void setPossibilities(int possibilities) {
		possib= possibilities;
		lastSymbol= possib/2;
	}
	void encode(std::vector<int> &data,std::ostream &file);
	void decode(std::istream &file,int count,std::vector<int> &data);

	void writeSettings(std::ostream &file)
		{ put<Uchar>( file, settingsInt(VLIExponent) ); }
	void readSettings(std::istream &file)
		{ settingsInt(VLIExponent)= get<Uchar>(file); }
///	@}
};

#endif // VLICODEC_HEADER_
