#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/NativeUtils.hpp"

namespace ModData
{
	constexpr std::uint32_t coSaveId = std::byteswap('NGD');
	constexpr std::uint32_t decapMapId = std::byteswap('DCAP');

	struct DecapEntry
	{
		RE::FormID headId;
		float      gameTime;
	};
	inline std::unordered_map<RE::FormID, DecapEntry> actorToHeadMap;
	inline std::unordered_map<RE::FormID, RE::FormID> headToActorMap;

	class Serialization
	{
	public:
		static void RegisterSerializationCallbacks()
		{
			auto serialization = SKSE::GetSerializationInterface();
			serialization->SetUniqueID(coSaveId);
			serialization->SetSaveCallback(OnSKSESave);
			serialization->SetLoadCallback(OnSKSELoad);
			serialization->SetRevertCallback(OnSKSERevert);
		}

		static void AddDecapitation(RE::FormID actorFormID, RE::FormID headFormID)
		{
			if (!MiscUtils::IsFormIDValid(actorFormID) || !MiscUtils::IsFormIDValid(headFormID)) return;
			
			const RE::Calendar* calendarInstance = RE::Calendar::GetSingleton();
			if (!calendarInstance) REPORT_AND_FAIL("Failed to retrieve RE::Calendar singleton for decapitation timestamp.");
			const float gameTime = calendarInstance->GetCurrentGameTime();

			actorToHeadMap[actorFormID] = DecapEntry{ headFormID, gameTime };
			headToActorMap[headFormID] = actorFormID;
		}

		static RE::FormID GetActorFromHead(RE::FormID headFormID)
		{
			if (!MiscUtils::IsFormIDValid(headFormID)) return 0x0;

			auto it = headToActorMap.find(headFormID);
			return (it != headToActorMap.end()) ? it->second : 0x0;
		}

		static RE::FormID GetHeadFromActor(RE::FormID actorFormID)
		{
			if (!MiscUtils::IsFormIDValid(actorFormID)) return 0x0;

			auto it = actorToHeadMap.find(actorFormID);
			return (it != actorToHeadMap.end()) ? it->second.headId : 0x0;
		}

		static void RemoveDecapitation(RE::FormID formID)
		{
			if (!MiscUtils::IsFormIDValid(formID)) return;

			// If it's an actor
			auto a_it = actorToHeadMap.find(formID);
			if (a_it != actorToHeadMap.end()) {
				headToActorMap.erase(a_it->second.headId);
				actorToHeadMap.erase(a_it);
				TRACE("Removed decapitation entry for actor [REF:{:08X}].", formID);
				return;
			}

			// If it's a head
			auto h_it = headToActorMap.find(formID);
			if (h_it != headToActorMap.end()) {
				actorToHeadMap.erase(h_it->second);
				headToActorMap.erase(h_it);
				TRACE("Removed decapitation entry for head [REF:{:08X}].", formID);
			}
		}

		static bool HeadMaintenanceProcess(RE::FormID headFormID, const bool originReady, RE::Actor* originOptional = nullptr)
		{
			const bool result = [&]() {
				auto* headRef = MiscUtils::GetValidReference<RE::Actor>(headFormID);
				if (!headRef) return false;

				auto* originRef = originOptional ? originOptional : MiscUtils::GetValidReference<RE::Actor>(GetActorFromHead(headFormID));
				if (!headRef->IsDead() || headRef->IsDisabled() ||
					((originReady || originRef) && (!originRef || originRef->IsDisabled() || originRef->IsDeleted() ||
						(!originRef->IsDead() && (!originRef->AsActorState() || !originRef->AsActorState()->IsReanimated()))))) {
					RemoveHead(headRef);
					return false;
				}

				return true;
			}();

			if (!MiscUtils::IsFormIDValid(headFormID)) return false;
			if (!result) RemoveDecapitation(headFormID);
			return result;
		}

		static bool RemoveHead(RE::Actor* actor)
		{
			using namespace ModData;
			if (!actor) return false;

			actor->RemoveChange(RE::TESObjectREFR::ChangeFlags::kPromoted);
			actor->inGameFormFlags.set(RE::TESForm::InGameFormFlag::kWantsDelete, RE::TESForm::InGameFormFlag::kRefPermanentlyDeleted);
			if (actor->Is3DLoaded()) {
				auto& runtime = actor->GetActorRuntimeData();
				if (runtime.currentProcess && runtime.currentProcess->high) {
					runtime.currentProcess->high->fadeState = RE::HighProcessData::FADE_STATE::kOutDelete;
				}
			}
			TRACE("Removed head reference [REF:{:08X}].", actor->formID);
			return true;
		}

