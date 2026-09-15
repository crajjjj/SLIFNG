#include "Report.h"

#include "API/SKEE.h"
#include "BodyProfile.h"
#include "Ledger.h"
#include "Skee.h"
#include "Vocabulary.h"

#include <set>

namespace SLIFNG::Report
{
	namespace
	{
		void Row(std::vector<RE::BSFixedString>& a_out, std::string_view a_label, std::string_view a_value)
		{
			a_out.emplace_back(a_label.data());
			a_out.emplace_back(a_value.data());
		}

		void Header(std::vector<RE::BSFixedString>& a_out, std::string_view a_label)
		{
			Row(a_out, a_label, "");
		}

		std::string Num(float a_value)
		{
			return std::format("{:.3f}", a_value);
		}

		// Which skeleton nodes this actor actually has. Real evidence, unlike a
		// guess from the plugin list: it reads the loaded 3D.
		std::string SkeletonNodes(RE::Actor* a_actor, int& a_found, int& a_total)
		{
			a_found = 0;
			a_total = 0;
			std::string present;
			auto* root = a_actor->Get3D(false);
			for (const auto& target : Vocabulary::kTargets) {
				for (const auto* node : target.nodes) {
					if (!node) {
						continue;
					}
					++a_total;
					if (root && root->GetObjectByName(RE::BSFixedString(node))) {
						++a_found;
						if (!present.empty()) {
							present += ", ";
						}
						present += node;
					}
				}
			}
			if (!root) {
				return "(no 3D loaded - cannot probe)";
			}
			return a_found == 0 ? "none found" : present;
		}

		// Body identification is HEURISTIC until per-actor profiles land (PLAN
		// P2). Report the evidence rather than asserting a body: the race is
		// decisive for UBE (it is race-based), and skee's knowledge of a slider
		// name tells you whether the morph we drive exists at all.
		std::string BodyGuess(RE::Actor* a_actor)
		{
			const auto* race = a_actor->GetRace();
			const std::string raceID = race && race->GetFormEditorID() ? race->GetFormEditorID() : "";
			if (!raceID.empty() && Lower(raceID).find("ube") != std::string::npos) {
				return "UBE (race-based) - heuristic";
			}
			auto* dh = RE::TESDataHandler::GetSingleton();
			auto has = [dh](const char* a_plugin) { return dh && dh->LookupModByName(a_plugin) != nullptr; };
			if (has("UBE_AllRace.esp") || has("UBE.esp")) {
				return "UBE installed, non-UBE race - heuristic";
			}
			if (has("3BBB.esp") || has("CBBE 3BA.esp")) {
				return "CBBE 3BA - heuristic";
			}
			if (has("BHUNP.esp") || has("BHUNP3BBB.esp")) {
				return "BHUNP - heuristic";
			}
			return "unknown (no marker plugin) - heuristic";
		}
	}

