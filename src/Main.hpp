#pragma once

#include "DataHandler.hpp"
#include "MainFunctions.hpp"
#include "SettingsIni.hpp"
#include "Serialization.hpp"
#include "TaskManager.hpp"

#include "API/DF-API.h"
#include "API/DDO-API.h"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"
#include "Utils/NativeUtils.hpp"
#include "Utils/NiUtils.hpp"

namespace Events
{
	using namespace ModData;

	class MainEvent
	{
		public:

			using DecapitateParams = NGDecapitationsAPI::DecapitateParams;

			static void InstallHooks()
			{
				REL::Relocation<std::uintptr_t> decapitateHandlerVtbl{ RE::VTABLE_DecapitateHandler[0] };
				_DecapitateHandler = decapitateHandlerVtbl.write_vfunc(0x1, OnDecapitateTemplate);
				logger::info("DecapitateHandler hooked at virtual table index 0x1. Address: 0x{:X}", _DecapitateHandler.address());

				REL::Relocation<std::uintptr_t> characterVtbl{ RE::VTABLE_Character[0] };
				_ResurrectHandler = characterVtbl.write_vfunc(REL::Module::IsVR() ? 0x0AD : 0x0AB, ResurrectHookTemplate);
				logger::info("ResurrectHandler hooked at virtual table index 0x1. Address: 0x{:X}", _ResurrectHandler.address());

				REL::Relocation<std::uintptr_t> reanimateVtbl{ RE::VTABLE_ReanimateEffect[0] };
				_ReanimateHandler = reanimateVtbl.write_vfunc(0x14, ReanimateHookTemplate);
				logger::info("ReanimateHandler hooked at address: 0x{:X}", _ReanimateHandler.address());
			};

			static bool ProceedDecapitation(RE::Actor* actor, DecapitateParams params, const bool noHead = false)
			{
				if (!actor || !MainFunctions::TestClothing(actor)) return false;

				SKSE::GetTaskInterface()->AddTask([actor, params, noHead]() {
					OnDecapitate_Step01(actor, params, noHead);
				});

				return true;
			};

		private:
			static inline REL::Relocation<void (*)(void*, RE::Actor*, void*, void*)> _DecapitateHandler;
			static inline REL::Relocation<void (*)(RE::Character*, bool, bool)> _ResurrectHandler;
			static inline REL::Relocation<void (*)(RE::ReanimateEffect*)> _ReanimateHandler;

			static void ResurrectHookTemplate(RE::Character* a_this, bool a_resetInventory, bool a_attach3D)
			{
				if (a_this) {
					if (auto* actor = a_this->As<RE::Actor>()) {
						if (auto* rootActor = ModUtils::ClearDecapitatedActor(actor)) {
							TaskManager::ClearActorTask(actor->formID);
							if (rootActor->As<RE::Character>()) a_this = rootActor->As<RE::Character>();
						}
					}
				}

				_ResurrectHandler(a_this, a_resetInventory, a_attach3D);
			}

			static void ReanimateHookTemplate(RE::ReanimateEffect* a_this)
			{
				auto cancelReanimate = [&]() -> void {
					a_this->conditionStatus = RE::ActiveEffect::ConditionStatus::kFalse;
					a_this->flags.set(RE::ActiveEffect::Flag::kDispelled);
					a_this->flags.set(RE::ActiveEffect::Flag::kInactive);
					a_this->magnitude = 0.0f;
					a_this->duration = 0.0f;
				};

				auto altReanimate = [&](RE::Actor* caster, RE::Actor* target) -> bool {
					if (!target || !ModUtils::IsDecapitated(target)) return true;

					if (SettingsIni::iCanBeResurrected < 1) return false;

					if (auto* rootActor = ModUtils::ClearDecapitatedActor(target, SettingsIni::iCanBeResurrected < 2)) {
						if (ModUtils::IsHead(target) && target != rootActor) {
							if (auto* magicCaster = rootActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
								magicCaster->CastSpellImmediate(a_this->spell, false, rootActor, 1.0, false, 0.0, caster);
							}
							return false;
						}
					}
					return true;
				};

				if (a_this && a_this->caster && a_this->commandedActor && a_this->spell) {
					auto* caster = MiscUtils::ResolveHandleAs<RE::Actor>(a_this->caster);
					auto* target = MiscUtils::ResolveHandleAs<RE::Actor>(a_this->commandedActor);
					if (caster && target && !altReanimate(caster, target)) {
						cancelReanimate();
						return;
					}
				}

				_ReanimateHandler(a_this);
			}

			static void OnDecapitateTemplate(void* unk01, RE::Actor* actor, void* unk02, void* unk03)
			{
				OnDecapitate_Step01(actor);

				_DecapitateHandler(nullptr, actor, nullptr, nullptr);
			};

