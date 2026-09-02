#pragma once

#include "Utils/MiscUtils.hpp"

class SettingsIni
{
public:
	// General
	static inline int iVerboseMode = 1;

	//Impulse
	static inline float fHeadImpulseFactor = 2.75f;
	static inline float fHeadSpinFactor = 1.2f;
	static inline float fHeadEquipmentImpulseMult = 1.0f;
	static inline float fHeadEquipmentSpinMult = 1.0f;

	// Equipment
	static inline bool  bCanBeActivated = true;
	static inline bool  bReplicateHeadEquipment = true;
	static inline bool  bUnequipExtraHeadEquipment = true;
	static inline float fClothingEquipmentEjectionChance = 85.0f;
	static inline float fLightEquipmentEjectionChance = 70.0f;
	static inline float fHeavyEquipmentEjectionChance = 40.0f;
	static inline float fEquipmentEjectionDelayMin = 0.05f;
	static inline float fEquipmentEjectionDelayMax = 0.3f;

	// Nodes
	static inline float fCOMNodeZPositionMale = 65.0f;
	static inline float fCOMNodeZPositionFemale = 65.0f;

	// Misc
	static inline int iCanBeResurrected = 1;
	static inline bool bAdvancedNPCMaintenance = true;
	static inline float fHeadDespawnTimeout = 14.0f;
	static inline float fHeadMass = 10.0f;
	static inline int iScalesUpdateFrequencyFactor = 10;
	static inline std::unordered_set<std::string> sExcludedRaces;
	static inline std::unordered_set<std::string> sExcludedArmorModels;

	static bool ReadSettings()
	{
		constexpr auto path = L"Data/SKSE/Plugins/NextGenDecapitations.ini";

		if (!std::filesystem::exists(path)) return false;

		CSimpleIniA ini;
		ini.SetUnicode();
		SI_Error rc = ini.LoadFile(path);

		if (rc < 0) return false;

		// General
		iVerboseMode = ini.GetLongValue("General", "iVerboseMode", 1);

		fHeadImpulseFactor = static_cast<float>(ini.GetDoubleValue("Impulse", "fHeadImpulseFactor", 2.75f));
		fHeadSpinFactor = static_cast<float>(ini.GetDoubleValue("Impulse", "fHeadSpinFactor", 1.2f));
		fHeadEquipmentImpulseMult = static_cast<float>(ini.GetDoubleValue("Impulse", "fHeadEquipmentImpulseMult", 1.0f));
		fHeadEquipmentSpinMult = static_cast<float>(ini.GetDoubleValue("Impulse", "fHeadEquipmentSpinMult", 1.0f));

		bCanBeActivated = ini.GetBoolValue("Equipment", "bCanBeActivated", true);
		bReplicateHeadEquipment = ini.GetBoolValue("Equipment", "bReplicateHeadEquipment", true);
		bUnequipExtraHeadEquipment = ini.GetBoolValue("Equipment", "bUnequipExtraHeadEquipment", true);
		fClothingEquipmentEjectionChance = static_cast<float>(ini.GetDoubleValue("Equipment", "fClothingEquipmentEjectionChance", 85.0f));
		fLightEquipmentEjectionChance = static_cast<float>(ini.GetDoubleValue("Equipment", "fLightEquipmentEjectionChance", 70.0f));
		fHeavyEquipmentEjectionChance = static_cast<float>(ini.GetDoubleValue("Equipment", "fHeavyEquipmentEjectionChance", 40.0f));
		fEquipmentEjectionDelayMin = static_cast<float>(ini.GetDoubleValue("Equipment", "fEquipmentEjectionDelayMin", 0.05f));
		fEquipmentEjectionDelayMax = static_cast<float>(ini.GetDoubleValue("Equipment", "fEquipmentEjectionDelayMax", 0.3f));

		fCOMNodeZPositionMale = static_cast<float>(ini.GetDoubleValue("Nodes", "fCOMNodeZPositionMale", 65.0f));
		fCOMNodeZPositionFemale = static_cast<float>(ini.GetDoubleValue("Nodes", "fCOMNodeZPositionFemale", 65.0f));

		iCanBeResurrected = ini.GetLongValue("Misc", "iCanBeResurrected", 1);
		bAdvancedNPCMaintenance = ini.GetBoolValue("Misc", "bAdvancedNPCMaintenance", true);
		fHeadDespawnTimeout = static_cast<float>(ini.GetDoubleValue("Misc", "fHeadDespawnTimeout", 14.0f));
		fHeadMass = static_cast<float>(ini.GetDoubleValue("Misc", "fHeadMass", 10.0f));
		iScalesUpdateFrequencyFactor = ini.GetLongValue("Misc", "iScalesUpdateFrequencyFactor", 10);
		sExcludedRaces = MiscUtils::SplitString<std::unordered_set<std::string>>(ini.GetValue("Misc", "sExcludedRaces", "DremoraRace,DLC2DremoraRace"), ',');
		sExcludedArmorModels = MiscUtils::SplitString<std::unordered_set<std::string>>(ini.GetValue("Misc", "sExcludedArmorModels", "wig,hair"), ',', false, true);

		// Apply constraints
		fHeadImpulseFactor = std::clamp(fHeadImpulseFactor, 0.0f, 10.0f);
		fHeadSpinFactor = std::clamp(fHeadSpinFactor, -5.0f, 5.0f);
		fHeadEquipmentImpulseMult = std::clamp(fHeadEquipmentImpulseMult, 0.0f, 3.0f);
		fHeadEquipmentSpinMult = std::clamp(fHeadEquipmentSpinMult, 0.0f, 3.0f);

		fClothingEquipmentEjectionChance = std::clamp(fClothingEquipmentEjectionChance, 0.0f, 100.0f);
		fLightEquipmentEjectionChance = std::clamp(fLightEquipmentEjectionChance, 0.0f, 100.0f);
		fHeavyEquipmentEjectionChance = std::clamp(fHeavyEquipmentEjectionChance, 0.0f, 100.0f);
		fEquipmentEjectionDelayMin = std::clamp(fEquipmentEjectionDelayMin, 0.05f, 2.0f);
		fEquipmentEjectionDelayMax = std::clamp(fEquipmentEjectionDelayMax, 0.05f, 2.0f);
		if (fEquipmentEjectionDelayMin > fEquipmentEjectionDelayMax) {
			std::swap(fEquipmentEjectionDelayMin, fEquipmentEjectionDelayMax);
		}

		iCanBeResurrected = std::clamp(iCanBeResurrected, 0, 2);
		fHeadDespawnTimeout = std::clamp(fHeadDespawnTimeout, 0.0f, 30.0f);
		fHeadMass = std::clamp(fHeadMass, 1.0f, 100.0f);
		iScalesUpdateFrequencyFactor = std::clamp(iScalesUpdateFrequencyFactor, 1, 1000);

		debugVerboseMode = iVerboseMode;

		return true;
	}
};
