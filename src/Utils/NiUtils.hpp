#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

class NiUtils
{
public:


	static void ApplyImpulseToRigidBody(RE::NiAVObject* niAVObject, RE::NiPoint3 fromPosition, RE::NiPoint3 toPosition, float linearImpulseMagnitude, float angularImpulseMagnitude)
	{
		if (!niAVObject) return;

		auto* hkpRigidBody = GetRigidBody(niAVObject);
		if (!hkpRigidBody) return;

		RE::NiPoint3 direction = (toPosition - fromPosition);
		float norm = direction.Length();

		if (norm == 0.0f) return;
		direction /= norm;

		float bodyMass = niAVObject->GetMass();

		float linearImpulseFactor = MiscUtils::GetRandomNumber(0.65f, 1.35f);
		RE::hkVector4 linearImpulse = {
			direction.x * linearImpulseMagnitude * linearImpulseFactor,
			direction.y * linearImpulseMagnitude * linearImpulseFactor,
			direction.z * linearImpulseMagnitude * linearImpulseFactor,
			0.0f
		};
		hkpRigidBody->motion.ApplyLinearImpulse(linearImpulse * bodyMass);
		
		float angularImpulseFactor = MiscUtils::GetRandomNumber(0.65f, 1.35f);
		RE::hkVector4 angularImpulse = {
			(MiscUtils::GetRandomNumber(-0.022f, 0.022f) * angularImpulseFactor) * bodyMass,
			(MiscUtils::GetRandomNumber(-0.022f, 0.022f) * angularImpulseFactor) * bodyMass,
			(angularImpulseMagnitude * angularImpulseFactor) * bodyMass,
			0.0f
		};
		hkpRigidBody->motion.ApplyAngularImpulse(angularImpulse);
	}

	static bool IsReferenceRagdollReady(RE::TESObjectREFR* ref)
	{
		if (!ref || !ref->Is3DLoaded()) return false;

		RE::NiAVObject* niAVObject = ref->Get3D();
		if (!niAVObject) return false;
		
		auto* hkpRigidBody = GetRigidBody(niAVObject);
		if (hkpRigidBody && hkpRigidBody->world && hkpRigidBody->motion.GetMass() > 0.0f) return true;

		return false;
	}

	static RE::hkpRigidBody* GetRigidBody(RE::NiAVObject* a_object)
	{
		if (!a_object) return nullptr;

		const auto collisionObject = a_object->GetCollisionObject();
		if (!collisionObject) return nullptr;

		const auto bhkRigidBody = RE::NiPointer<RE::bhkRigidBody>(collisionObject->GetRigidBody());
		if (!bhkRigidBody || !bhkRigidBody->referencedObject) return nullptr;

		const auto hkpRigidBody = static_cast<RE::hkpRigidBody*>(bhkRigidBody->referencedObject.get());
		return hkpRigidBody;
	}

	static void UpdateNode(RE::NiAVObject* node, SKSE::stl::enumeration<RE::NiUpdateData::Flag, std::uint32_t> flags = RE::NiUpdateData::Flag::kNone, float updateTime = 0.f)
	{
		if (!node) return;

		auto updateData = RE::NiUpdateData{};
		updateData.flags = flags;
		updateData.time = updateTime;
		node->Update(updateData);
	}
};
