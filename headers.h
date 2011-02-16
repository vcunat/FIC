#ifndef HEADERS_HEADER_
#define HEADERS_HEADER_

#ifdef QT_NO_DEBUG
	#define NDEBUG
#endif

/* Standard C++ includes used almost everywhere */
#include <algorithm>	// many functions concerning iterators
#include <cmath>		// maths functions - sqrt, isnan, ...
#include <fstream>		// streams
#include <limits>		// numeric_limits
#include <vector>		// std::vector

/* Qt forwards, pointers to these types are needed for some methods */
class QImage;
class QGroupBox;
class QTreeWidgetItem;
class QWidget;
#ifndef NDEBUG // needed for Module::debugModule prototypes
	class QPixmap;
	class QPoint;
#endif


/* Some type shortcuts */
typedef unsigned char	Uchar;
typedef unsigned short	Uint16;
typedef unsigned int	Uint32;
typedef ptrdiff_t		PtrInt;
typedef size_t			Uint;


/* Headers used almost everywhere */
#include "debug.h"
#include "util.h"
#include "matrixUtil.h"
#include "modules.h"
#include "interfaces.h"

#endif
