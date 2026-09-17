#pragma once

// Serves API/SLIFNG_API.h to other SKSE plugins over the messaging interface.

namespace SLIFNG::APIServer
{
	// Register the plugin-to-plugin message listener. Call once from
	// SKSEPluginLoad, after SKSE::Init.
	void Install();
}
