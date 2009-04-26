#ifndef COLORMODEL_HEADER_
#define COLORMODEL_HEADER_

#include "../interfaces.h"
#include "../fileUtil.h"

#include <QColor>

/** Simple color transformer, can use several color models linear to the basic RGB */
class MColorModel: public IColorTransformer {

	DECLARE_TypeInfo( MColorModel, "Color models"
	, "Splits image into color planes using some <b>color model</b> (RGB or YCbCr)"
	, {
		label:	"Color model",
		desc:	"The color model that will be used to encode the images",
		type:	settingCombo("RGB\nYCbCr",1)
	} );

private:
	/** Indices for settings */
	enum Settings { ColorModel };
//	Settings-retrieval methods
	int &colorModel() { return settingsInt(ColorModel); }
	int numOfModels() { return 1+countEOLs( info().setType[ColorModel].type.data.text ); }
	
protected:
	PlaneList ownedPlanes;

//	Construction and destruction
	/* Using auto-generated */
	~MColorModel() {
		for (PlaneList::iterator it=ownedPlanes.begin(); it!=ownedPlanes.end(); ++it) {
			it->pixels.free();
			delete it->settings;
		}
	}
public:
	static const Real RGBCoeffs[][4]	/** RGB color coefficients */
	, YCbCrCoeffs[][4];					/**< YCbCr color coefficients */
public:
/** \name IColorTransformer interface
 *	@{ */
	PlaneList image2planes(const QImage &toEncode,const PlaneSettings &prototype);
	QImage planes2image();

	void writeData(std::ostream &file)
		{ put<Uchar>( file, colorModel() ); }
	PlaneList readData(std::istream &file,const PlaneSettings &prototype);
///	@}
private:
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
inline int getGray(QRgb color) {
	return Float2int<8,Real>::convert( getColor( color, MColorModel::YCbCrCoeffs[0] ) );
}

#endif
