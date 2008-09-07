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
	static const float RGBCoeffs[][4]	/** RGB color coefficients */
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
	/** Creates a list of planes according to \a prototype,
	 *	makes new matrices and adjusts encoding parameters */
	PlaneList createPlanes(IRoot::Mode mode,const Plane &prototype,int width,int height);
};


/** Computes the amount of a color (coefficients as a parameter) in RGB \relates MColorModel */
inline float getColor( QRgb color, const float *koefs ) {
    return koefs[3]+std::ldexp
    ( 0.5+qRed(color)*koefs[0]+qGreen(color)*koefs[1]+qBlue(color)*koefs[2], -8	);
}
/** Converts color from a model (coefficients as a parameter) to RGB \relates MColorModel */
inline QRgb getColor( const float (*coeffs)[4], const float *planes ) {
	float rgb[3]={0,0,0};
	for (int i=0; i<3; ++i) {
		const float *cLine=coeffs[i];
		float col=planes[i]+cLine[3];
		for (int c=0; c<3; ++c)
			rgb[c]+=col*cLine[c];
	}
	return qRgb
	( checkBoundsFunc( 0, (int)std::ldexp(rgb[0],8), 255 )
	, checkBoundsFunc( 0, (int)std::ldexp(rgb[1],8), 255 )
	, checkBoundsFunc( 0, (int)std::ldexp(rgb[2],8), 255 )
	);
}

#endif
