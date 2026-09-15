#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace logger = SKSE::log;

namespace SLIFNG
{
	// Papyrus strings compare case-insensitively; every string used as a map key
	// goes through this first so the native side matches that behavior.
	inline std::string Lower(std::string_view a_text)
	{
		std::string out{ a_text };
		std::transform(out.begin(), out.end(), out.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return out;
	}
}
