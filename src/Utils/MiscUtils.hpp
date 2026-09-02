#pragma once

class MiscUtils
{
	public:

	template <typename Container>
	static Container SplitString(const std::string& str, char delimiter, bool removeSpaces = true, bool toLower = false)
	{
		Container         tokens;
		std::stringstream ss(str);
		std::string       token;

		while (std::getline(ss, token, delimiter)) {
			if (removeSpaces) {
				token.erase(0, token.find_first_not_of(' '));
				token.erase(token.find_last_not_of(' ') + 1);
			}

			if (toLower) {
				std::transform(token.begin(), token.end(), token.begin(), ::tolower);
			}

			if (!token.empty()) {
				if constexpr (std::is_same_v<Container, std::unordered_set<std::string>>) {
					tokens.insert(token);
				} else {
					tokens.push_back(token);
				}
			}
		}
		return tokens;
	}
	
	template <typename T, typename HandleT>
	static T* ResolveHandleAs(const HandleT& handle)
	{
		auto ptr = handle ? handle.get() : nullptr;
		if (!ptr) return nullptr;

		return ptr->As<T>();
	}

	static bool IsFormIDValid(const RE::FormID formID)
	{
		return (formID > 0x0 && formID < 0xFFFFFFFF);
	}

	template <typename T>
	static T* GetValidReference(RE::FormID formID, const bool extraChecks = false)
	{
		if (!MiscUtils::IsFormIDValid(formID)) return nullptr;
		return GetValidReference<T>(RE::TESForm::LookupByID<RE::TESObjectREFR>(formID), extraChecks);
	}

	template <typename T>
	static T* GetValidReference(RE::TESObjectREFR* ref, const bool extraChecks = false)
	{
		using namespace ModData;

		if (!ref || !ref->As<T>() || !MiscUtils::IsFormIDValid(ref->formID) || ref->IsDeleted()) return nullptr;
		
		if (extraChecks) {
			if (ref->IsDisabled() || ref->IsMarkedForDeletion()) return nullptr;
		}

		if constexpr (std::is_same_v<T, RE::Actor>) {
			auto* refActor = ref->As<RE::Actor>();
			if (!refActor || !ref->Is(RE::FormType::ActorCharacter)) return nullptr;
			
			if (extraChecks && (refActor->GetActorRuntimeData().criticalStage != RE::ACTOR_CRITICAL_STAGE::kNone)) return nullptr;
		}

		return ref->As<T>();
	}

	static RE::TESNPC* GetTraitTemplate(RE::TESNPC* baseForm)
	{
		auto npc = baseForm;
		if (!npc) return nullptr;

		while (npc->faceNPC && npc->formID >= 0xFF000000) {
			npc = npc->faceNPC;
		}

		return npc;
	}

	static float GetRandomNumber(float min = 0.0f, float max = 1.0f)
	{
		static std::mt19937                   generator(std::random_device{}());
		std::uniform_real_distribution<float> distribution(min, max);
		return distribution(generator);
	}
};
