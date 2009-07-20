#include "imageUtil.h"

#include <QImage>

using namespace std;
namespace Color {
	const Real
		YCbCrCoeffs[][4]= {
	        { 0.299,     0.587,     0.114,    0   },
	        {-0.168736, -0.331264,  0.5,      0.5 },
	        { 0.5,      -0.418688, -0.081312, 0.5 },
	
	        { 1,         1,         1,        0   },
	        { 0,        -0.34414,   1.772,   -0.5 },
	        { 1.402,    -0.71414,   0,       -0.5 }
	    },
	    RGBCoeffs[][4]= {
	        {1,0,0,0}, {0,1,0,0}, {0,0,1,0},
	        {1,0,0,0}, {0,1,0,0}, {0,0,1,0}
	    };
	
	vector<Real> getPSNR(const QImage &a,const QImage &b) {
		int width= a.width(), height= a.height();
		int x, y;
		QRgb *line1, *line2;
		long sum, sumR, sumG, sumB;
		sum= sumR= sumG= sumB= 0;
		for (y=0; y<height; ++y) { /// \todo using walkers instead?
			line1= (QRgb*)a.scanLine(y);
			line2= (QRgb*)b.scanLine(y);
			for (x=0; x<width; ++x) {
				sum+= sqr( getGray(line1[x]) - getGray(line2[x]) );
				sumR+= sqr( qRed(line1[x]) - qRed(line2[x]) );
				sumG+= sqr( qGreen(line1[x]) - qGreen(line2[x]) );
				sumB+= sqr( qBlue(line1[x]) - qBlue(line2[x]) );
			}
		}
		vector<Real> result(4);
		result[0]= sumR;
		result[1]= sumG;
		result[2]= sumB;
		result[3]= sum;
		Real mul= Real(width*height) * Real(sqr(255));
		for (vector<Real>::iterator it=result.begin(); it!=result.end(); ++it)
			*it= Real(10) * log10(mul / *it);
		return result;
	}

} // Color namespace