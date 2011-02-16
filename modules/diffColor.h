#ifndef DIFFCOLOR_HEADER_
#define DIFFCOLOR_HEADER_

#include "../headers.h"
#include "../fileUtil.h"

/// \ingroup modules
/**  */
class MDiffColor: public IColorTransformer {

	DECLARE_TypeInfo( MDiffColor, "Differential colors"
	, ""
	, {
		label:	"Primary color transformer",
		desc:	"The module that will be used to transform colors before differentiation",
		type:	settingModule<IColorTransformer>()
	}
	
	/*, {
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
	} */
	);

protected:
	/** Indices for settings */
	enum Settings { ModuleColor };
//	Settings-retrieval methods
	IColorTransformer* moduleColor() const
		{ return debugCast<IColorTransformer*>(settings[ModuleColor].m); }
	
protected:
	/** Internal structure keeping information about one differential plane */
	struct PlaneDiff: public Plane {
		SMatrix origPix;	///< The original matrix owned by moduleColor()
		Real *accelArray;	///< The array to store averages and their differences
		MinMax<Real> diffBounds; ///< The bounds of the unnormalized differential plane
		
		/** Initializes a differential plane with a given matrix, its settings
		 *	(with shrinked dimensions) and original pixel-matrix*/
		PlaneDiff( SMatrix pix, const PlaneSettings *set, SMatrix origPixels
		, const MinMax<Real> &bounds )
			: origPix(origPixels), accelArray(0), diffBounds(bounds)
		{
			pixels= pix;
			settings= set;
		}
		
		/** If ::accelArray is zero, allocates it and returns true; otherwise returns false */
		bool allocAvgs() {
			if (accelArray)
				return false;
			accelArray= new Real[settings->width+settings->height+1];
			return true;
		}
		
		/** Releases all referenced memory */
		void free() {
			delete[] accelArray;
			accelArray= 0;
			Plane::free(); //< free inherited pointers
		}
		
		/** Returns pointer to normalized and "quantized" vertical averages */
		Real* vertAvgs()		{ ASSERT(accelArray); return accelArray; }
		/** Returns pointer to normalized and "quantized" differences of horizontal averages */
		Real* horizAvgDiffs()	{ ASSERT(accelArray); return accelArray+settings->width+1; }
	}; // PlaneDiff struct
	
protected:
	vector<PlaneDiff> ownedPlanes; ///< The owned differential planes
	
protected:
//	Construction and destruction
	/** Just frees ::ownedPlanes */
	void free()		{ for_each( ownedPlanes, mem_fun_ref(&PlaneDiff::free) ); }
	/** Only calls ::free() */
	~MDiffColor()	{ free(); }
	
	
public:
/** \name IColorTransformer interface
 *	@{ */
	PlaneList image2planes(const QImage &toEncode,const PlaneSettings &prototype);
	QImage planes2image();

	void writeData(std::ostream &file);
	PlaneList readData(std::istream &file,const PlaneSettings &prototype);
///	@}
protected:
	/** For all the owned planes, fills vertAvgs() and horizAvgDiffs() with correct values */
	void ensureAvgs();
};

#endif
