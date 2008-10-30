#ifndef MODULES_HEADER_
#define MODULES_HEADER_

#include "util.h"

//	Forwards for all classes visible outside this header
class Module;
class ModuleFactory;
template <class Iface> class Interface;


/** A common base class for all modules */
class Module {
	friend class ModuleFactory; ///< Permission for the ModuleFactory to manipulate Modules

//	Type definitions
public:
	/** Two types of cloning, used in #clone */
	enum CloneMethod { ShallowCopy, DeepCopy };
	/** Types of a setting */
	enum ChoiceType {
		Stop,		///< a list-terminator
		Int,		///< integer from an interval
		IntLog2,	///< integer from an interval prefixed with 2^
		Float,		///< real number from an interval
		ModuleCombo,///< a connection to another module (shown as a combo-box)
		Combo		///  choice from a list of strings
	};
	/** Represents the type of one setting - including label, description, etc.\ */
	struct SettingsTypeItem {
		static const SettingsTypeItem stopper; ///< Predefined list terminator

		ChoiceType type; ///< The type of the item
		union {
			int i[2];			///< Lower and upper bound (type==Int)
			float f[2];			///< Lower and upper bound (type==Float)
			const char *text;	///< Lines of the combo-box (type==Combo)
			/** Pointer to the vector of IDs of compatible modules (type==ModuleCombo),
			 *	meant to be one of Interface::getCompMods() */
			const std::vector<int> *compatIDs;
		} data;				///< Additional data, differs for different #type
		const char *label	///  The text label of the setting
		, *desc;			///< Description text
	};
	/** Represents one setting value */
	struct SettingsItem {
		Module *m;	///< Pointer to the connected module (if type is Module::ModuleCombo)
		union {
			int i;	///< Setting value for integer types (see Module::ChoiceType)
			float f;///< Setting value for real-number type (see Module::ChoiceType)
		};			///< The setting value

		SettingsItem()			: m(0) 	{}			///< Just nulls the module pointer
		SettingsItem(int i_)	: m(0), i(i_) {} 	///< Auto-init for integer settings
		SettingsItem(float f_)	: m(0), f(f_) {}	///< Auto-init for real settings
		~SettingsItem() 		{ delete m; }		///< Deletes the module pointer
	};

//	Data definitions
protected:
	SettingsItem *settings; ///< The current setting values of this Module

/** \name Construction, destruction and related methods
 *	@{	- all private, only to be used by ModuleFactory */
private:
	/**	Creates a deep copy of module's settings (includes the whole module subtree) */
	SettingsItem* copySettings(CloneMethod method) const;
	/** Called for prototypes to initialize the default links to other modules */
	void initDefaultModuleLinks();
	/** Called for prototypes to null links to other modules */
	void nullModuleLinks();
	/** Called for prototypes to create the settings and inicialize them with defaults */
	void createDefaultSettings()
		{ assert(!settings), settings= newDefaultSettings(); }

	/** Denial of assigment */
	Module& operator=(const Module&);
	/** Denial of copying */
	Module(const Module&);
protected:
	/**	Initializes an empty module */
	Module(): settings(0) {}

	/** Cloning method, implemented in all modules via macros \return a new module
	 *	\param method determines creation of a deep or a shallow copy (zeroed children) */
	virtual Module* abstractClone(CloneMethod method) const =0;
	/** Concrete cloning method - templated by the actual type of the module */
	template<class M> M* concreteClone(CloneMethod method) const;
public:
	/** Friend non-member cloning function (returns the type it gets) */
	template<class M> friend M* clone( const M *module, CloneMethod method=Module::DeepCopy )
		{ return debugCast<M*>( module->abstractClone(method) ); }

	/** Deletes the settings, destroying child modules as well */
	virtual ~Module()
		{ delete[] settings; }
		
	#ifndef NDEBUG
		DECLARE_debugModule { return 0; }
	#endif	
///	@}

/** \name Virtual "static" const methods
 *	@{	- only depend on the type of the module, all implemented inline by macros */
public:
	/** Returns module's name */
	virtual const char* moduleName() const =0;
	/** Returns module's description */
	virtual const char* moduleDescription() const =0;
	/** Returns module's ID */
	virtual int moduleId() const =0;
	/** Returns the type of module's settings */
	virtual const SettingsTypeItem* settingsType() const =0;
	/** Creates and array filled with the compile-time default settings of the module */
	virtual SettingsItem* newDefaultSettings() const =0;
	/** Returns the number of setting items */
	virtual int settingsLength() const =0;
///	@}

/**	\name Settings visualization and related methods
 *	@{	- all implemented in gui.cpp */
public:
	/** Creates or updates settings-box and/or settings-tree,
	 *	if overridden in derived modules, it is recommended to call this one at first */
	virtual void adjustSettings( int which, QTreeWidgetItem *myTree, QGroupBox *setBox );
protected:
	/** Reads a setting value from a widget */
	void widget2settings( const QWidget *widget, int which );
	/** Writes a setting value into a widget */
	void settings2widget( QWidget *widget, int which );
	/** Initializes a widget according to a setting-item type */
	void settingsType2widget( QWidget *widget, const SettingsTypeItem &typeItem );
///	@}

//	Other methods
protected:
	/** Puts a module-identifier in a stream (which = the index in settings) */
	void file_saveModuleType( std::ostream &os, int which );
	/** Gets an module-identifier from the stream, initializes the pointer in settings
	 *	with a new empty instance and does some checking (bounds,Iface) */
	void file_loadModuleType( std::istream &is, int which );
	/** A shortcut method for working with integer settings */
	int& settingsInt(int index)
		{ return settings[index].i; }
};	//	Module class


