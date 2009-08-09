#include "gui.h"
#include "modules.h"

using namespace std;

/** Converts the files in batch mode, returns the exit code, implemented in batch.cpp */
int batchRun(const vector<const char*> &fileNames);


struct TestOpt: public unary_function<const char*,bool> {
	bool operator()(const char *str) const
		{ return *str=='-'; }
};

void autoTranslation(QCoreApplication &app,QTranslator &trans) {
	if ( trans.load("lang-"+QLocale::system().name(),app.applicationDirPath()) )
		app.installTranslator(&trans);
}


int main(int argc,char **argv) {
	int result;
	ModuleFactory::init();
	
	QTranslator trans;
	
	vector<const char*> fileNames;
	remove_copy_if( argv+1, argv+argc, back_inserter(fileNames), TestOpt() );

	if ( fileNames.empty() ) { // no filenames passed -> GUI mode
		QApplication app(argc,argv);
		autoTranslation(app,trans);
		{
			ImageViewer viewer(app,trans);
			viewer.show();
			result= app.exec();
		} 
	} else { // batch mode
		QCoreApplication app(argc,argv);
		autoTranslation(app,trans);
		result= batchRun(fileNames);
	}
	
	ModuleFactory::destroy();
	return result;
}
