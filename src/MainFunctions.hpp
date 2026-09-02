#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"
#include "TaskManager.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"
#include "Utils/NativeUtils.hpp"
#include "Utils/NiUtils.hpp"

#include "API/DDO-API.h"

namespace Events
{
	using namespace ModData;

	class MainFunctions
	{
		public:

			static bool ShowHelmet(RE::Actor* actor)
			{
				if (!actor) return true;

				if (HT_NPCManagedEffect_Effect && HT_HeadgearEquippedEffect_Effect) {
					if (RE::MagicTarget* actorMagicTarget = actor->AsMagicTarget()) {
						if (actorMagicTarget->HasMagicEffect(HT_NPCManagedEffect_Effect) && !actorMagicTarget->HasMagicEffect(HT_HeadgearEquippedEffect_Effect)) {
							return false;
						}
					}
				}

				return true;
			}

			static bool IsHelmetExcluded(RE::TESObjectARMO* armor, RE::Actor* actor)
			{
				RE::TESRace* race = actor->GetRace();
				if (!race) return false;

				RE::TESObjectARMA* armorAddon = armor->GetArmorAddonByMask(race, RE::BIPED_MODEL::BipedObjectSlot::kHair);
				if (!armorAddon) return false;

				RE::TESNPC* actorBase = actor->GetActorBase();
				if (!actorBase) return false;

				auto checkModel = [&](RE::SEX sex) -> bool {
					RE::TESModelTextureSwap* modelSwap = &(armorAddon->bipedModels[sex]);
					if (!modelSwap) return true;
					const char* modelPath = modelSwap->GetModel();
					if (!modelPath || *modelPath == '\0') return true;

					std::string formatedModelPath = modelPath;
					std::transform(formatedModelPath.begin(), formatedModelPath.end(), formatedModelPath.begin(), ::tolower);

					if (formatedModelPath.empty()) return true;
					for (const auto& excludedModel : SettingsIni::sExcludedArmorModels) {
						if (formatedModelPath.find(excludedModel) != std::string::npos) return true;
					}
					return false;
				};

				if (actorBase->IsFemale() && &(armorAddon->bipedModels[RE::SEX::kFemale])) {
					if (checkModel(RE::SEX::kFemale)) return true;
				} else if (checkModel(RE::SEX::kMale)) return true;

				return false;
			}

			static RE::TESObjectARMO* GetActorHelmet(RE::Actor* actor)
			{
				RE::TESObjectARMO* equippedArmor = actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kHair);
				if (!equippedArmor || !equippedArmor->GetPlayable() || equippedArmor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kBody)) return nullptr;

