#ifndef COLORMODEL_HEADER_
#define COLORMODEL_HEADER_

#include "../interfaces.h"

#include <QColor>

/** Simple color transformer, can use several color models linear to the basic RGB */
class MColorModel: public IColorTransformer {

	DECLARE_M_cloning_name_desc( MColorModel, "Color models"
	, "Splits image into color planes using some <b>color model</b> (RGB or YCbCr)" );

	DECLARE_M_settings_type({
		type:	Combo,
		data: {	text: "RGB\nYCbCr" },
		label:	"Color model",
		desc:	"The color model that will be used to encode the images"
	});

	DECLARE_M_settings_default(
		1	// the color-model index
	);
private:
	/** Indices for settings */
	enum Settings { ColorModel }; 
//	Settings-retrieval methods
	int& colorModel()
		{ return settings[ColorModel].i; }
	int numOfModels()
		{ return 1+countEOLs( settingsType()[ColorModel].data.text ); }
protected:
//	Construction and destruction
	/* Using auto-generated */
public:
	static const SReal RGBCoeffs[][4]	/** RGB color coefficients */
	, YCbCrCoeffs[][4];					/**< YCbCr color coefficients */
public:
/** \name IColorTransformer interface
 *	@{ */
	PlaneList image2planes(const QImage &toEncode,const Plane &prototype);
	QImage planes2image(const MatrixList &pixels,int width,int height);

	void writeData(std::ostream &file);
	PlaneList readData(std::istream &file,const Plane &prototype,int width,int height);
///	@}
private:
	/** Creates a list of planes according to \p prototype,
	 *	makes new matrices and adjusts encoding parameters */
	PlaneList createPlanes(IRoot::Mode mode,const Plane &prototype,int width,int height);
};


/** Computes the amount of a color (coefficients as a parameter) in RGB \relates MColorModel */
inline SReal getColor( QRgb color, const SReal *koefs ) {
    return koefs[3] + std::ldexp( Real(0.5) + qRed(color)*Real(koefs[0]) 
    	+ qGreen(color)*Real(koefs[1]) + qBlue(color)*Real(koefs[2]), -8 );
}
/** Converts color from a model (coefficients as a parameter) to RGB \relates MColorModel */
inline QRgb getColor( const SReal (*coeffs)[4], const SReal *planes ) {
	SReal rgb[3]= {0,0,0};
	for (int i=0; i<3; ++i) {
		const SReal *cLine= coeffs[i];
		SReal col= planes[i]+cLine[3];
		for (int c=0; c<3; ++c)
			rgb[c]+= col*cLine[c];
	}
	typedef Float2int<8,Real> Conv;
	return qRgb
	( Conv::convertCheck(rgb[0]), Conv::convertCheck(rgb[1]), Conv::convertCheck(rgb[2]) );
}
inline int getGray(QRgb color) {
	return Float2int<8,Real>::convert( getColor( color, MColorModel::YCbCrCoeffs[0] ) );
}

#endif
