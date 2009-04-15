#ifdef QT_NO_DEBUG
	#define NDEBUG
#endif

/* Standard C++ includes */
#include <algorithm>
#include <cmath>
//#include <cstdlib>	// size_t
#include <cstring>		// memcpy
#include <exception>
#include <fstream>
#include <functional>
#include <limits>		// numeric_limits
#include <memory>
#include <sstream>		// c++ conversions through streams
#include <typeinfo>
#include <vector>

#include "FerrisLoki/static_check.h"

/* Qt forwards */
class QImage;
class QGroupBox;
class QTreeWidgetItem;
class QWidget;
#ifndef NDEBUG
	class QPixmap;
	class QPoint;
#endif

typedef unsigned char	Uchar;
typedef unsigned short	Uint16;
typedef unsigned int	Uint32;
