#pragma once

#include "DataHandler.hpp"
#include "Main.hpp"
#include "Serialization.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"

namespace Papyrus
{
	bool Decapitate(RE::StaticFunctionTag*, RE::Actor* actor)
	{
		if (!actor) return false;

		Events::MainEvent::DecapitateParams f_params;
		return Events::MainEvent::ProceedDecapitation(actor, f_params);
	}
	
	bool DecapitateNoHead(RE::StaticFunctionTag*, RE::Actor* actor)
	{
		if (!actor) return false;

		Events::MainEvent::DecapitateParams f_params;
		return Events::MainEvent::ProceedDecapitation(actor, f_params, true);
	}

	std::vector<uint32_t> GetNGDVersion(RE::StaticFunctionTag*)
	{
		using namespace SKSE;
        const auto* plugin = PluginDeclaration::GetSingleton();
        auto version = plugin->GetVersion();

        uint32_t versionMajor = plugin->GetVersion().major();
        uint32_t versionMinor = plugin->GetVersion().minor();
        uint32_t versionPatch = plugin->GetVersion().patch();

		std::vector<uint32_t> versionVector;
		versionVector.push_back(versionMajor);
		versionVector.push_back(versionMinor);
		versionVector.push_back(versionPatch);

		return versionVector;
	}

	bool IsDecapitated(RE::StaticFunctionTag*, RE::Actor* actor)
	{
		if (!actor) return false;

		return ModUtils::IsDecapitated(actor);
	}

	bool IsHead(RE::StaticFunctionTag*, RE::Actor* actor)
	{
		if (!actor) return false;

		return ModUtils::IsHead(actor);
	}

	RE::Actor* GetHead(RE::StaticFunctionTag*, RE::Actor* actor)
	{
		if (!actor) return nullptr;
		if (ModUtils::IsHead(actor)) return actor;

		RE::FormID headFormID = ModData::Serialization::GetHeadFromActor(actor->formID);
		auto* headRef = MiscUtils::GetValidReference<RE::Actor>(headFormID, true);

		return headRef ? headRef : nullptr;
	}

	bool RemoveHead(RE::StaticFunctionTag*, RE::Actor* actor)
	{
		if (!actor) return false;

		RE::Actor* headRef;
		if (ModUtils::IsHead(actor)) {
			headRef = actor;
		} else {
			RE::FormID headFormID = ModData::Serialization::GetHeadFromActor(actor->formID);
			headRef = MiscUtils::GetValidReference<RE::Actor>(headFormID);
		}

		if (headRef) {
			if (ModData::Serialization::RemoveHead(headRef)) {
				ModData::Serialization::RemoveDecapitation(headRef->formID);
				return true;
			}
		}
		return false;
	}

	bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm)
	{
		vm->RegisterFunction("Decapitate", "NGDecapitations", Decapitate);
		vm->RegisterFunction("DecapitateNoHead", "NGDecapitations", DecapitateNoHead);
		vm->RegisterFunction("GetNGDVersion", "NGDecapitations", GetNGDVersion);
		vm->RegisterFunction("IsDecapitated", "NGDecapitations", IsDecapitated);
		vm->RegisterFunction("IsHead", "NGDecapitations", IsHead);
		vm->RegisterFunction("GetHead", "NGDecapitations", GetHead);
		vm->RegisterFunction("RemoveHead", "NGDecapitations", RemoveHead);
		return true;
	}
};
