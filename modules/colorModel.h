#ifndef COLORMODEL_HEADER_
#define COLORMODEL_HEADER_

#include "../headers.h"
#include "../fileUtil.h"

/// \ingroup modules
/** Simple color transformer for affine models. It currently supports RGB and YCbCr
 *	color models and allows to se quality multipliers for individual color channels. */
class MColorModel: public IColorTransformer {

	DECLARE_TypeInfo( MColorModel, "Color models"
	, "Splits image into color planes using some <b>color model</b> (RGB or YCbCr)"
	, {
		label:	"Color model",
		desc:	"The color model that will be used to encode the images",
		type:	settingCombo("RGB\nYCbCr",1)
	} 
	, {
		label:	"Quality multiplier for R/Y channel",
		desc:	"The real encoding quality for Red/Y channel\n"
				"will be multiplied by this number",
		type:	settingFloat(0,1,1)
	} 
	, {
		label:	"Quality multiplier for G/Cb channel",
		desc:	"The real encoding quality for Green/Cb channel\n"
				"will be multiplied by this number",
		type:	settingFloat(0,0.5,1)
	} 
	, {
		label:	"Quality multiplier for B/Cr channel",
		desc:	"The real encoding quality for Blue/Cr channel\n"
				"will be multiplied by this number",
		type:	settingFloat(0,0.5,1)
	} 
	);

protected:
	/** Indices for settings */
	enum Settings { ColorModel, QualityMul1, QualityMul2, QualityMul3 };
//	Settings-retrieval methods
	int numOfModels() { return 1+countEOLs( info().setType[ColorModel].type.data.text ); }
	float qualityMul(int channel) {
		ASSERT(channel>=0 && channel<3);
		return settings[QualityMul1+channel].val.f;
	}
	
protected:
	PlaneList ownedPlanes; ///< the list of color planes, owned by the module

protected:
//	Construction and destruction
	/** Just frees ::ownedPlanes */
	~MColorModel() {
		for (PlaneList::iterator it=ownedPlanes.begin(); it!=ownedPlanes.end(); ++it) {
			it->pixels.free();
			delete it->settings;
		}
	}
	
public:
/** \name IColorTransformer interface
 *	@{ */
	PlaneList image2planes(const QImage &toEncode,const PlaneSettings &prototype);
	QImage planes2image();

	void writeData(std::ostream &file)
		{ put<Uchar>( file, settingsInt(ColorModel) ); }
	PlaneList readData(std::istream &file,const PlaneSettings &prototype);
///	@}
protected:
	/** Creates a list of planes according to \p prototype,
	 *	makes new matrices and adjusts encoding parameters */
	PlaneList createPlanes(IRoot::Mode mode,const PlaneSettings &prototype);
};

#endif
