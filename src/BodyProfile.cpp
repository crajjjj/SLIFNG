#include "BodyProfile.h"

#include <filesystem>
#include <fstream>

namespace SLIFNG::BodyProfile
{
	namespace
	{
		constexpr const char* kFolder = "Data/SLIFNG/Bodies";

		std::vector<Profile> g_profiles;
		const Profile* g_default = nullptr;
		std::unordered_map<RE::FormID, const Profile*> g_cache;
		std::mutex g_lock;

		std::string Trim(std::string_view a_text)
		{
			const auto first = a_text.find_first_not_of(" \t\r\n");
			if (first == std::string_view::npos) {
				return {};
			}
			const auto last = a_text.find_last_not_of(" \t\r\n");
			return std::string{ a_text.substr(first, last - first + 1) };
		}

		// The fallback when no .ini ships or none matches: NODE scaling for every
		// key, no morphs - which is exactly what reference SLIF does out of the
		// box (its shipped bodymorphs config has every percent at 0.0). Morphs
		// are something a body profile grants, and the installer is where a body
		// gets chosen; with no profile there is no slider knowledge to invent.
		Profile BuiltInDefault()
		{
			Profile p;
			p.name = "node scaling (SLIF default)";
			p.file = "(compiled in)";
			p.isDefault = true;
			return p;
		}

		// [slif_belly]
		//   FullScale = 7.5        <- node deviation at which the sliders hit Max
		//   Morph1    = PregnancyBelly
		//   Morph1Max = 1.0        <- slider value at FullScale (BF NG's semantics)
		void ParseTargetSection(Target& a_target, float a_fullScale,
			const std::vector<std::pair<std::string, std::string>>& a_pairs)
		{
			for (int i = 1; i <= 16; ++i) {
				const std::string nameKey = std::format("morph{}", i);
				const std::string maxKey = nameKey + "max";
				std::string slider;
				float maxValue = 1.0f;
				bool found = false;
				for (const auto& [k, v] : a_pairs) {
					if (k == nameKey) {
						slider = v;
						found = true;
					} else if (k == maxKey) {
						try {
							maxValue = std::stof(v);
						} catch (...) {
							maxValue = 1.0f;
						}
					}
				}
				if (!found || slider.empty()) {
					continue;
				}
				// Convert "value at full scale" into "value per 1.0 of deviation",
				// which is the unit the apply path works in.
				const float denom = a_fullScale > 0.0001f ? a_fullScale : 1.0f;
				a_target.morphs.push_back({ slider, maxValue / denom });
			}
		}

		bool LoadFile(const std::filesystem::path& a_path, Profile& a_out)
		{
			std::ifstream in(a_path);
			if (!in) {
				return false;
			}
			a_out.file = a_path.filename().string();
			a_out.name = a_out.file;
			a_out.isDefault = Lower(a_out.file) == "default.ini";

			std::string section;
			std::vector<std::pair<std::string, std::string>> pairs;
			float fullScale = 1.0f;

			auto flush = [&]() {
				if (section.empty()) {
					return;
				}
				if (section == "profile") {
					for (const auto& [k, v] : pairs) {
						if (k == "name") {
							a_out.name = v;
						} else if (k == "race") {
							a_out.race = Lower(v);
						} else if (k == "plugin") {
							a_out.plugins.push_back(v);
						}
					}
				} else {
					Target t;
					ParseTargetSection(t, fullScale, pairs);
					a_out.targets.emplace(section, std::move(t));
				}
				pairs.clear();
				fullScale = 1.0f;
			};

			std::string line;
			while (std::getline(in, line)) {
				const std::string trimmed = Trim(line);
				if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') {
					continue;
				}
				if (trimmed.front() == '[' && trimmed.back() == ']') {
					flush();
					section = Lower(Trim(trimmed.substr(1, trimmed.size() - 2)));
					continue;
				}
				const auto eq = trimmed.find('=');
				if (eq == std::string::npos) {
					continue;
				}
				const std::string key = Lower(Trim(trimmed.substr(0, eq)));
				const std::string value = Trim(trimmed.substr(eq + 1));
				if (key == "fullscale") {
					try {
						fullScale = std::stof(value);
					} catch (...) {
						fullScale = 1.0f;
					}
				}
				pairs.emplace_back(key, value);
			}
			flush();
			return true;
		}