	std::vector<RE::BSFixedString> ForActor(RE::Actor* a_actor)
	{
		std::vector<RE::BSFixedString> out;
		if (!a_actor) {
			Row(out, "No actor selected", "");
			return out;
		}
		auto& ledger = Ledger::GetSingleton();
		const auto formID = a_actor->GetFormID();

		// ---- identity -------------------------------------------------------
		Header(out, "Actor");
		Row(out, "Name", a_actor->GetName() ? a_actor->GetName() : "(unnamed)");
		Row(out, "FormID", std::format("{:08X}", formID));
		const auto* race = a_actor->GetRace();
		Row(out, "Race", race && race->GetFormEditorID() ? race->GetFormEditorID() : "(unknown)");
		const auto* base = a_actor->GetActorBase();
		Row(out, "Sex", base && base->GetSex() == RE::SEX::kFemale ? "Female" : "Male");
		Row(out, "3D loaded", a_actor->Is3DLoaded() ? "yes" : "NO - changes deferred to load");

		// ---- body -----------------------------------------------------------
		Header(out, "Body");
		Row(out, "Profile", BodyProfile::ResolvedName(a_actor));
		Row(out, "Heuristic", BodyGuess(a_actor));
		int found = 0;
		int total = 0;
		const std::string nodes = SkeletonNodes(a_actor, found, total);
		Row(out, std::format("Skeleton nodes ({}/{})", found, total), nodes);

		// ---- what mods sent -------------------------------------------------
		Header(out, "Contributions (what each mod sent)");
		const auto targets = ledger.TargetsOf(formID);
		bool anyContribution = false;
		// Walk mods per target so the page groups by what is being driven.
		for (const auto& target : targets) {
			const std::string shown = IsMorphTarget(target)
			                              ? ledger.SliderName(SliderOf(target))
			                              : target;
			for (const auto& mod : ledger.ModsDriving(formID, target)) {
				const float raw = ledger.GetContribution(formID, mod, target);
				Row(out, std::format("{} -> {}", mod, shown), Num(raw));
				anyContribution = true;
			}
		}
		if (!anyContribution) {
			Row(out, "(nothing registered for this actor)", "");
		}

		// ---- applied vs default --------------------------------------------
		Header(out, "Applied vs default");
		const float master = ledger.MasterScale();
		if (std::abs(master - 1.0f) > 0.0001f) {
			Row(out, "Overall magnitude", std::format("{}x", Num(master)));
		}

		// Collect every skee slider these targets drive, plus any node-fallback
		// targets, so the summary matches what actually reached the body.
		std::set<std::string> sliders;
		std::set<std::string> nodeTargets;
		for (const auto& target : targets) {
			if (IsMorphTarget(target)) {
				sliders.insert(SliderOf(target));
			} else if (Vocabulary::Find(target)) {
				// Per-actor: the profile decides morph vs node for this key.
				const auto* blends = BodyProfile::BlendFor(a_actor, target);
				if (blends && !blends->empty()) {
					for (const auto& blend : *blends) {
						sliders.insert(Lower(blend.slider));
					}
				} else {
					nodeTargets.insert(target);
				}
			}
		}

		if (sliders.empty() && nodeTargets.empty()) {
			Row(out, "(nothing applied)", "");
		}
		for (const auto& sliderLower : sliders) {
			const std::string name = ledger.SliderName(sliderLower);
			const float folded = ledger.AggregateSlider(formID, sliderLower);
			const float scaled = folded * ledger.EffectiveScale(sliderLower);
			// Morph default is 0.0. The skee readback proves OUR WRITE LANDED
			// (right key, right name, not clobbered) - it is NOT an availability
			// test: SetMorph/GetMorph are a dictionary keyed by
			// (actor, slider, key) and never consult the mesh. A slider absent
			// from this body's morphs.tri stores and reads back exactly the same;
			// it is only ignored later, inside ApplyBodyMorphs. Knowing whether a
			// body HAS a slider needs the per-body profile (PLAN P2) - skee
			// exposes no such query.
			std::string value = std::format("{} (default 0.000", Num(scaled));
			if (Skee::IsReady()) {
				value += std::format(", skee {}", Num(Skee::ReadMorph(a_actor, name)));
			}
			value += ")";
			Row(out, std::format("morph {}", name), value);
		}
		for (const auto& target : nodeTargets) {
			const float folded = ledger.Aggregate(formID, target);
			const float scaled = 1.0f + (folded - 1.0f) * ledger.EffectiveScale(target);
			Row(out, std::format("node {}", target), std::format("{} (default 1.000)", Num(scaled)));
		}

		Header(out, "Aggregation");
		Row(out, "Mode", ledger.GetMode() == AggregationMode::kAdditive ? "Additive" : "Highest wins");
		return out;
	}

	void LogForActor(RE::Actor* a_actor)
	{
		const auto rows = ForActor(a_actor);
		logger::info("[Report] ===== actor diagnostics =====");
		for (std::size_t i = 0; i + 1 < rows.size(); i += 2) {
			const std::string label = rows[i].c_str();
			const std::string value = rows[i + 1].c_str();
			if (value.empty()) {
				logger::info("[Report] -- {} --", label);
			} else {
				logger::info("[Report]    {}: {}", label, value);
			}
		}
		logger::info("[Report] ===== end =====");
	}
}
