#ifndef GUI_HEADER_
#define GUI_HEADER_

#include "interfaces.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProgressDialog>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QThread>
#include <QTime>
#include <QTranslator>
#include <QTreeWidgetItem>

class ImageViewer;
class SettingsDialog;
class EncodingProgress;

/** Represents the main window of the program, providing a GUI */
class ImageViewer: public QMainWindow { Q_OBJECT
	static const int AutoIterationCount= 20;

	IRoot *modules_settings	///  Module tree holding current settings
	, *modules_encoding;	///< Module tree that's currently encoding or the last one

	QTranslator translator;	///< The application's only translator
	QLabel *imageLabel; 	///< A pointer to the label showing images
/**	\name Actions
 *	@{ */
	QAction
        readAct, writeAct, compareAct, exitAct,
        settingsAct, encodeAct, saveAct,
        loadAct, clearAct, iterateAct;
///	@}
/**	\name Menus
 *	@{ */
	QMenu imageMenu, compMenu, decompMenu;//, langMenu, helpMenu;
///	@}
	void createActions();	///< Creates all actions and connects them to correct slots
    void createMenus();		///< Creates the menu bar and fills it with actions
    void translateUi();		///< Sets all texts in the application (uses current language)
    void updateActions();	///< Updates the availability of all actions
    void changePixmap(const QPixmap &pixmap) {
		imageLabel->setPixmap(pixmap);
		imageLabel->resize( pixmap.size() );
	}						///< Shows a new image
private slots:
/**	\name Methods performing the actions
	@{ */
	void read();
	void write();
	void compare();
	void settings();
	void encode();
	void save();
	void load();
	void clear();
	void iterate();
///	@}
public:
	/** Initializes the object to default settings */
	ImageViewer(QApplication &app);
	/** Only releases the modules */
	virtual ~ImageViewer()
		{ delete modules_settings; delete modules_encoding; }
private:
#ifndef NDEBUG
	void mousePressEvent(QMouseEvent *event);
#endif
};

inline void aConnect( const QObject *sender, const char *signal, const QObject *receiver
, const char *slot, Qt::ConnectionType type=Qt::AutoCompatConnection ) {
	#ifndef NDEBUG
		bool result=
	#endif
	QObject::connect(sender,signal,receiver,slot,type);
	assert(result);
}

/** Represents the encoding-settings dialog  */
class SettingsDialog: public QDialog { Q_OBJECT
	QGroupBox *setBox;
	QTreeWidget *treeWidget;
	IRoot *settings;
private slots:
	void currentItemChanged(QTreeWidgetItem *current,QTreeWidgetItem *previous);
	void settingChanges(int which);
public:
	SettingsDialog(ImageViewer *parent,IRoot *settingsHolder);
};


class SignalChanger: public QObject { Q_OBJECT
	int signal;
public:
	SignalChanger( int whichSignal, QWidget *parent=0, Module::ChoiceType type=Module::Stop )
	: QObject(parent), signal(whichSignal) {
		if (!parent)
			return;
	//	connect widget's changing signal to my notifyXxx slot
		const char *signalString=0, *slotString=0;
		switch (type) {
			case Module::Int:
			case Module::IntLog2:
				signalString= SIGNAL(valueChanged(int));
				slotString= SLOT(notifyInt(int));
				break;
			case Module::Float:
				signalString= SIGNAL(valueChanged(double));
				slotString= SLOT(notifyDouble(double));
				break;
			case Module::ModuleCombo:
			case Module::Combo:
				signalString= SIGNAL(currentIndexChanged(int));
				slotString= SLOT(notifyInt(int));
				break;
			default:
				assert(false);
		}//	switch
		aConnect( parent, signalString, this, slotString );
	}
public slots:
	void notifyInt(int)			{ emit notify(signal); }
	void notifyDouble(double)	{ emit notify(signal); }
signals:
	void notify(int);
};

class EncodingProgress: public QProgressDialog { Q_OBJECT
	static EncodingProgress *instance;

	bool terminate;

private:
	static void setMaxProgress(int maximum)
		{ instance->setMaximum(maximum); }
	static void incProgress(int increment)
		{ instance->setValue( instance->value() + increment ); }

private slots:
	void setTerminate()
		{ terminate= true; }
public:
	EncodingProgress(QWidget *parent)
	: QProgressDialog( tr("Encoding..."), tr("Cancel"), 0, 100, parent, Qt::Dialog )
	, terminate(false) {
		setWindowModality(Qt::ApplicationModal);
		setValue(0);
		setAutoClose(false);
		setAutoReset(false);
		aConnect( this, SIGNAL(canceled()), this, SLOT(setTerminate()) );
		instance= this;
	}
	UpdatingInfo getUpdatingInfo() const
		{ return UpdatingInfo( terminate, &setMaxProgress, &incProgress ); }
};

#endif // GUI_HEADER_
