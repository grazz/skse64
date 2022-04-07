#include "skse64/PluginAPI.h"
#include "skse64_common/skse_version.h"
#include "skse64/PapyrusNativeFunctions.h"
#include "skse64/GameObjects.h"
#include "skse64/GameReferences.h"
#include "skse64/PapyrusObjectReference.h"
#include "skse64/PapyrusActor.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameAPI.h"
#include <vector>
#include <algorithm>

IDebugLog gLog("C98CosmeticOverride.log");

void copyAA(TESObjectARMA* dst, TESObjectARMA* src) {
	dst->biped           = src->biped          ;
	dst->race            = src->race           ;
	dst->additionalRaces = src->additionalRaces;
	dst->models[0][0]    = src->models[0][0]   ;
	dst->models[0][1]    = src->models[0][1]   ;
	dst->models[1][0]    = src->models[1][0]   ;
	dst->models[1][1]    = src->models[1][1]   ;
	dst->data            = src->data           ;
	//dst->flags           = src->flags          ;
	dst->footstepSet     = src->footstepSet    ;
}

int addnum = 0;

void CO_clear(StaticFunctionTag* base, TESObjectARMO* armo, TESObjectREFR* mann) {
	armo->bipedObject.SetSlotMask(0);
	tArray<TESObjectARMA*> aas = armo->armorAddons;
	_MESSAGE("Clearing");
	for(int i = 0; i < aas.count - 1; i++) {
		copyAA(aas[i], aas[aas.count - 1]);
		aas[i]->biped.SetSlotMask(0);
	}
	addnum = 0;
	armo->bipedObject.SetSlotMask(0);
}

unsigned int msb32(register unsigned int x) {
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x & ~(x >> 1);
}

#define RESERVED_SLOTS 0x80030803
void CO_update(StaticFunctionTag* base, TESObjectARMO* armo, TESObjectREFR* mann) {
	PlayerCharacter* p = *g_thePlayer;
	
	_MESSAGE("Finding and copying addons");
	for(int i = 0; i < papyrusObjectReference::GetNumItems(mann); i++) {
		TESObjectARMO* a = DYNAMIC_CAST(papyrusObjectReference::GetNthForm(mann, i), TESForm, TESObjectARMO);
		if(!a) continue;
		for(int j = 0; j < a->armorAddons.count; j++) {
			TESObjectARMA* aa = a->armorAddons[j];
			if(aa->isValidRace(p->race)) {
				copyAA(armo->armorAddons[addnum], aa);
				armo->armorAddons[addnum]->data.priority[0] = 25;
				armo->armorAddons[addnum]->data.priority[1] = 25;
				addnum++;
			}
		}
	}
	_MESSAGE("Found %d addons", addnum);
	std::sort(armo->armorAddons.entries, armo->armorAddons.entries + addnum, [](TESObjectARMA* a, TESObjectARMA* b) { return a->formID > b->formID; });
	_MESSAGE("Finding free slots");
	int usedSlots = 0;
	for(unsigned int i = 1; i != 0; i <<= 1) {
		TESForm* worn = papyrusActor::GetWornForm(p, i);
		if(worn) {
			_MESSAGE(" Slot %08X contains %s", i, ((TESObjectARMO*)worn)->fullName.name.data);
			usedSlots |= i;
		}
	}
	_MESSAGE("Slots %08X are used by physical equips, %08X are reserved", usedSlots, RESERVED_SLOTS);
	usedSlots |= RESERVED_SLOTS;
	_MESSAGE("Testing for addons we don't need to change...");
	std::vector<TESObjectARMA*> addons(armo->armorAddons.entries, armo->armorAddons.entries + addnum);
	for(auto it = addons.begin(); it != addons.end(); it++) {
		int mask = (*it)->biped.GetSlotMask();
		if(!(mask & usedSlots)) {
			int msb = msb32(mask & ~usedSlots);
			_MESSAGE(" Keeping slot %08X for %s (%08X)", msb, (*it)->models[0][1].GetModelName(), mask);
			armo->bipedObject.AddSlotToMask(msb);
			usedSlots |= msb;
			it = addons.erase(it) - 1;
		};
	}
	_MESSAGE("Adding slots where needed");
	for(unsigned int i = 1 << 31; i != 0 && addons.size(); i >>= 1) {
		if(!(i & usedSlots)) {
			TESObjectARMA* aa = addons[0];
			addons.erase(addons.begin());
			_MESSAGE(" Set slot %08X for %s (%08X)", i, aa->models[0][1].GetModelName(), aa->biped.GetSlotMask());
			aa->biped.AddSlotToMask(i);
			armo->bipedObject.AddSlotToMask(i);
			usedSlots |= i;
		}
	}
	if(addons.size()) {
		_MESSAGE("Failed to copy %d addons", addons.size());
		for(auto it = addons.begin(); it != addons.end(); it++) {
			_MESSAGE(" %s (%08X)", (*it)->models[0][1].GetModelName(), (*it)->biped.GetSlotMask());
		}
	}
	_MESSAGE("Final slot mask is %08X", armo->bipedObject.GetSlotMask());
}

bool registerFunc(VMClassRegistry* registry) {
	registry->RegisterFunction(new NativeFunction2<StaticFunctionTag, void, TESObjectARMO*, TESObjectREFR*>("Update", "C98CosmeticHandler", CO_update, registry));
	registry->RegisterFunction(new NativeFunction2<StaticFunctionTag, void, TESObjectARMO*, TESObjectREFR*>("Clear", "C98CosmeticHandler", CO_clear, registry));
	return true;
}

extern "C" {
	bool SKSEPlugin_Load(const SKSEInterface* skse) {
		return ((SKSEPapyrusInterface*)skse->QueryInterface(kInterface_Papyrus))->Register(registerFunc);
	}

	__declspec(dllexport) SKSEPluginVersionData SKSEPlugin_Version =
	{
		SKSEPluginVersionData::kVersion,
		2,
		"Skyrim Engine Extender",
		"Expired6978",
		"expired6978@gmail.com",
		0,	// not version independent
		{ RUNTIME_VERSION_1_6_353, 0 },	// compatible with 1.6.318
		0,	// works with any version of the script extender. you probably do not need to put anything here
	};
}