			static void DecapitateNoHead(RE::Actor* origin)
			{
				NativeUtils::PlayArtObject(origin, NeckBloodFX_AO, 5.0f);
				ModUtils::UnequipHeadGear(origin);

				ModUtils::EquipActorObject(origin, ModUtils::GetArmorPart(origin, ModUtils::DecapArmorPart::Neck));
				ModUtils::EquipActorObject(origin, ModUtils::GetArmorPart(origin, ModUtils::DecapArmorPart::HeadHidden));

				origin->StopCurrentDialogue();

				TaskManager::ProcessStumpTask(origin);
			}

			static inline std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> lastExecutionTimes;
			static inline std::mutex executionMutex;
			static RE::TESObjectREFR* OnDecapitate_Step01(RE::Actor* origin, DecapitateParams params = DecapitateParams(), const bool noHead = false)
			{
				if (!origin || origin->formType != RE::FormType::ActorCharacter) return nullptr;

				auto currentTime = std::chrono::steady_clock::now();
				std::lock_guard<std::mutex> lock(executionMutex);
				{
					if (lastExecutionTimes.find(origin->formID) != lastExecutionTimes.end()) {
						auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastExecutionTimes[origin->formID]);
						if (elapsed.count() < 1) return nullptr;
					}
					lastExecutionTimes[origin->formID] = currentTime;
				}

				if (ModUtils::IsDecapitated(origin) || ModUtils::IsHead(origin) || !MainFunctions::TestClothing(origin)) return nullptr;

				if (!MainFunctions::TestRace(origin)) {
					origin->Decapitate();
					return nullptr;
				}

				if (!params.customHitData && FurnitureExecutioner_Keyword) {
					params.motionless = [origin]() -> bool {
						if (RE::TESObjectREFR* furnitureRef = origin->GetOccupiedFurniture() ? origin->GetOccupiedFurniture().get().get() : nullptr) {
							if (RE::TESFurniture* furniture = furnitureRef->GetBaseObject() ? furnitureRef->GetBaseObject()->As<RE::TESFurniture>() : nullptr) {
								return furniture->HasKeyword(FurnitureExecutioner_Keyword);
							}
						}

						return false;
					}();
				}

				if (noHead) {
					DecapitateNoHead(origin);
					return nullptr;
				}

				RE::TESNPC* baseActorForm = origin->GetActorBase();
				if (!baseActorForm) return nullptr;

				baseActorForm = MiscUtils::GetTraitTemplate(baseActorForm);
				if (!baseActorForm || baseActorForm->formID >= 0xFF000000) return nullptr;

				auto* clone = MiscUtils::GetValidReference<RE::Actor>(NativeUtils::PlaceAtMe(origin, baseActorForm));
				if (!clone) return nullptr;

				const bool showHelmet = MainFunctions::ShowHelmet(origin);
				RE::TESObjectARMO* equippedHelmet = showHelmet ? MainFunctions::GetActorHelmet(origin) : nullptr;
				RE::FormID equippedHelmetFormID = equippedHelmet ? equippedHelmet->formID : 0x0;

				MainFunctions::FixAlphaState(clone);
				MainFunctions::SetHeadPosition(origin, clone, true);
				NativeUtils::PlayArtObject(origin, NeckBloodFX_AO, 5.0f);

				// To keep an eye on. Possible crash since manipulating the inventory of an uninitialized NPC can be problematic.
				SKSE::GetTaskInterface()->AddTask([cloneHandle = clone->GetHandle()]() {
					auto* clone = MiscUtils::GetValidReference<RE::Actor>(MiscUtils::ResolveHandleAs<RE::Actor>(cloneHandle), true);
					if (!clone) return;
					
					MainFunctions::ClearInventory(clone);
					ModUtils::EquipActorObject(clone, Invisible_Actor_Armor);
				});

				clone->SetActivationBlocked(true);
				clone->SetLifeState(RE::ACTOR_LIFE_STATE::kDead);
				clone->SetDisplayName(origin->GetName() ? std::string(origin->GetName()) + " " : "", true);
				
				if (auto* cloneAv = clone->AsActorValueOwner()) cloneAv->SetActorValue(RE::ActorValue::kVariable01, ngdValueMarker);

				// Disable origin actor head collisions
				ModUtils::WaitUntilRagdollReady(origin, [](RE::TESObjectREFR* objectRef, const bool result) {
					if (!result || !objectRef) return;

					auto* origin = MiscUtils::GetValidReference<RE::Actor>(objectRef, true);
					if (!origin) return;
					
					if (auto* headCollision = origin->GetNodeByName("NPC Head [Head]")) {
						MainFunctions::ToggleCollisionForNode(headCollision, false);
					}
					if (auto* neckCollision = origin->GetNodeByName("NPC Neck [Neck]")) {
						MainFunctions::ToggleCollisionForNode(neckCollision, false);
					}
				});