	private:
		static void OnSKSESave(SKSE::SerializationInterface* intfc)
		{
			TRACE("Starting SKSE Save for actorToHeadMap and headToActorMap.");

			if (!intfc->OpenRecord(decapMapId, 1)) {
				logger::critical("Failed to open the DCAP record for serialization.");
				return;
			}

			std::size_t mapSize = actorToHeadMap.size();
			intfc->WriteRecordData(&mapSize, sizeof(mapSize));

			TRACE("    -> Saving actorToHeadMap and headToActorMap with {} entries.", mapSize);

			for (const auto& [actorFormID, entry] : actorToHeadMap) {
				intfc->WriteRecordData(&actorFormID, sizeof(actorFormID));
				intfc->WriteRecordData(&entry.headId, sizeof(entry.headId));
				intfc->WriteRecordData(&entry.gameTime, sizeof(entry.gameTime));
			}

			TRACE("Finished SKSE Save for actorToHeadMap and headToActorMap.");
		}

		static void OnSKSELoad(SKSE::SerializationInterface* intfc)
		{
			TRACE("Starting SKSE Load for actorToHeadMap and headToActorMap.");

			actorToHeadMap.clear();
			headToActorMap.clear();

			std::uint32_t type, version, length;
			while (intfc->GetNextRecordInfo(type, version, length)) {
				if (type != decapMapId || version != 1) continue;

				std::size_t mapSize;
				if (!intfc->ReadRecordData(&mapSize, sizeof(mapSize))) {
					logger::critical("Unable to read the decap map size during deserialization.");
					continue;
				}

				TRACE("    -> Loading actorToHeadMap and headToActorMap with {} entries.", mapSize);

				for (std::size_t i = 0; i < mapSize; ++i) {
					RE::FormID actorFormID, headFormID;
					float gameTime;
			
					if (!intfc->ReadRecordData(&actorFormID, sizeof(actorFormID)) ||
						!intfc->ReadRecordData(&headFormID, sizeof(headFormID)) ||
						!intfc->ReadRecordData(&gameTime, sizeof(gameTime))) {
						logger::critical("Unable to read a decap entry (actor/head/time) at index {}.", i);
						continue;
					}

					actorToHeadMap[actorFormID] = DecapEntry{ headFormID, gameTime };
					headToActorMap[headFormID] = actorFormID;

					TRACE("        -> Loaded decap entry <[ACTOR:{:08X}] [HEAD:{:08X}] [TIME:{:.3f}]>", actorFormID, headFormID, gameTime);
				}
			}

			MaintainDecapitations();

			TRACE("Finished SKSE Load for actorToHeadMap and headToActorMap.");
		}

		static void OnSKSERevert(SKSE::SerializationInterface*)
		{
			actorToHeadMap.clear();
			headToActorMap.clear();
		}

		static void MaintainDecapitations()
		{
			std::vector<RE::FormID> toRemove;

			const RE::Calendar* calendar = RE::Calendar::GetSingleton();
			if (!calendar) REPORT_AND_FAIL("Failed to retrieve RE::Calendar singleton for decapitation timestamp.");
			const float currentGameTime = calendar->GetCurrentGameTime();

			const auto headToActorMapCopy = headToActorMap;
			for (const auto& [headFormID, actorFormID] : headToActorMapCopy) {
				if (!MiscUtils::IsFormIDValid(headFormID) || !MiscUtils::IsFormIDValid(actorFormID)) {
					toRemove.push_back(headFormID);
					continue;
				}

				auto* headRef = MiscUtils::GetValidReference<RE::Actor>(headFormID, true);
				if (!headRef) {
					toRemove.push_back(headFormID);
					continue;
				}

				const auto it = actorToHeadMap.find(actorFormID);
				if (it != actorToHeadMap.end() && ((currentGameTime - it->second.gameTime) > SettingsIni::fHeadDespawnTimeout)) {
					toRemove.push_back(headFormID);
					RemoveHead(headRef);
					continue;
				}

				HeadMaintenanceProcess(headFormID, false);
			}

			for (RE::FormID headFormID : toRemove) {
				auto actorIt = headToActorMap.find(headFormID);
				if (actorIt != headToActorMap.end()) {
					headToActorMap.erase(actorIt);
					RE::FormID actorFormID = actorIt->second;
					if (MiscUtils::IsFormIDValid(actorFormID)) actorToHeadMap.erase(actorFormID);
					TRACE("    -> Removed decapitation entry: Actor [REF:{:08X}] no longer valid. Head [REF:{:08X}] cleaned up.", actorFormID, headFormID);
				}
			}
		}
	};
}
