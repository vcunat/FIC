#include "gui.h"
#include "modules.h"

int main(int argc,char **argv) {
	int result;
	{
		QApplication app(argc,argv);
		ModuleFactory::init();
		try {
			ImageViewer viewer(app);
			viewer.show();
			result= app.exec();
		} catch(std::string &s) {
			QMessageBox::critical( 0, "Uncaught exception", QString::fromStdString(s) );
		}
		ModuleFactory::destroy();
	}
	return result;
}
