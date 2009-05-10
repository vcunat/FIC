#ifndef COLORMODEL_HEADER_
#define COLORMODEL_HEADER_

#include "../interfaces.h"
#include "../fileUtil.h"

#include <QColor>

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
	static const Real RGBCoeffs[][4]	///  RGB color coefficients
	, YCbCrCoeffs[][4];					///< YCbCr color coefficients

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


/** Computes the amount of a color (coefficients as a parameter) in RGB \relates MColorModel */
inline Real getColor( QRgb color, const Real *koefs ) {
    return koefs[3] + std::ldexp( Real(0.5) + qRed(color)*Real(koefs[0])
    	+ qGreen(color)*Real(koefs[1]) + qBlue(color)*Real(koefs[2]), -8 );
}
/** Converts color from a model (coefficients as a parameter) to RGB \relates MColorModel */
inline QRgb getColor( const Real (*coeffs)[4], const Real *planes ) {
	Real rgb[3]= {0,0,0};
	for (int i=0; i<3; ++i) {
		const Real *cLine= coeffs[i];
		Real col= planes[i]+cLine[3];
		for (int c=0; c<3; ++c)
			rgb[c]+= col*cLine[c];
	}
	typedef Float2int<8,Real> Conv;
	return qRgb( Conv::convertCheck(rgb[0]), Conv::convertCheck(rgb[1]), Conv::convertCheck(rgb[2]) );
}
/** Computes the gray level of a QRgb color in 0-255 interval \relates MColorModel */
inline int getGray(QRgb color) {
	return Float2int<8,Real>::convert( getColor( color, MColorModel::YCbCrCoeffs[0] ) );
}

#endif
