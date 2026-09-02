#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"
#include "Serialization.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/NiUtils.hpp"

#define FRAME_DELAY_MS() std::chrono::milliseconds(static_cast<int>(std::lround(ModUtils::GetFrameDelay() * 1000.0f)))

class ModUtils
{
public:

	static void EquipActorObject(RE::Actor* target, RE::TESBoundObject* object, const bool forceEquip = true)
	{
		if (!MiscUtils::GetValidReference<RE::Actor>(target, true) || !object) return;

		target->AddObjectToContainer(object, nullptr, 1, target);

		auto equipType = object->As<RE::BGSEquipType>();
		RE::BGSEquipSlot* slot = equipType ? equipType->GetEquipSlot() : nullptr;

		const auto& itemManager = RE::ActorEquipManager::GetSingleton();
		if (!itemManager) REPORT_AND_FAIL("Item Manager could not be initialized.");
		itemManager->EquipObject(target, object, nullptr, 1, slot, false, forceEquip, false, false);

		TRACE("Equipped object: [BASE:{:08X}] on actor [REF:{:08X}]", object->formID, target->formID);
	}

	static void RemoveActorObject(RE::Actor* actor, RE::TESBoundObject* object)
	{
		if (!MiscUtils::GetValidReference<RE::Actor>(actor, true) || !object) return;

		for (const auto& [form, data] : actor->GetInventory()) {
			if (form->formID == object->formID && data.second > 0) {
				actor->RemoveItem(form, data.first, RE::ITEM_REMOVE_REASON::kRemove, &(actor->extraList), nullptr);
				break;
			}
		}
	
		TRACE("Removed object: [BASE:{:08X}] on actor [REF:{:08X}]", object->formID, actor->formID);
	}

