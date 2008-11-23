#include "modules/root.h"
#include "modules/colorModel.h"
#include "modules/squarePixels.h"
#include "modules/quadTree.h"
#include "modules/stdDomains.h"
#include "modules/quality2SE.h"
#include "modules/stdEncoder.h"
#include "modules/vliCodec.h"
#include "modules/saupePredictor.h"
#include "modules/noPredictor.h"

#include "util.h"
#include "fileUtil.h"

#include "FerrisLoki/DataGenerators.h"

using namespace std;

typedef Loki::TL::MakeTypelist< MRoot, MColorModel, MSquarePixels, MQuadTree, MStdDomains
, MQuality2SE_std, MStandardEncoder, MDifferentialVLICodec, MSaupePredictor, NoPredictor >
::Result Modules;

const int powers[]={1,2,4,8,16,32,64,128,256,512,1024,2*1024,4*1024,8*1024,16*1024
    ,32*1024,64*1024,128*1024,256*1024,512*1024,1024*1024,2*1024*1024,4*1024*1024
    ,8*1024*1024,16*1024*1024,32*1024*1024,64*1024*1024,128*1024*1024,256*1024*1024};

////	Compatible<TypeList,Iface> struct template - leaves in the TypeList only derivates
////	of Iface class parameter - used by Inteface<Iface>, hidden for others
namespace NOSPACE {
	using namespace Loki;
	using namespace Loki::TL;

	template <class Typelist,class Interface>
	struct Compatible;

	template <class Iface>
	struct Compatible<NullType,Iface> {
		typedef NullType Result;
	};

	template <class Head,class Tail,class Iface>
	struct Compatible< Typelist<Head,Tail> , Iface > {
		typedef typename Select
			<SuperSubclass<Iface,Head>::value
			,Typelist< Head , typename Compatible<Tail,Iface>::Result >
			,typename Compatible<Tail,Iface>::Result
		>::Result Result;
	};

	template <class T> struct ExtractId {
		int operator()() const
			{ return Loki::TL::IndexOf<Modules,T>::value; }
	};
}
template<class Iface> const vector<int>& Interface<Iface>::getCompMods() {
	using namespace Loki::TL;
	typedef typename Compatible<Modules,Iface>::Result CompList;
	if ( compMods_.empty() && Length<CompList>::value ) {
		compMods_.reserve(Length<CompList>::value);
		IterateTypes<CompList,ExtractId> gendata;
		gendata( back_inserter(compMods_) );
	}
	return compMods_;
}
template <class Iface> std::vector<int> Interface<Iface>::compMods_;


////	Module class members
const Module::SettingsTypeItem Module::SettingsTypeItem
::stopper= { 0, 0, {Stop,{i:-1},{text:0}} };

Module::SettingsItem* Module::copySettings(CloneMethod method) const {
	assert( method==DeepCopy || method==ShallowCopy );
//	copy the settings array
	int length= info().setLength;
	if (!length)
		return 0;
	SettingsItem *result= new SettingsItem[length];
	copy( settings, settings+length, result );

	SettingsItem *item= result, *itemEnd= result+length;
	if (method==DeepCopy)
	//	make child modules copy themselves (deeply again)
		while (item!=itemEnd) {
			if (item->m)
				item->m= clone(item->m);
			++item;
		}
	else // ShallowCopy
	//	null module links
		while (item!=itemEnd) {
			item->m= 0;
			++item;
		}

	return result;
}
void Module::initDefaultModuleLinks() {
//	iterate over all settings
	const SettingsTypeItem *setType= info().setType;
	for (int i=0; setType[i].type.type!=Stop; ++i )
		if (setType[i].type.type==ModuleCombo) {
		//	it is a module link -> initialize it with the right prototype
			assert(!settings[i].m);
			settings[i].m= constCast(&ModuleFactory::prototype(
				(*setType[i].type.data.compatIDs)[settings[i].val.i]
			));
		}
}
void Module::nullModuleLinks() {
//	iterate over all settings
	SettingsItem *itEnd= settings+info().setLength;
	for (SettingsItem *it=settings; it!=itEnd; ++it)
		it->m= 0;
}
template<class M> M* Module::concreteClone(CloneMethod method) const {
	assert( this && info().id == ModuleFactory::getModuleID<M>() );
//	create a new instance of the same type and fill its settings with a copy of mine
	M *result= new M;
	result->settings= copySettings(method);
	return result;
}
	
void Module::createDefaultSettings() {
	assert(!settings);
	const TypeInfo &inf= info();
	settings= new SettingsItem[inf.setLength];
	copy( inf.setType, inf.setType+inf.setLength, settings );
}

void Module::file_saveModuleType( ostream &os, int which ) {
//	do some assertions - we expect to have the child module, etc.
	assert( which>=0 && which<info().setLength );
	SettingsItem &setItem= settings[which];
	assert( info().setType[which].type.type==ModuleCombo && setItem.m );
//	put the module's identifier
	put<Uchar>( os, setItem.m->info().id );
}
void Module::file_loadModuleType( istream &is, int which ) {
//	do some assertions - we expect not to have the child module, etc.
	assert( which>=0 && which<info().setLength );
	const SettingsTypeItem &setType= info().setType[which];
	SettingsItem &setItem= settings[which];
	assert( setType.type.type==ModuleCombo && !setItem.m );
//	get module identifier and check its existence
	int newId= get<Uchar>(is);
	checkThrow( 0<=newId && newId<Loki::TL::Length<Modules>::value );
//	check module compatibility
	const vector<int> &v= *setType.type.data.compatIDs;
	checkThrow( find(v.begin(),v.end(),newId) != v.end() );
//	create a new correct empty module
	setItem.m= ModuleFactory::newModule(newId,ShallowCopy);
}


////	ModuleFactory class members
ModuleFactory* ModuleFactory::instance=0;

template<class M> int ModuleFactory::getModuleID()
	{ return Loki::TL::IndexOf<Modules,M>::value; }

void ModuleFactory::initialize() {
//	create one instance of each module-type
	assert( prototypes.empty() );
	prototypes.reserve( Loki::TL::Length<Modules>::value );
	Loki::TL::IterateTypes<Modules,Creator> gendata;
	gendata( back_inserter(prototypes) );
//	initialize the prototypes' settings and interconnections
	for_each( prototypes.begin(), prototypes.end()
	, mem_fun(&Module::createDefaultSettings) );
	for_each( prototypes.begin(), prototypes.end()
	, mem_fun(&Module::initDefaultModuleLinks) );
}
void ModuleFactory::changeDefaultSettings(const Module &module) {
//	get the right prototype
	const Module::TypeInfo &mi= module.info();
	Module::SettingsItem *protSet= prototype(mi.id).settings;
//	replace prototype's settings
	assert( protSet && module.settings );
	copy( module.settings, module.settings+mi.setLength, protSet );
//	convert module-links to links to the correct prototypes
	for (const Module::SettingsTypeItem *setType= mi.setType
	; setType->type.type!=Module::Stop; ++setType,++protSet)
		if ( setType->type.type==Module::ModuleCombo ) {
			assert(protSet->m);
			protSet->m= constCast(&prototype( protSet->m->info().id ));
		}
}

template<class T> int ModuleFactory::Instantiator<T>::operator()() const {
	T::newCompatibleModule();
	getModuleID<T>();
	((T*)(0))->T::abstractClone();
	return 0;
}
void ModuleFactory::instantiateModules() {
	IterateTypes<Modules,Instantiator> gendata;
	vector<int> v;
	gendata(back_inserter(v));
}