				return equippedArmor;
			}

			static void DropActorHelmet(RE::Actor* origin, RE::Actor* clone, RE::TESObjectARMO* armor)
			{
				if ((armor->GetArmorType() == RE::BIPED_MODEL::ArmorType::kClothing && (SettingsIni::fClothingEquipmentEjectionChance / 100.0f) < MiscUtils::GetRandomNumber()) ||
					(armor->GetArmorType() == RE::BIPED_MODEL::ArmorType::kLightArmor && (SettingsIni::fLightEquipmentEjectionChance / 100.0f) < MiscUtils::GetRandomNumber()) ||
					(armor->GetArmorType() == RE::BIPED_MODEL::ArmorType::kHeavyArmor && (SettingsIni::fHeavyEquipmentEjectionChance / 100.0f) < MiscUtils::GetRandomNumber())) {
					return;
				}

				if (!SettingsIni::sExcludedArmorModels.empty() && IsHelmetExcluded(armor, origin)) return;

				const float delay = MiscUtils::GetRandomNumber(SettingsIni::fEquipmentEjectionDelayMin, SettingsIni::fEquipmentEjectionDelayMax);
				ModUtils::WaitAndCall(std::chrono::milliseconds(static_cast<int>(delay * 1000.0f)),
					[originFormID = origin->formID, cloneFormID = clone->formID, equippedHelmetFormID = armor->formID]() {
					auto* origin = MiscUtils::GetValidReference<RE::Actor>(originFormID, true);
					auto* clone = MiscUtils::GetValidReference<RE::Actor>(cloneFormID, true);
					RE::TESObjectARMO* equippedHelmet = RE::TESForm::LookupByID<RE::TESObjectARMO>(equippedHelmetFormID);
					if (!origin || !clone || !equippedHelmet || !clone->Is3DLoaded()) return;

					if (auto cloneHeadNode = clone->GetNodeByName("NPC COM [COM ]")) {
						DeathDropOverhaulAPI::g_API->DropItemFromActor(origin, equippedHelmet, cloneHeadNode,
							SettingsIni::fHeadEquipmentImpulseMult, SettingsIni::fHeadEquipmentSpinMult);
						if (SettingsIni::bReplicateHeadEquipment) TaskManager::RemoveHeadEquipment(origin, equippedHelmet);
					}
				});
			}

			static void ReplicateHeadGear(RE::Actor* clone, const std::vector<RE::TESObjectARMO*>& armors)
			{
				if (!clone) return;

				for (auto* armor : armors) {
					if (!armor) continue;
					ModUtils::EquipActorObject(clone, armor);
				}
			}

			static void Set3DTransform(RE::TESObjectREFR* targetRef, RE::TESObjectREFR* sourceRef, RE::BSFixedString startingNode = "")
			{
				if (!targetRef) return;

				RE::NiAVObject* targetRoot = ((!startingNode.empty() && targetRef->GetNodeByName(startingNode)) ? targetRef->GetNodeByName(startingNode) : targetRef->Get3D());
				if (!targetRoot) return;

				auto geHkTransform = [](RE::NiAVObject* targetNode) -> RE::hkTransform {
					if (!targetNode) return RE::hkTransform();

					RE::hkTransform targetTrans{};
					if (auto* rigidBody = NiUtils::GetRigidBody(targetNode); rigidBody && rigidBody->world) {
						auto* motionState = rigidBody->GetMotionState();
						if (motionState) targetTrans = motionState->transform;
					}
					return targetTrans;
				};

				std::function<void(RE::NiAVObject*)> findNextRigidBody = [&](RE::NiAVObject* targetNode) {
					if (!targetNode || !targetNode->AsNode()) return;
					auto niNode = targetNode->AsNode();

					if (auto* rigidBody = NiUtils::GetRigidBody(targetNode); rigidBody && rigidBody->world) {
						if (auto* motionState = rigidBody->GetMotionState()) {
							auto hkTransform = geHkTransform(sourceRef->GetNodeByName(targetNode->name));
							hkTransform.translation = motionState->transform.translation;
							rigidBody->motion.SetTransform(hkTransform);
						}
					}
	
					if (targetNode->name != "NPC Head [Head]") {
						for (auto& child : niNode->GetChildren()) {
							if (child && child.get()) {
								findNextRigidBody(child.get());
							}
						}
					}
				};

				findNextRigidBody(targetRoot);
			}

			static void ToggleCollisionForNode(RE::NiAVObject* node, bool enable)
			{
				if (!node) return;

				if (auto* rigidBody = NiUtils::GetRigidBody(node); rigidBody && rigidBody->world) {
					auto& collisionFilterInfo = rigidBody->collidable.broadPhaseHandle.collisionFilterInfo;

					if (enable) collisionFilterInfo.SetCollisionLayer(RE::COL_LAYER::kDeadBip);
					else collisionFilterInfo.SetCollisionLayer(ModData::NonCollidableLayer);
				}
			}

			static bool TestRace(RE::Actor* actor)
			{
				if (!actor) return false;

				RE::TESRace* race = actor->GetRace();
				if (!race) return false;
				
				if (SettingsIni::sExcludedRaces.find(race->formEditorID.c_str()) != SettingsIni::sExcludedRaces.end()) {
					return false;
				}

				auto it = std::find(patchedRaces.begin(), patchedRaces.end(), race);

				return it != patchedRaces.end();
			}

			static bool TestClothing(RE::Actor* actor)
			{
				if (!actor) return false;

				RE::TESObjectARMO* armor = actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kBody);
				if (armor) {
					if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHead) ||
						armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHair) ||
						armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kCirclet) ||
						armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kEars)) {
						return false;
					}
				}

				return true;
			}

			static void FixAlphaState(RE::Actor* target)
			{
				auto &runtime = target->GetActorRuntimeData();

				if (!runtime.currentProcess || !runtime.currentProcess->high) return;

				runtime.currentProcess->high->fadeState = RE::HighProcessData::FADE_STATE::kNormal;
				runtime.currentProcess->high->fadeAlpha = 1.0f;
			}

			static void ClearInventory(RE::Actor* actor, const std::unordered_set<RE::TESBoundObject*>& excludeItems = {})
			{
				if (!actor) return;

				for (const auto& [form, data] : actor->GetInventory(RE::TESObjectREFR::DEFAULT_INVENTORY_FILTER, true)) {
					if (!form || !(data.second > 0)) continue;
					if (excludeItems.find(static_cast<RE::TESBoundObject*>(form)) == excludeItems.end()) {
						actor->RemoveItem(form, data.first, RE::ITEM_REMOVE_REASON::kRemove, &(actor->extraList), nullptr);
					}
				}
			}

			static void SetHeadPosition(RE::Actor* origin, RE::Actor* clone, bool init)
			{
				RE::NiAVObject* headNode = origin->GetNodeByName("NPC Head [Head]");
				RE::NiPoint3 HeadPos = (headNode ? headNode->world.translate : origin->GetPosition());

				// Subtract the height of the "NPC COM [COM ]" node (adjusted for scale) from the head position of the original NPC 
				// to ensure the clone is perfectly centered relative to the original NPC's reference position.
				HeadPos.z -= (origin->GetActorBase()->IsFemale() ? SettingsIni::fCOMNodeZPositionFemale : SettingsIni::fCOMNodeZPositionMale) * origin->GetScale();

				NativeUtils::MoveToImpl(clone, clone->GetHandle(), clone->GetParentCell(), clone->GetWorldspace(), HeadPos, origin->data.angle);
			}
	};
};