	static void UnequipHeadGearExtra(RE::Actor* origin, std::vector<RE::TESObjectARMO*>& unequippedArmors)
	{
		if (!origin) return;

		const auto headNode = origin->GetNodeByName("NPC Head [Head]");
		if (!headNode) return;

		std::unordered_set<std::string> headNodesList;
		RE::BSVisit::TraverseScenegraphObjects(headNode, [&](RE::NiAVObject* a_object) -> RE::BSVisit::BSVisitControl {
			if (a_object) headNodesList.insert(a_object->name.c_str());
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		const auto actorRoot = origin->Get3D();
		if (!actorRoot) return;

		auto getGeometryArmorForm = [](const RE::BSFixedString geometryName) -> RE::TESObjectARMO* {
			std::string_view name(geometryName.data());
			std::size_t      lastOpen = name.rfind('(');
			std::size_t      lastClose = name.rfind(')');

			if (lastOpen != std::string_view::npos && lastClose != std::string_view::npos && lastClose > lastOpen + 1) {
				std::string_view hexStr = name.substr(lastOpen + 1, lastClose - lastOpen - 1);
				if (hexStr.size() == 8 && std::all_of(hexStr.begin(), hexStr.end(), [](char c) {
					return std::isxdigit(static_cast<unsigned char>(c));
				})) {
					RE::FormID formId = static_cast<RE::FormID>(std::stoul(std::string(hexStr), nullptr, 16));
					RE::TESObjectARMO* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(formId);
					if (armor) return armor;
				}
			}
			return nullptr;
		};

		RE::BSVisit::TraverseScenegraphGeometries(actorRoot, [&](RE::BSGeometry* a_geometry) -> RE::BSVisit::BSVisitControl {		
			if (!a_geometry->GetGeometryRuntimeData().skinInstance) return RE::BSVisit::BSVisitControl::kContinue;
		
			RE::TESObjectARMO* armor = getGeometryArmorForm(a_geometry->name);
			if (!armor) return RE::BSVisit::BSVisitControl::kContinue;

			if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kBody) ||
				armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kForearms) ||
				armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kFeet)) {
				return RE::BSVisit::BSVisitControl::kContinue;
			}

			auto skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
			if (!skin || !skin->bones) return RE::BSVisit::BSVisitControl::kContinue;

			for (std::uint32_t i = 0; i < skin->numMatrices; ++i) {
				if (RE::NiAVObject* bone = skin->bones[i]) {
					if (!headNodesList.contains(bone->name.c_str())) continue;
					if (std::find(unequippedArmors.begin(), unequippedArmors.end(), armor) == unequippedArmors.end()) {
						unequippedArmors.push_back(armor);
					}

					break;
				}
			}

			return RE::BSVisit::BSVisitControl::kContinue;
		});
	}

	static std::vector<RE::TESObjectARMO*> UnequipHeadGear(RE::Actor* origin, RE::TESObjectARMO* originHelmet = nullptr)
	{
		using namespace ModData;

		std::vector<RE::TESObjectARMO*> unequippedArmors;
		if (originHelmet) unequippedArmors.push_back(originHelmet);

		if (!origin) return unequippedArmors;

		for (auto& item : origin->GetInventory()) {
			auto& [form, data] = item;
			if (!data.second || !form) continue;

			if (data.second->IsWorn() && form->IsArmor()) {
				if (auto* armor = form->As<RE::TESObjectARMO>()) {
					if (armor == originHelmet || armor == Decapitated_HeadHidden_ARMO ||
						armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kDecapitate)) {
						continue;
					}

					if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHead) ||
						armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHair) ||
						armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kCirclet) ||
						armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kEars)) {

						if (std::find(unequippedArmors.begin(), unequippedArmors.end(), armor) == unequippedArmors.end()) {
							unequippedArmors.push_back(armor);
						}
					}
				}
			}
		}

		if (SettingsIni::bUnequipExtraHeadEquipment) UnequipHeadGearExtra(origin, unequippedArmors);

		if (!unequippedArmors.empty()) {
			auto* actorEquipManager = RE::ActorEquipManager::GetSingleton();
			TRACE("Unequipped {} headgear item(s) from actor [REF:{:08X}]:", unequippedArmors.size(), origin->formID);
			for (auto* armor : unequippedArmors) {
				actorEquipManager->UnequipObject(origin, armor, nullptr, 1, armor->GetEquipSlot(), true, false, false, true);
				TRACE("    -> Unequipped <\"{}\" [BASE:{:08X}]>", armor->GetName(), armor->formID);
			}
		}

		return unequippedArmors;
	}

	enum DecapArmorPart {
		Neck,
		Head,
		HeadHidden
	};
	static RE::TESObjectARMO* GetArmorPart(RE::Actor* actor, DecapArmorPart part)
	{
		using namespace ModData;

		if (part == DecapArmorPart::HeadHidden) return Decapitated_HeadHidden_ARMO;

		if (!actor) return nullptr;

		RE::TESRace* actorRace = actor->GetRace();
		if (!actorRace) return nullptr;

		switch (part) {
		case DecapArmorPart::Neck:
			if (actorRace->formEditorID == "DA13AfflictedRace") return Decapitated_Neck_Afflicted_ARMO;
			else return Decapitated_Neck_ARMO;
			break;
		case DecapArmorPart::Head:
			if (actorRace->formEditorID == "DA13AfflictedRace") return Decapitated_Head_Afflicted_ARMO;
			else return Decapitated_Head_ARMO;
			break;
		}

		return nullptr;
	}

	static RE::Actor* ClearDecapitatedActor(RE::Actor* actor, const bool preserveVisuals = false)
	{
		using namespace ModData;

		if (!actor) return nullptr;

		RE::Actor* bodyRef = nullptr;
		RE::Actor* headRef = nullptr;
		if (RE::FormID actorFormID = Serialization::GetActorFromHead(actor->formID)) {
			headRef = actor;
			bodyRef = MiscUtils::GetValidReference<RE::Actor>(actorFormID);
		} else if (RE::FormID headFormID = Serialization::GetHeadFromActor(actor->formID)) {
			bodyRef = actor;
			headRef = MiscUtils::GetValidReference<RE::Actor>(headFormID);
		} else return nullptr;

		if (!preserveVisuals) {
			if (headRef) Serialization::RemoveHead(headRef);
			Serialization::RemoveDecapitation(actor->formID);

			if (bodyRef) ModUtils::WaitAndCall(1s, [bodyHandle = bodyRef->GetHandle()]() {
				if (auto* bodyRef = MiscUtils::GetValidReference<RE::Actor>(MiscUtils::ResolveHandleAs<RE::Actor>(bodyHandle), true)) {
					ModUtils::RemoveActorObject(bodyRef, ModUtils::GetArmorPart(bodyRef, ModUtils::DecapArmorPart::Neck));
					ModUtils::RemoveActorObject(bodyRef, ModUtils::GetArmorPart(bodyRef, ModUtils::DecapArmorPart::HeadHidden));
				}
			});
		}

		return bodyRef;
	}
	
	static bool IsDecapitated(RE::Actor* actor)
	{
		if (!actor) return false;

		if (auto* neckArmor = ModUtils::GetArmorPart(actor, ModUtils::DecapArmorPart::Neck)) {
			if (actor->GetWornArmor(neckArmor->formID, true)) return true;
		}

		return ModData::actorToHeadMap.contains(actor->formID) || ModData::headToActorMap.contains(actor->formID);
	}
	
	static bool IsHead(RE::Actor* actor)
	{
		if (!actor) return false;
		if (ModData::headToActorMap.contains(actor->formID)) return true;

		if (auto actorAv = actor->AsActorValueOwner()) {
			if (actorAv->GetActorValue(RE::ActorValue::kVariable01) == ModData::ngdValueMarker) {
				return true;
			}
		}
		return false;
	}

	static void LocalizedHeadScaleUpdate(RE::NiAVObject* targetAVStart, RE::NiAVObject* targetNode = nullptr)
	{
		if (!targetAVStart) return;
		if (!targetNode) targetNode = targetAVStart;

		if (auto* hkpRigidBody = NiUtils::GetRigidBody(targetNode)) {
			if (targetNode == targetAVStart) {
				hkpRigidBody->motion.SetMass(SettingsIni::fHeadMass);
			} else {
				hkpRigidBody->motion.SetMass(0.01f);
				hkpRigidBody->motion.gravityFactor = 0.0f;

				auto& collisionFilterInfo = hkpRigidBody->collidable.broadPhaseHandle.collisionFilterInfo;
				collisionFilterInfo.SetCollisionLayer(ModData::NonCollidableLayer);

				if (targetNode->name == "NPC Head [Head]") {
					if (auto actorRef = targetNode->GetUserData()) {
						if (RE::NiAVObject* headRoot = actorRef->GetNodeByName("NPC Root [Root]")) {
							headRoot->local.scale = 0.01f;
							targetNode->local.scale = 100.0f;

							NiUtils::UpdateNode(targetNode);
							NiUtils::UpdateNode(headRoot);
						}
					}
				}
			}
		}

		if (targetNode->name != "NPC Head [Head]") {
			if (RE::NiNode* niNode = targetNode->AsNode()) {
				for (auto& child : niNode->GetChildren()) {
					if (child && child.get()) {
						LocalizedHeadScaleUpdate(targetAVStart, child.get());
					}
				}
			}
		}
	}

	static void SetScalesAndCollisions(RE::TESObjectREFR* targetRef)
	{
		if (!targetRef) return;

		TRACE("    -> Set Scales and Collisions process on actor: <\"{}\" [REF:{:08X}]>", targetRef->GetName(), targetRef->formID);

		ModUtils::DoFor(FRAME_DELAY_MS() * SettingsIni::iScalesUpdateFrequencyFactor, 1s, []() { return false; },
			[cloneHandle = targetRef->GetHandle()]() {
				RE::Actor* clone = MiscUtils::ResolveHandleAs<RE::Actor>(cloneHandle);
				if (!clone) return;

				if (RE::NiAVObject* targetAVStart = clone->GetNodeByName("NPC COM [COM ]")) {
					LocalizedHeadScaleUpdate(targetAVStart);
				}

			}, [cloneHandle = targetRef->GetHandle()]() {
				if (RE::Actor* clone = MiscUtils::ResolveHandleAs<RE::Actor>(cloneHandle)) {
					RE::NiAVObject* targetRoot = clone->GetNodeByName("NPC Root [Root]");
					if (targetRoot && targetRoot->local.scale < 0.02f) {
						TRACE("    -> Set Scales and Collisions DONE for actor: <\"{}\" [REF:{:08X}]>", clone->GetName(), clone->formID);
						return;
					}

					logger::warn( "    -> Set Scales and Collisions SKIPPED for actor: <\"{}\" [REF:{:08X}]>, unexpected scale value: {}",
						clone->GetName(), clone->formID, targetRoot ? std::to_string(targetRoot->local.scale) : "undefined"
					);

					ModData::Serialization::RemoveHead(clone);
				} else {
					logger::warn("    -> Actor not found, unable to set scales and collisions.");
				}
			}
		);
	}

	template <typename TInterval, typename TDuration, typename TCallback, typename TEndCallback = std::function<void()>>
	static void DoFor(TInterval interval, TDuration duration, std::function<bool()> stopCondition = []() { return false; },
					  TCallback&& callback = []() {}, TEndCallback&& endCallback = []() {}, const bool pausable = true, const bool secureFrame = true)
	{
		std::jthread([=]() mutable {
			WaitForGameReady();
			auto failure = std::make_shared<std::atomic_bool>(false);
			auto conditionMet = std::make_shared<std::atomic_bool>(false);
			auto deadline = std::chrono::steady_clock::now() + duration;

			while (true) {
				if (pausable) {
					auto beforePause = std::chrono::steady_clock::now();
					if (WaitForGameReady(true)) deadline += (std::chrono::steady_clock::now() - beforePause);
				}

				std::chrono::nanoseconds currentInterval;
				if constexpr (std::is_invocable_v<TInterval>) currentInterval = interval();
				else currentInterval = interval;

				const bool last = (std::chrono::steady_clock::now() + currentInterval >= deadline);

				SKSE::GetTaskInterface()->AddTask([=, lastInner = last, taskStart = std::chrono::steady_clock::now()]() {
					if (secureFrame && std::chrono::steady_clock::now() - taskStart > 300ms) {
						if (!*failure) {
							*failure = true;
							TRACE("DoFor: Task was delayed and invalidated due to frame timing (>{}ms)", 300);
						}
					}
					if (*failure || *conditionMet) return;

					callback();
					*conditionMet = (stopCondition && stopCondition());
					if (lastInner || *conditionMet) {
						auto remaining = (*conditionMet ? 0ns : deadline - std::chrono::steady_clock::now());
						ModUtils::WaitAndCall(remaining > 0ns ? remaining : 0ns, [endCallback]() { endCallback(); }, secureFrame);
					}
				});

				std::this_thread::sleep_for(currentInterval);
				if (last || *conditionMet) break;
			}
		}).detach();
	}

	template <typename TDuration, typename TCallback>
	static void WaitAndCall(TDuration delay, TCallback&& callback, const bool secureFrame = true)
	{
		std::jthread([delay, callback = std::forward<TCallback>(callback), secureFrame]() mutable {
			WaitForGameReady();
			auto failure = std::make_shared<std::atomic_bool>(false);
			const auto deadline = (std::chrono::steady_clock::now() + delay);

			while (true) {
				auto remaining = (deadline - std::chrono::steady_clock::now());
				std::this_thread::sleep_for(remaining > 100ms ? 100ms : (remaining > 0ns ? remaining : FRAME_DELAY_MS()));

				const bool last = (std::chrono::steady_clock::now() >= deadline);
				SKSE::GetTaskInterface()->AddTask([callback, secureFrame, failure, last, taskStart = std::chrono::steady_clock::now()]() {
					if (secureFrame && std::chrono::steady_clock::now() - taskStart > 300ms) *failure = true;
					if (last) {
						if (*failure) TRACE("WaitAndCall: Task was delayed and invalidated due to frame timing (>{}ms)", 300);
						else callback();
					}
				});
				if (last) break;
			}

		}).detach();
	}

	template <typename TCallback>
	static void WaitUntilRagdollReady(RE::TESObjectREFR* ref, TCallback&& callback, std::chrono::milliseconds timeout = 3s, const bool secureFrame = true)
	{
		if (!ref) {
			callback(ref, false);
			return;
		}

		std::jthread([formId = ref->formID, callback = std::forward<TCallback>(callback), timeout, secureFrame]() mutable {
			WaitForGameReady();
			const auto start = std::chrono::steady_clock::now();
			while (std::chrono::steady_clock::now() - start < timeout) {
				auto* ref = MiscUtils::GetValidReference<RE::TESObjectREFR>(formId, true);
				if (ref && NiUtils::IsReferenceRagdollReady(ref)) {
					SKSE::GetTaskInterface()->AddTask([callback, ref, secureFrame, start = std::chrono::steady_clock::now()]() {
						if (secureFrame && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > 300) {
							TRACE("WaitUntilRagdollReady: Task was delayed and invalidated due to frame timing (>{}ms)", 300);
							return;
						}
						callback(ref, true);
					});
					return;
				}
				std::this_thread::sleep_for(FRAME_DELAY_MS());
			}
        
			SKSE::GetTaskInterface()->AddTask([callback, formId, secureFrame, start = std::chrono::steady_clock::now()]() {
				if (secureFrame && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > 300) {
					TRACE("WaitUntilRagdollReady: Task was delayed and invalidated due to frame timing (>{}ms)", 300);
					return;
				}
				auto* ref = MiscUtils::GetValidReference<RE::TESObjectREFR>(formId);
				callback(ref, false);
			});
		}).detach();
	}

	static bool WaitForGameReady(bool ignoreLoadingMenu = false)
	{
		bool wasPaused = false;

		while (true) {
			if (auto ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
				static auto loadingMenu = ui->GetMenu("Loading Menu");
				if (ignoreLoadingMenu && ui->numPausesGame == 1 && loadingMenu && loadingMenu->OnStack()) break;

				std::this_thread::sleep_for(FRAME_DELAY_MS());
				wasPaused = true;
				continue;
			}

			std::promise<void> p;
			auto f = p.get_future();

			SKSE::GetTaskInterface()->AddTask([&p]() { p.set_value(); });
        
			auto start = std::chrono::high_resolution_clock::now();
			f.get();

			if ((std::chrono::high_resolution_clock::now() - start) > 100ms) {
				wasPaused = true;
				continue;
			}

			break;
		}

		return wasPaused;
	}

	static float GetFrameDelay()
	{
		RE::BSTimer* bsTimer = RE::BSTimer::GetSingleton();
		if (!bsTimer) return 0.00694444f; // 144Hz

		float frame_delay = bsTimer->realTimeDelta / bsTimer->QGlobalTimeMultiplier();
		frame_delay = std::clamp(frame_delay, 0.004f, 0.1f);

		return frame_delay;
	}
};
