#include "colorModel.h"

#include <QImage>
using namespace std;

const Real
    MColorModel::YCbCrCoeffs[][4]= {
        { 0.299,     0.587,     0.114,    0   },
        {-0.168736, -0.331264,  0.5,      0.5 },
        { 0.5,      -0.418688, -0.081312, 0.5 },

        { 1,         1,         1,        0   },
        { 0,        -0.34414,   1.772,   -0.5 },
        { 1.402,    -0.71414,   0,       -0.5 }
    },
    MColorModel::RGBCoeffs[][4]= {
        {1,0,0,0},{0,1,0,0},{0,0,1,0},
        {1,0,0,0},{0,1,0,0},{0,0,1,0}
    };


MColorModel::PlaneList MColorModel
::image2planes( const QImage &image, const PlaneSettings &prototype ) {
	ASSERT( !image.isNull() && ownedPlanes.empty() );
//	get the dimensions and create the planes
	int width= image.width(), height= image.height();
	ownedPlanes= createPlanes(IRoot::Encode,prototype);
//	get the correct coefficients and plane count
	const Real (*coeffs)[4]= ( settingsInt(ColorModel) ? YCbCrCoeffs : RGBCoeffs);
	int planeCount= ownedPlanes.size();
//	fill pixels in all planes
	for (int i=0; i<planeCount; ++i) {
		SMatrix pixels= ownedPlanes[i].pixels;
	//	fill the pixels in this plane
		for (int y=0; y<height; ++y) {
		//	fill pixels in this line
			const QRgb *line= (QRgb*)image.scanLine(y);
			for (int x=0; x<width; ++x)
				pixels[x][y]= getColor(line[x],coeffs[i]);
		}
	}
	return ownedPlanes;
}

QImage MColorModel::planes2image() {
	ASSERT( settingsInt(ColorModel)>=0 && settingsInt(ColorModel)<numOfModels() 
		&& ownedPlanes.size()==3 );
//	get the correct coefficients
	const Real (*coeffs)[4]= 3 + (settingsInt(ColorModel) ? YCbCrCoeffs : RGBCoeffs);
//	create and fill the image
	const PlaneSettings &firstSet= *ownedPlanes.front().settings;
	QImage result( firstSet.width, firstSet.height, QImage::Format_RGB32 );

	for (int y=0; y<firstSet.height; ++y) {
		QRgb *line= (QRgb*)result.scanLine(y);
		for (int x=0; x<firstSet.width; ++x) {
			Real vals[3]= {
				ownedPlanes[0].pixels[x][y],
				ownedPlanes[1].pixels[x][y],
				ownedPlanes[2].pixels[x][y]
			};
			line[x]= getColor( coeffs, vals );
		}
	}
	return result;
}

MColorModel::PlaneList MColorModel
::readData( istream &file, const PlaneSettings &prototype ) {
	ASSERT( ownedPlanes.empty() );
//	read the color-model identifier and check it
	settingsInt(ColorModel)= get<Uchar>(file);
	checkThrow( 0<=settingsInt(ColorModel) && settingsInt(ColorModel)<numOfModels() );
	return ownedPlanes= createPlanes( IRoot::Decode, prototype );
}

MColorModel::PlaneList MColorModel
::createPlanes( IRoot::Mode DEBUG_ONLY(mode), const PlaneSettings &prototype ) {
	ASSERT( 0<=settingsInt(ColorModel) && settingsInt(ColorModel)<numOfModels() 
		&& mode!=IRoot::Clear );
//	set the max. progress in UpdateInfo to the total count of pixels
	int planeCount= 3;
	(*prototype.updateInfo.incMaxProgress)( planeCount*prototype.width*prototype.height );
//	create the plane list	
	PlaneList result(planeCount);
	for (int i=0; i<planeCount; ++i) {
		result[i].pixels.allocate( prototype.width, prototype.height );
		PlaneSettings *newSet= new PlaneSettings(prototype);
		newSet->quality*= qualityMul(i);
		result[i].settings= newSet;
	}
	return result;
}
