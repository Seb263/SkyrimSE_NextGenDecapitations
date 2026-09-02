#include "API.h"
#include "Main.hpp"

#include "Utils/ModUtils.hpp"

size_t NGDecapitationsAPI::NGDecapitationsAPI::GetVersion() const
{
	return NGD_API_VERSION;
}

bool NGDecapitationsAPI::NGDecapitationsAPI::Decapitate(RE::Actor* target, DecapitateParams* params) const
{
	if (!target) return false;

	DecapitateParams f_params;
	if (params) f_params = *params;

	return Events::MainEvent::ProceedDecapitation(target, f_params);
}

bool NGDecapitationsAPI::NGDecapitationsAPI::IsDecapitated(RE::Actor* actor) const
{
	return ModUtils::IsDecapitated(actor);
}

bool NGDecapitationsAPI::NGDecapitationsAPI::IsHead(RE::Actor* actor) const
{
	return ModUtils::IsHead(actor);
}
