#pragma once

#include "DataHandler.hpp"
#include "TaskManager.hpp"
#include "Serialization.hpp"
#include "SettingsIni.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"

namespace Events
{
	class ModEventSink :
		public RE::BSTEventSink<RE::TESLoadGameEvent>,
		public RE::BSTEventSink<RE::TESActivateEvent>,
		public RE::BSTEventSink<RE::BGSActorCellEvent>,
		public RE::BSTEventSink<RE::TESDeathEvent>,
		public RE::BSTEventSink<RE::TESEquipEvent>,
		public RE::BSTEventSink<RE::TESContainerChangedEvent>,
		public RE::BSTEventSink<RE::TESResetEvent>,
		public RE::BSTEventSink<RE::TESFormDeleteEvent>
	{
		ModEventSink() = default;
		ModEventSink(const ModEventSink&) = delete;
		ModEventSink(ModEventSink&&) = delete;
		ModEventSink& operator=(const ModEventSink&) = delete;
		ModEventSink& operator=(ModEventSink&&) = delete;

	public:
		#define continueEvent RE::BSEventNotifyControl::kContinue
		
		static inline bool postLoadEventsLoaded = false;

		static ModEventSink* GetSingleton()
		{
			static ModEventSink singleton;
			return &singleton;
		}

		static void LoadEvents()
		{
			auto* eventSink = GetSingleton();
			auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
			eventSourceHolder->AddEventSink<RE::TESLoadGameEvent>(eventSink);
			eventSourceHolder->AddEventSink<RE::TESActivateEvent>(eventSink);
			eventSourceHolder->AddEventSink<RE::TESDeathEvent>(eventSink);
			eventSourceHolder->AddEventSink<RE::TESEquipEvent>(eventSink);
			eventSourceHolder->AddEventSink<RE::TESContainerChangedEvent>(eventSink);
			eventSourceHolder->AddEventSink<RE::TESResetEvent>(eventSink);
			eventSourceHolder->AddEventSink<RE::TESFormDeleteEvent>(eventSink);
		}

		static void TimeBasedCellChange();

		static void LoadEventsPostLoad()
		{
			if (postLoadEventsLoaded) return;

			if (!REL::Module::IsVR()) {
				auto* eventSink = GetSingleton();
				const auto player = RE::PlayerCharacter::GetSingleton();
				if (!player) REPORT_AND_FAIL("PlayerCharacter not found!");
				player->AsBGSActorCellEventSource()->AddEventSink(eventSink);
			} else {
				ModEventSink::TimeBasedCellChange();
			}
			postLoadEventsLoaded = true;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent* event, RE::BSTEventSource<RE::TESLoadGameEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* event, RE::BSTEventSource<RE::TESActivateEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::BGSActorCellEvent* event, RE::BSTEventSource<RE::BGSActorCellEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event, RE::BSTEventSource<RE::TESContainerChangedEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESResetEvent* event, RE::BSTEventSource<RE::TESResetEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* event, RE::BSTEventSource<RE::TESFormDeleteEvent>*);
	};
};
