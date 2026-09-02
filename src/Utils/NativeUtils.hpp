#pragma once

class NativeUtils
{
	public:

	static void PlayArtObject(RE::TESObjectREFR* a_target, RE::BGSArtObject* a_artObject, float a_lifetime, RE::TESObjectREFR* a_facingObject = nullptr, bool a_bAttachToCamera = false, bool a_bInheritRotation = false, void* unk01 = nullptr, void* unk02 = nullptr)
	{
		using func_t = decltype(&PlayArtObject);
		REL::Relocation<func_t> func{ RELOCATION_ID(22289, 22769) };
		return func(a_target, a_artObject, a_lifetime, a_facingObject, a_bAttachToCamera, a_bInheritRotation, unk01, unk02);
	}

	static void MoveToImpl(RE::TESObjectREFR* base, const RE::ObjectRefHandle& a_targetHandle, RE::TESObjectCELL* a_targetCell, RE::TESWorldSpace* a_selfWorldSpace, const RE::NiPoint3& a_position = {}, const RE::NiPoint3& a_rotation = {})
	{
		using func_t = decltype(&MoveToImpl);
		static REL::Relocation<func_t> func{ RELOCATION_ID(56227, 56626) };
		return func(base, a_targetHandle, a_targetCell, a_selfWorldSpace, a_position, a_rotation);
	}

	static RE::TESObjectREFR* PlaceAtMe(RE::TESObjectREFR* self, RE::TESForm* a_form, std::uint32_t count = 1, bool forcePersist = false, bool initiallyDisabled = false)
	{
		using func_t = RE::TESObjectREFR* (RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, RE::TESObjectREFR*, RE::TESForm*, std::uint32_t, bool, bool);
		RE::VMStackID frame = 0;

		REL::Relocation<func_t> func{ RELOCATION_ID(55672, 56203) };
		auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();

		return func(vm, frame, self, a_form, count, forcePersist, initiallyDisabled);
	};

	static RE::TESObjectREFR* PlaceAtMe(RE::TESObjectREFR* ref, RE::TESForm* baseForm, RE::NiPoint3 position, RE::NiPoint3 angle = RE::NiPoint3(), bool forcePersist = false)
	{
		const auto boundObject = baseForm->As<RE::TESBoundObject>();
		if (!boundObject || !ref) return nullptr;

		const auto handle = RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(boundObject, position, angle, ref->GetParentCell(), ref->GetWorldspace(), nullptr, nullptr, RE::ObjectRefHandle(), forcePersist, true);
		const auto handlePtr = handle.get();
		return (handlePtr && handlePtr.get() ? handlePtr.get() : nullptr);
	};
};