				// Set 3D Transform for clone actor
				ModUtils::WaitUntilRagdollReady(clone, [originHandle = origin->GetHandle(), cloneHandle = clone->GetHandle(), equippedHelmetFormID, params, showHelmet]
					(RE::TESObjectREFR* objectRef, const bool result) {
					if (!result || !objectRef) return;

					ModUtils::SetScalesAndCollisions(objectRef);

					ModUtils::WaitAndCall(FRAME_DELAY_MS(), [=]() {
						auto* origin = MiscUtils::GetValidReference<RE::Actor>(MiscUtils::ResolveHandleAs<RE::Actor>(originHandle), true);
						auto* clone = MiscUtils::GetValidReference<RE::Actor>(MiscUtils::ResolveHandleAs<RE::Actor>(cloneHandle), true);
						if (!origin || !clone) return;

						MainFunctions::Set3DTransform(clone, origin);

						OnDecapitate_Step02(origin, clone, equippedHelmetFormID, showHelmet, params);
					});
				});

				TRACE("Decapitated actor : <\"{}\" [REF:{:08X}] [BASE:{:08X}]>",
					origin->GetName(), origin->formID, (origin->GetActorBase() ? origin->GetActorBase()->formID : 0x0));

				return clone;
			}

			static void OnDecapitate_Step02(RE::Actor* origin, RE::Actor* clone, RE::FormID equippedHelmetFormID, const bool showHelmet, const DecapitateParams& params)
			{
				if (!origin || !clone || !origin->Is3DLoaded() || !clone->Is3DLoaded())	return;

				RE::TESObjectARMO* originHelmet = equippedHelmetFormID ? RE::TESForm::LookupByID<RE::TESObjectARMO>(equippedHelmetFormID) : nullptr;
				std::vector<RE::TESObjectARMO*> unequippedHeadGear = ModUtils::UnequipHeadGear(origin, originHelmet);
				if (!showHelmet) unequippedHeadGear.clear();

				ModUtils::EquipActorObject(origin, ModUtils::GetArmorPart(origin, ModUtils::DecapArmorPart::Neck));
				ModUtils::EquipActorObject(origin, ModUtils::GetArmorPart(origin, ModUtils::DecapArmorPart::HeadHidden));
				
				ModUtils::RemoveActorObject(clone, Invisible_Actor_Armor);
				ModUtils::EquipActorObject(clone, ModUtils::GetArmorPart(clone, ModUtils::DecapArmorPart::Head));
				if (SettingsIni::bReplicateHeadEquipment) MainFunctions::ReplicateHeadGear(clone, unequippedHeadGear);

				NativeUtils::PlayArtObject(clone, HeadBloodFX_AO, 5.0f);
				
				origin->StopCurrentDialogue();
				clone->StopCurrentDialogue();

				RE::NiAVObject* cloneCOMNode = clone->GetNodeByName("NPC COM [COM ]");
				if (!cloneCOMNode) return;

				if (params.customHitData) {
					NiUtils::ApplyImpulseToRigidBody(
						cloneCOMNode,
						params.hitFromPosition,
						params.hitToPosition,
						params.hitPower,
						SettingsIni::fHeadSpinFactor
					);
				} else {
					RE::NiPoint3 impulseFromPos = origin->GetNodeByName("NPC Spine2 [Spn2]") ?
						origin->GetNodeByName("NPC Spine2 [Spn2]")->world.translate :
						RE::NiPoint3( 0.0f, 0.0f, 0.0f );
								
					RE::NiPoint3 impulseToPos = origin->GetNodeByName("NPC Head [Head]") ?
						origin->GetNodeByName("NPC Head [Head]")->world.translate :
						RE::NiPoint3( 0.0f, 0.0f, 1.0f );
					
					NiUtils::ApplyImpulseToRigidBody(
						cloneCOMNode,
						impulseFromPos, impulseToPos,
						(params.motionless ? 2.0f : SettingsIni::fHeadImpulseFactor),
						(params.motionless ? 0.2f : SettingsIni::fHeadSpinFactor)
					);
				}

				if (deathDropOverhaulEnabled && originHelmet) {
					MainFunctions::DropActorHelmet(origin, clone, originHelmet);
				}

				ModUtils::WaitAndCall(50ms, [originHandle = origin->GetHandle(), cloneHandle = clone->GetHandle()]() {
					if (auto* origin = MiscUtils::ResolveHandleAs<RE::Actor>(originHandle)) {
						origin->AddChange(RE::Actor::ChangeFlags::kDismemberedLimbs);
					}
					if (auto* clone = MiscUtils::ResolveHandleAs<RE::Actor>(cloneHandle)) {
						clone->AddChange(RE::Actor::ChangeFlags::kDismemberedLimbs);
						clone->AddChange(RE::TESObjectREFR::ChangeFlags::kPromoted);
					}
				});

				Serialization::AddDecapitation(origin->formID, clone->formID);

				TaskManager::ProcessStumpTask(origin);
				TaskManager::ProcessStumpTask(clone);

				if (params.callback) params.callback(clone);
				else if (dismemberingFrameworkEnabled) {
					DismemberingFrameworkAPI::g_API->PostDecapitate(origin, clone);
				}
			}
	};
};
