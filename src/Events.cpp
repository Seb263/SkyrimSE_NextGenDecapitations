#include "Events.h"

namespace Events
{
	void ModEventSink::TimeBasedCellChange()
	{
		std::thread([]() {
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (!player) REPORT_AND_FAIL("PlayerCharacter not found!");
			RE::FormID previousCell = 0x0;

			while (true) {
				SKSE::GetTaskInterface()->AddTask([player, &previousCell]() {
					const auto currentCell = player->GetParentCell() ? player->GetParentCell()->formID : 0x0;
					if (player->Is3DLoaded() && currentCell != previousCell) {
						previousCell = currentCell;
						TRACE("Player moved to new cell: [{:08X}].", currentCell);
						TaskManager::CellsMaintenanceTask();
					}
				});
				std::this_thread::sleep_for(1s);
			}
		}).detach();
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESLoadGameEvent* event, RE::BSTEventSource<RE::TESLoadGameEvent>*)
	{
		TaskManager::ClearActorTasks();

		ModUtils::WaitAndCall(100ms, []() {
			TaskManager::CellsMaintenanceTask();
		}, false);

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::BGSActorCellEvent* event, RE::BSTEventSource<RE::BGSActorCellEvent>*)
	{
		if (event->flags.all(RE::BGSActorCellEvent::CellFlag::kEnter) && event->flags.none(RE::BGSActorCellEvent::CellFlag::kLeave)) {
			TRACE("Player moved to new cell: [{:08X}].", event->cellID);

			ModUtils::WaitAndCall(100ms, []() {
				TaskManager::CellsMaintenanceTask();
			}, false);
		}

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESActivateEvent* event, RE::BSTEventSource<RE::TESActivateEvent>*)
	{
		if (!SettingsIni::bCanBeActivated) return continueEvent;
		if (!event || !event->objectActivated || !event->objectActivated.get() || !event->actionRef || !event->actionRef.get()) return continueEvent;

		RE::Actor* activatedRef = event->objectActivated->As<RE::Actor>();
		if (!activatedRef || !activatedRef->IsDead() || !ModUtils::IsHead(activatedRef)) return continueEvent;

		RE::TESObjectREFR* activatorRef = event->actionRef->As<RE::TESObjectREFR>();
		if (!activatorRef || !activatorRef->IsPlayerRef()) return continueEvent;
		
		if (RE::FormID actorFormID = ModData::Serialization::GetActorFromHead(activatedRef->formID)) {
			if (auto* actorRef = MiscUtils::GetValidReference<RE::Actor>(actorFormID, true)) {
				actorRef->ActivateRef(activatorRef, 0, actorRef->GetObjectReference(), 1, true);
			}
		}

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		RE::Actor* target = event->actorDying->As<RE::Actor>();
		if (!target) return continueEvent;

		auto pauseIfDecapitated = [](RE::Actor* actor) {
			if (actor && actor->Is3DLoaded() && ModUtils::IsDecapitated(actor)) {
				actor->StopCurrentDialogue();
			}
		};

		pauseIfDecapitated(target);

		ModUtils::WaitAndCall(100ms, [targetHandle = target->GetHandle(), pauseIfDecapitated]() {
			auto* actor = MiscUtils::ResolveHandleAs<RE::Actor>(targetHandle);
			pauseIfDecapitated(actor);
		});

		ModUtils::WaitAndCall(500ms, [targetHandle = target->GetHandle()]() {
			auto* target = MiscUtils::ResolveHandleAs<RE::Actor>(targetHandle);
			if (!target) return;

			if (target && ModUtils::IsDecapitated(target)) {
				if (!target->Is3DLoaded() || target->GetActorRuntimeData().criticalStage != RE::ACTOR_CRITICAL_STAGE::kNone) {
					if (auto* rootActor = ModUtils::ClearDecapitatedActor(target)) {
						TaskManager::ClearActorTask(target->formID);
					}
				}
			}
		});

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESContainerChangedEvent* event, RE::BSTEventSource<RE::TESContainerChangedEvent>*)
	{
		if (SettingsIni::bAdvancedNPCMaintenance) [event]() { // Stump Management System
			auto* origin =MiscUtils::GetValidReference<RE::Actor>(event->oldContainer);
			if (!origin || !ModUtils::IsDecapitated(origin)) {
				origin = MiscUtils::GetValidReference<RE::Actor>(event->newContainer);
				if (!origin || !ModUtils::IsDecapitated(origin)) return;
			}
			TaskManager::ProcessStumpTask(origin);
		}();

		if (SettingsIni::bReplicateHeadEquipment) [event]() { // Equipment Management System
			auto* origin = MiscUtils::GetValidReference<RE::Actor>(event->oldContainer);
			RE::TESObjectARMO* item = RE::TESForm::LookupByID<RE::TESObjectARMO>(event->baseObj);

			if (!origin || !item || !ModUtils::IsDecapitated(origin)) return;

			TaskManager::RemoveHeadEquipment(origin, item);
		}();

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*)
	{
		RE::Actor* target = event->actor->As<RE::Actor>();
		if (!target) return continueEvent;
		
		if (ModUtils::IsHead(target)) {
			ModUtils::WaitAndCall(FRAME_DELAY_MS(), [actorFormID = target->formID]() {
				auto* target = MiscUtils::GetValidReference<RE::Actor>(actorFormID, true);
				if (!target) return;

				ModUtils::SetScalesAndCollisions(target);
			});
		}

		if (SettingsIni::bAdvancedNPCMaintenance) TaskManager::ProcessStumpTask(target);

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESResetEvent* event, RE::BSTEventSource<RE::TESResetEvent>*)
	{
		if (!event->object || !event->object.get()) return continueEvent;
		
		RE::Actor* resetActor = event->object->As<RE::Actor>();
		if (!resetActor) return continueEvent;

		if (ModUtils::IsHead(resetActor)) {
			Serialization::RemoveHead(resetActor);
		} else {
			TaskManager::ClearActorTask(resetActor->formID);
			if (auto* headRef = MiscUtils::GetValidReference<RE::Actor>(Serialization::GetHeadFromActor(resetActor->formID))) {
				TRACE("TESResetEvent trigerred on actor [REF:{:08X}] for head [REF:{:08X}].", resetActor->formID, headRef->formID);
				Serialization::HeadMaintenanceProcess(headRef->formID, false, resetActor);
			}
		}

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESFormDeleteEvent* event, RE::BSTEventSource<RE::TESFormDeleteEvent>*)
	{
		if (!event->formID) return continueEvent;

		if (auto* headRef = MiscUtils::GetValidReference<RE::Actor>(Serialization::GetHeadFromActor(event->formID))) {
			TRACE("TESFormDeleteEvent trigerred on actor [REF:{:08X}] for head [REF:{:08X}].", event->formID, headRef->formID);
			Serialization::RemoveHead(headRef);
		}

		return continueEvent;
	}
}
