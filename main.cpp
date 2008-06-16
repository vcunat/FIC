#include "gui.h"
#include "modules.h"

int main(int argc,char **argv) {
	int result;
	{
		QApplication app(argc,argv);
		ModuleFactory::init();
		{
			ImageViewer viewer(app);
			viewer.show();
			result= app.exec();
		}
		ModuleFactory::destroy();
	}
	return result;
}
