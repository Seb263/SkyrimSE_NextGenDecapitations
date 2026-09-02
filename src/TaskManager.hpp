#pragma once

#include "DataHandler.hpp"
#include "Serialization.hpp"
#include "SettingsIni.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"

namespace Events
{
	using namespace ModData;

	class TaskManager
	{
	public:

		static void CellsMaintenanceTask()
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (!player) return;

			TRACE("Process Cells Maintenance Task.");

			const auto& playerPosition = RE::NiPoint3(player->GetPositionX(), player->GetPositionY(), 0.0f);
			const auto processActor = [](RE::Actor* ref) {
				if (!ref || !ref->GetParentCell() || !ref->GetParentCell()->IsAttached()) return;
			
				if (ModUtils::IsDecapitated(ref)) {
					ExecuteStumpTask(ref->formID);
                    if (ModUtils::IsHead(ref) && Serialization::HeadMaintenanceProcess(ref->formID, true)) {
                         ModUtils::SetScalesAndCollisions(ref);
                    }
				}
			};

			const auto processCell = [&](RE::TESObjectCELL* cell) {
				if (!cell || !cell->IsAttached() ||
					(cell->IsExteriorCell() && playerPosition.GetDistance({ cell->GetCoordinates()->worldX, cell->GetCoordinates()->worldY, 0.0f }) > 8192.0f)) return;
				
				TRACE("    -> Maintenance on cell [{:08X}]", cell->formID);

				cell->ForEachReference([&](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
					processActor(ref ? ref->As<RE::Actor>() : nullptr);
					return RE::BSContainer::ForEachResult::kContinue;
				});
			};

			auto* gridCells = RE::TES::GetSingleton()->gridCells;
			if (gridCells && gridCells->cells) {
				for (std::size_t i = 0; i < gridCells->length * gridCells->length; ++i) {
					processCell(gridCells->cells[i]);
				}
			}
			processCell(RE::TES::GetSingleton()->interiorCell);

			TRACE("Ended Cells Maintenance Task.");
		}

		struct TaskState
		{
			uint8_t completedIterations = 0;
		};

		static inline std::shared_mutex taskMutex;
		static inline std::unordered_map<RE::FormID, TaskState> actorTasks;

		static constexpr auto TASK_COOLDOWN = std::chrono::milliseconds(1000);
		static constexpr int  MAX_ITERATIONS = 10;

		static void ProcessStumpTask(RE::Actor* actor)
		{
			if (!actor || !ModUtils::IsDecapitated(actor)) return;
			RE::FormID actorFormID = actor->formID;

			{
				std::lock_guard lock(taskMutex);
				if (actorTasks.find(actorFormID) != actorTasks.end()) return;
				actorTasks[actorFormID] = { 0 };
			}

			std::jthread([actorFormID]() {
				{
					std::shared_lock lock(taskMutex);
					if (!actorTasks.contains(actorFormID)) return;
				}

				TRACE("Process Stump Task on actor [REF:{:08X}].", actorFormID);

				ModUtils::WaitForGameReady();

				const auto startTime = std::chrono::steady_clock::now();
				const auto endTime = startTime + TASK_COOLDOWN;
				while (std::chrono::steady_clock::now() < endTime) {
					{
						std::shared_lock lock(taskMutex);
						auto it = actorTasks.find(actorFormID);
						if (it == actorTasks.end()) break;
						if (it->second.completedIterations >= MAX_ITERATIONS) break;
					}
					
					if (auto ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) break;
					
					ExecuteStumpTask(actorFormID);

					std::this_thread::sleep_for(50ms);
				}

				{
					std::lock_guard lock(taskMutex);
					actorTasks.erase(actorFormID);
				}

				TRACE("Ended Stump Task on actor [REF:{:08X}].", actorFormID);
			}).detach();
		}

		static void ClearActorTask(RE::FormID actorFormID)
		{
			std::lock_guard lock(taskMutex);
			actorTasks.erase(actorFormID);
		}

		static void ClearActorTasks()
		{
			std::lock_guard lock(taskMutex);
			actorTasks.clear();
		}

		static void RemoveHeadEquipment(RE::Actor* actor, RE::TESObjectARMO* item)
		{
			if (!actor || !item) return;
			if (item == Decapitated_HeadHidden_ARMO || item == Decapitated_Head_ARMO || item == Decapitated_Head_Afflicted_ARMO) return;

			if (RE::FormID headFormID = ModData::Serialization::GetHeadFromActor(actor->formID)) {
				if (auto* headRef = MiscUtils::GetValidReference<RE::Actor>(headFormID, true)) {
					int itemCount = 0;
					for (const auto& [invItem, data] : headRef->GetInventory()) {
						const auto& [count, entry] = data;
						if (invItem->formID == item->formID) {
							itemCount = count;
							break;
						}
					}

					headRef->RemoveItem(item, itemCount ? itemCount : 1, RE::ITEM_REMOVE_REASON::kRemove, &(headRef->extraList), nullptr);
					if (itemCount > 0) TRACE("Removed {} [FORM:{:08X}] item(s) from actor [REF:{:08X}].", itemCount, item->formID, headRef->formID);
				}
			}
		}

	private:

		static void ExecuteStumpTask(RE::FormID actorFormID)
		{
			SKSE::GetTaskInterface()->AddTask([actorFormID, start = std::chrono::steady_clock::now()]() {
				if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > 300) return;

				using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
				auto* actor = MiscUtils::GetValidReference<RE::Actor>(actorFormID, true);

				auto equipArmorIfNeeded = [&](Slot slot, std::optional<RE::TESObjectARMO*> armorOpt) {
					if (!armorOpt.has_value()) return false;
					
					RE::TESObjectARMO* armor = armorOpt.value();
					if (actor->GetWornArmor(slot) != armor) {
						ModUtils::WaitAndCall(FRAME_DELAY_MS(), [actor, armor]() {
							ModUtils::EquipActorObject(actor, armor);
						});
						TRACE("    -> Reequiped stump [ARMO:{:08X}] on actor [REF:{:08X}].", armor->formID, actorFormID);
						return true;
					}
					return false;
				};

				if (actor) {
					bool taskCompleted = false;
					if (ModUtils::IsHead(actor)) {
						if (equipArmorIfNeeded(Slot::kDecapitateHead, ModUtils::GetArmorPart(actor, ModUtils::DecapArmorPart::Head))) {
							ModUtils::SetScalesAndCollisions(actor);
							taskCompleted = true;
						}
					} else {
						taskCompleted = equipArmorIfNeeded(Slot::kDecapitate, ModUtils::GetArmorPart(actor, ModUtils::DecapArmorPart::Neck));
						if (equipArmorIfNeeded(Slot::kHair, ModUtils::GetArmorPart(actor, ModUtils::DecapArmorPart::HeadHidden))) {
							ModUtils::UnequipHeadGear(actor);
							taskCompleted = true;
						}
					}

					if (taskCompleted) {
						std::lock_guard lock(taskMutex);
						auto it = actorTasks.find(actorFormID);
						if (it != actorTasks.end()) it->second.completedIterations++;
					}
				} else {
					std::lock_guard lock(taskMutex);
					auto it = actorTasks.find(actorFormID);
					if (it != actorTasks.end()) it->second.completedIterations += MAX_ITERATIONS;
				}
			});
		}
	};
}