/** Macros for easier Module-writing */
#define DECLARE_M_cloning_name_desc(Cname,name,desc) \
protected: \
/**	\name Macro-generated
	@{ - an enum, two friends and trivial implementations of virtual "static" methods */ \
	friend class Module; /**< Friend needed for template cloning */ \
	friend class ModuleFactory; \
	friend struct ModuleFactory::Creator<Cname>; /**< Needed for GCC-3 */ \
public: /* redefining virtual functions */ \
	Cname* abstractClone(CloneMethod method=DeepCopy) const \
											{ return concreteClone<Cname>(method); } \
	const char* moduleName() const			{ return name; } \
	const char* moduleDescription() const	{ return desc; } \
	int settingsLength() const				{ return settingsLength_; } \
	int moduleId() const					{ return ModuleFactory::getModuleID<Cname>(); } \

#define DECLARE_M_settings_none() \
private: \
	enum { settingsLength_=0 /**< the number of settings */ }; \
public: \
	virtual const SettingsTypeItem* settingsType() const \
		{ return &SettingsTypeItem::stopper; } \
	virtual SettingsItem* newDefaultSettings() const { return 0; } \
/**	@} */

#define DECLARE_M_settings_type(setType...) \
private: \
	enum { settingsLength_= /**< the number of settings \hideinitializer */ \
		sizeof((SettingsTypeItem[]){setType}) / sizeof(SettingsTypeItem) \
	}; \
public: \
	virtual const SettingsTypeItem* settingsType() const { \
		static SettingsTypeItem settingsType_[]= {setType,SettingsTypeItem::stopper}; \
		return settingsType_; \
	} \

#define DECLARE_M_settings_default(setDefault...) \
public: \
	virtual SettingsItem* newDefaultSettings() const { \
		static const SettingsItem defaults[]= {setDefault}; \
		LOKI_STATIC_CHECK( sizeof(defaults)/sizeof(SettingsItem) == settingsLength_ \
		, Wrong_number_of_default_settings_specified ); \
		SettingsItem *result= new SettingsItem[settingsLength_]; \
		std::copy( defaults, defaults+settingsLength_, result ); \
		return result; \
	} \
/**	@} */


/** A singleton factory class for creating modules */
class ModuleFactory {
	/** Static pointer to the only instance of the ModuleFactory */
	static ModuleFactory *instance;

	/** Pointers to the prototypes for every module type (indexed by their ID's) */
	std::vector<Module*> prototypes;

	/** Private constructor to avoid uncontrolled instantiation */
	ModuleFactory() {}
	/** Deletes the prototypes */
	~ModuleFactory() {
		for_each( prototypes.begin(), prototypes.end(), std::mem_fun(&Module::nullModuleLinks) );
		for_each( prototypes.begin(), prototypes.end(), SingleDeleter<Module>() );
	}
	/** The real constructing routine */
	void initialize();

public:
	/** Returns the ID of a module-type */
	template<class M> static int getModuleID();

	/** Method to instantiate the singleton */
	static void init()
		{ assert(!instance); (instance=new ModuleFactory)->initialize(); }
	/** Method to delete the singleton */
	static void destroy()
		{ delete instance; instance=0; }

	/** Returns a reference to the prototype of the module with given id */
	static const Module& prototype(int id) {
		assert( instance && instance->prototypes.at(id) );
		return *instance->prototypes[id];
	}
	/** Returns the name of the module-type with given id */
	static const char* moduleName(int id)
		{ return prototype(id).moduleName(); }
	/** Returns the description of the module-type with given id */
	static const char* moduleDescription(int id)
		{ return prototype(id).moduleDescription(); }
	/** Returns a new instance of the module-type with given id */
	static Module* newModule( int id, Module::CloneMethod method=Module::DeepCopy )
		{ return prototype(id).abstractClone(method); }

	/** Sets the defaults to given module's settings */
	static void changeDefaultSettings(const Module &module);

private:
//	some helper stuff
	template<class T> struct Creator {
		T* operator()() const { return new T; }
	};
	template<class T> struct Instantiator {
		int operator()() const;
	};
	/** Never called, only exists for certain templates to be instantiated in modules.cpp */
	static void instantiateModules();
};

/**	A base class for all interfaces, parametrized by the interface's type */
template<class Iface> class Interface: public Module {
	static std::vector<int> compMods_; ///< IDs of modules derived from this interface
public:
	/** Returns a reference to #compMods_ (initializes it if empty) */
	static const std::vector<int>& getCompMods();
	/** Returns a reference to the index-th prototype implementing this interface */
	static const Iface& compatiblePrototype(int index=0) {
		assert( index>=0 && index<(int)getCompMods().size() );
		return *debugCast<const Iface*>(&ModuleFactory::prototype( getCompMods()[index] ));
	}
	/** Creates a new instance of the index-th compatible module */
	static Iface* newCompatibleModule(int index=0)
		{ return clone( &compatiblePrototype(index) ); }
};

#endif // MODULES_HEADER_
