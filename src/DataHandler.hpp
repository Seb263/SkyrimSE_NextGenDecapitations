#pragma once

namespace ModData
{
	constexpr std::string_view MOD_NAME = "Next-Gen Decapitations";
	constexpr std::string_view PLUGIN_NAME = "Next-Gen Decapitations.esp";

	struct PluginForm
	{
		std::string_view name;
		void**           formPtr;
		uint32_t         formID;
		std::string_view pluginName;
		bool             optional = false;
	};

	// Properties storing game form references
	inline RE::TESObjectARMO* Decapitated_Neck_ARMO;
	inline RE::TESObjectARMO* Decapitated_Head_ARMO;
	inline RE::TESObjectARMO* Decapitated_Neck_Afflicted_ARMO;
	inline RE::TESObjectARMO* Decapitated_Head_Afflicted_ARMO;
	inline RE::TESObjectARMO* Decapitated_HeadHidden_ARMO;
	inline RE::TESObjectARMO* Invisible_Actor_Armor;
	inline RE::BGSArtObject*  NeckBloodFX_AO;
	inline RE::BGSArtObject*  HeadBloodFX_AO;
	inline RE::BGSKeyword*    ArmorHelmet_Keyword;
	inline RE::BGSKeyword*    FurnitureExecutioner_Keyword;
	inline RE::EffectSetting* HT_NPCManagedEffect_Effect;
	inline RE::EffectSetting* HT_HeadgearEquippedEffect_Effect;

	static inline const std::vector<PluginForm> pluginForms = {
		{ "Decapitated_Neck_ARMO", reinterpret_cast<void**>(&Decapitated_Neck_ARMO), 0x806, PLUGIN_NAME },
		{ "Decapitated_Head_ARMO", reinterpret_cast<void**>(&Decapitated_Head_ARMO), 0x807, PLUGIN_NAME },
		{ "Decapitated_Neck_Afflicted_ARMO", reinterpret_cast<void**>(&Decapitated_Neck_Afflicted_ARMO), 0x814, PLUGIN_NAME },
		{ "Decapitated_Head_Afflicted_ARMO", reinterpret_cast<void**>(&Decapitated_Head_Afflicted_ARMO), 0x813, PLUGIN_NAME },
		{ "Decapitated_HeadHidden_ARMO", reinterpret_cast<void**>(&Decapitated_HeadHidden_ARMO), 0x816, PLUGIN_NAME },
		{ "Invisible_Actor_Armor", reinterpret_cast<void**>(&Invisible_Actor_Armor), 0x80E, PLUGIN_NAME },
		{ "NeckBloodFX_AO", reinterpret_cast<void**>(&NeckBloodFX_AO), 0x805, PLUGIN_NAME },
		{ "HeadBloodFX_AO", reinterpret_cast<void**>(&HeadBloodFX_AO), 0x804, PLUGIN_NAME },

		{ "ArmorHelmet_Keyword", reinterpret_cast<void**>(&ArmorHelmet_Keyword), 0x6C0EE, "Skyrim.esm", true },
		{ "FurnitureExecutioner_Keyword", reinterpret_cast<void**>(&FurnitureExecutioner_Keyword), 0x2E8ED, "Skyrim.esm", true },
		{ "HT_NPCManagedEffect_Effect", reinterpret_cast<void**>(&HT_NPCManagedEffect_Effect), 0x850, "Helmet Toggle 2.esp", true },
		{ "HT_HeadgearEquippedEffect_Effect", reinterpret_cast<void**>(&HT_HeadgearEquippedEffect_Effect), 0x84F, "Helmet Toggle 2.esp", true }
	};

	inline RE::TESDataHandler* TESdataHandler;
	inline std::vector<RE::TESRace*> patchedRaces;

	inline bool dismemberingFrameworkEnabled = false;
	inline bool deathDropOverhaulEnabled = false;

	constexpr float ngdValueMarker = static_cast<float>('NGD');
	constexpr RE::COL_LAYER NonCollidableLayer = RE::COL_LAYER::kUnused3; // 49
}