		bool Matches(const Profile& a_profile, RE::Actor* a_actor)
		{
			if (a_profile.isDefault) {
				return false;  // the fallback is chosen only when nothing matched
			}
			if (!a_profile.race.empty()) {
				const auto* race = a_actor->GetRace();
				const char* id = race ? race->GetFormEditorID() : nullptr;
				if (id && Lower(id).find(a_profile.race) != std::string::npos) {
					return true;
				}
			}
			if (!a_profile.plugins.empty()) {
				auto* dh = RE::TESDataHandler::GetSingleton();
				for (const auto& plugin : a_profile.plugins) {
					if (dh && dh->LookupModByName(plugin.c_str())) {
						return true;
					}
				}
			}
			return false;
		}
	}

	void Load()
	{
		std::scoped_lock lock(g_lock);
		g_profiles.clear();
		g_cache.clear();
		g_default = nullptr;

		std::error_code ec;
		if (std::filesystem::is_directory(kFolder, ec)) {
			std::vector<std::filesystem::path> files;
			for (const auto& entry : std::filesystem::directory_iterator(kFolder, ec)) {
				if (entry.is_regular_file(ec) && Lower(entry.path().extension().string()) == ".ini") {
					files.push_back(entry.path());
				}
			}
			std::sort(files.begin(), files.end());
			for (const auto& file : files) {
				Profile p;
				if (LoadFile(file, p)) {
					logger::info("[BodyProfile] loaded '{}' from {} ({} target(s), race '{}', {} plugin matcher(s))",
						p.name, p.file, p.targets.size(), p.race, p.plugins.size());
					g_profiles.push_back(std::move(p));
				}
			}
		} else {
			logger::warn("[BodyProfile] no {} folder - using the built-in default only", kFolder);
		}

		// Always keep a usable fallback, even with no files on disk.
		const auto hasDefault = std::any_of(g_profiles.begin(), g_profiles.end(),
			[](const Profile& p) { return p.isDefault; });
		if (!hasDefault) {
			g_profiles.push_back(BuiltInDefault());
		}
		for (const auto& p : g_profiles) {
			if (p.isDefault) {
				g_default = &p;
			}
		}
		logger::info("[BodyProfile] {} profile(s) available; fallback '{}'", g_profiles.size(),
			g_default ? g_default->name : "(none!)");
	}

	std::size_t Count()
	{
		std::scoped_lock lock(g_lock);
		return g_profiles.size();
	}

	const Profile* ForActor(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return nullptr;
		}
		std::scoped_lock lock(g_lock);
		if (g_profiles.empty()) {
			return nullptr;
		}
		const auto formID = a_actor->GetFormID();
		if (const auto it = g_cache.find(formID); it != g_cache.end()) {
			return it->second;
		}
		const Profile* chosen = g_default;
		for (const auto& p : g_profiles) {
			if (Matches(p, a_actor)) {
				chosen = &p;
				break;
			}
		}
		g_cache.emplace(formID, chosen);
		logger::info("[BodyProfile] {:08X} '{}' -> '{}'", formID,
			a_actor->GetName() ? a_actor->GetName() : "?", chosen ? chosen->name : "(none)");
		return chosen;
	}

	std::string ResolvedName(RE::Actor* a_actor)
	{
		const auto* p = ForActor(a_actor);
		return p ? p->name : "(none)";
	}

	const std::vector<Blend>* BlendFor(RE::Actor* a_actor, const std::string& a_key)
	{
		const auto* profile = ForActor(a_actor);
		if (!profile) {
			return nullptr;
		}
		std::scoped_lock lock(g_lock);
		const auto it = profile->targets.find(a_key);
		if (it == profile->targets.end()) {
			// The profile does not describe this key at all: treat it as "this
			// body cannot morph it" and let the caller use the node path.
			return nullptr;
		}
		return &it->second.morphs;
	}

	const std::vector<Blend>* BlendForID(RE::FormID a_actor, const std::string& a_key)
	{
		{
			std::scoped_lock lock(g_lock);
			if (const auto it = g_cache.find(a_actor); it != g_cache.end()) {
				const auto* profile = it->second;
				if (!profile) {
					return nullptr;
				}
				const auto tit = profile->targets.find(a_key);
				return tit == profile->targets.end() ? nullptr : &tit->second.morphs;
			}
		}
		// Not resolved yet: do it properly if the actor is reachable, else fall
		// back to the default so an unloaded actor still folds sensibly.
		if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_actor)) {
			return BlendFor(actor, a_key);
		}
		std::scoped_lock lock(g_lock);
		if (!g_default) {
			return nullptr;
		}
		const auto it = g_default->targets.find(a_key);
		return it == g_default->targets.end() ? nullptr : &it->second.morphs;
	}

	void ClearCache()
	{
		std::scoped_lock lock(g_lock);
		g_cache.clear();
	}
}
