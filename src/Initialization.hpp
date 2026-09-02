#pragma once

#include <future>

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Utils/MiscUtils.hpp"

namespace ModData
{
	class DataHandler
	{
	public:
		static DataHandler* GetSingleton()
		{
			static DataHandler singleton;
			return &singleton;
		}

		void LoadData()
		{
			static bool loadingStarted = false;

			if (loadingStarted) return;
			loadingStarted = true;

			TESdataHandler = RE::TESDataHandler::GetSingleton();
			LoadPluginsForms();
			PatchRaces();
			PatchHelmetToggle2();
		}

	private:
		static inline void LoadPluginsForms()
		{
			logger::info("Loading Plugins Froms Data...");

			for (const auto& formInfo : pluginForms) {
				*formInfo.formPtr = TESdataHandler->LookupForm(formInfo.formID, formInfo.pluginName.data());
				if (!*formInfo.formPtr && !formInfo.optional) {
					if (formInfo.pluginName == PLUGIN_NAME) {
						REPORT_AND_FAIL(
							"ERROR: The required plugin \"{}\" is missing! This means the mod is either not installed correctly or your mod manager failed to enable it.\n"
							"If you believe you installed the mod properly, please redo the manual installation without using a mod manager.\n\n"
							"This is NOT a bug - DO NOT report it! Instructions for manual installation are available on the mod's page.\n\n"
							"DETAILS: Form \"{}\" not found in \"{}\".",
							formInfo.pluginName, formInfo.name, formInfo.pluginName);
					} else {
						REPORT_AND_FAIL("ERROR: Form \"{}\" not found in \"{}\".", formInfo.pluginName, formInfo.name, formInfo.pluginName);
					}
				}
			}

			logger::info("Loading Plugins Froms Data: DONE");
		}

		static void PatchRaces()
		{
			using namespace ModData;

			for (auto* race : TESdataHandler->GetFormArray<RE::TESRace>()) {
				if (!race) continue;
				if (SettingsIni::sExcludedRaces.find(race->formEditorID.c_str()) != SettingsIni::sExcludedRaces.end()) continue;

				if (race->data.flags.all(RE::RACE_DATA::Flag::kFaceGenHead)) {
					race->decapitateArmors[RE::SEXES::kMale] = nullptr;
					race->decapitateArmors[RE::SEXES::kFemale] = nullptr;
					patchedRaces.push_back(race);
					TRACE("Patching <\"{}\" [REF:{:08X}]> race: DONE", race->formEditorID, race->formID);
				}
			}
		}

		static void PatchHelmetToggle2()
		{
			using namespace ModData;

			TRACE("Processing Patching Helmet Toggle 2...");

			const RE::BSFixedString& keywordName = "HT_IgnoreHeadgear";

			for (auto* keyword : TESdataHandler->GetFormArray<RE::BGSKeyword>()) {
				if (keyword && keyword->formEditorID == keywordName) {
					Decapitated_HeadHidden_ARMO->AddKeyword(keyword);
					if (ArmorHelmet_Keyword) Decapitated_HeadHidden_ARMO->AddKeyword(ArmorHelmet_Keyword);

					TRACE("Processing Patching Helmet Toggle 2: DONE");
					return;
				}
			}

			TRACE("Processing Patching Helmet Toggle 2: Mod not found");
		}
	};
}
