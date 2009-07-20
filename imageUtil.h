#ifndef IMAGEUTIL_HEADER_
#define IMAGEUTIL_HEADER_

#include "headers.h"

#include <QColor>

namespace Color {
	extern const Real RGBCoeffs[][4]	///  RGB color coefficients
		, YCbCrCoeffs[][4];				///< YCbCr color coefficients
	
	/** Computes PSNRs between two images for gray, red, green and blue */
	std::vector<Real> getPSNR(const QImage &a,const QImage &b);
	
	/** Computes the amount of a color (coefficients as a parameter) in RGB */
	inline Real getColor( QRgb color, const Real *koefs ) {
	    return koefs[3] + std::ldexp( Real(0.5) + qRed(color)*Real(koefs[0])
	    	+ qGreen(color)*Real(koefs[1]) + qBlue(color)*Real(koefs[2]), -8 );
	}
	
	/** Converts color from a model (coefficients as a parameter) to RGB */
	inline QRgb getColor( const Real (*coeffs)[4], const Real *planes ) {
		Real rgb[3]= {0,0,0};
		for (int i=0; i<3; ++i) {
			const Real *cLine= coeffs[i];
			Real col= planes[i]+cLine[3];
			for (int c=0; c<3; ++c)
				rgb[c]+= col*cLine[c];
		}
		typedef Float2int<8,Real> Conv;
		return qRgb( Conv::convertCheck(rgb[0]), Conv::convertCheck(rgb[1])
			, Conv::convertCheck(rgb[2]) );
	}
	
	/** Computes the gray level of a QRgb color in 0-255 interval */
	inline int getGray(QRgb color) {
		return Float2int<8,Real>::convert( getColor( color, YCbCrCoeffs[0] ) );
	}
		
} // Color namespace

#endif
