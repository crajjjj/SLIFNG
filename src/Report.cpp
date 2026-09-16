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
		std::vector<std::string> SkeletonNodes(RE::Actor* a_actor, int& a_found, int& a_total)
		{
			// Returns only the MISSING nodes: the full list overflows the MCM's
			// value column, and "which are absent" is the useful answer anyway.
			a_found = 0;
			a_total = 0;
			std::vector<std::string> missing;
			auto* root = a_actor->Get3D(false);
			for (const auto& target : Vocabulary::kTargets) {
				for (const auto* node : target.nodes) {
					if (!node) {
						continue;
					}
					++a_total;
					if (root && root->GetObjectByName(RE::BSFixedString(node))) {
						++a_found;
					} else if (root) {
						missing.emplace_back(node);
					}
				}
			}
			return missing;
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
				return "UBE (race)";
			}
			auto* dh = RE::TESDataHandler::GetSingleton();
			auto has = [dh](const char* a_plugin) { return dh && dh->LookupModByName(a_plugin) != nullptr; };
			if (has("UBE_AllRace.esp") || has("UBE.esp")) {
				return "UBE installed";
			}
			if (has("3BBB.esp") || has("CBBE 3BA.esp")) {
				return "CBBE 3BA";
			}
			if (has("BHUNP.esp") || has("BHUNP3BBB.esp")) {
				return "BHUNP";
			}
			return "unknown";
		}
	}

	std::vector<RE::BSFixedString> ForActor(RE::Actor* a_actor)
	{
		// LAYOUT RULES - this renders in a TWO-COLUMN SkyUI MCM:
		//   * a label past ~30 chars collides with its own value column;
		//   * a value past ~20 chars runs left across the label;
		//   * an EMPTY value means "section header", so a placeholder line must
		//     still carry a value or it draws as a header complete with divider.
		// The log reuses these rows, so short helps there too.
		std::vector<RE::BSFixedString> out;
		if (!a_actor) {
			Row(out, "Actor", "none selected");
			return out;
		}
		auto& ledger = Ledger::GetSingleton();
		const auto formID = a_actor->GetFormID();

		Header(out, "Actor");
		Row(out, "Name", a_actor->GetName() ? a_actor->GetName() : "(unnamed)");
		Row(out, "FormID", std::format("{:08X}", formID));
		const auto* race = a_actor->GetRace();
		Row(out, "Race", race && race->GetFormEditorID() ? race->GetFormEditorID() : "(unknown)");
		const auto* base = a_actor->GetActorBase();
		Row(out, "Sex", base && base->GetSex() == RE::SEX::kFemale ? "Female" : "Male");
		Row(out, "3D loaded", a_actor->Is3DLoaded() ? "yes" : "NO (deferred)");

		Header(out, "Body");
		Row(out, "Profile", BodyProfile::ResolvedName(a_actor));
		Row(out, "Guessed", BodyGuess(a_actor));
		int found = 0;
		int total = 0;
		const auto missing = SkeletonNodes(a_actor, found, total);
		Row(out, "Skeleton nodes", std::format("{} / {}", found, total));
		if (!a_actor->Is3DLoaded()) {
			Row(out, "  probe", "no 3D - unknown");
		}
		for (const auto& name : missing) {
			Row(out, "  missing", name);   // one per row: a long name cannot overflow
		}

		Header(out, "Contributions");
		const auto targets = ledger.TargetsOf(formID);
		bool any = false;
		for (const auto& target : targets) {
			const auto mods = ledger.ModsDriving(formID, target);
			if (mods.empty()) {
				continue;
			}
			// Sub-header per target, one short row per mod beneath it.
			Header(out, IsMorphTarget(target)
					? "morph " + ledger.SliderName(SliderOf(target))
					: target);
			for (const auto& mod : mods) {
				Row(out, "  " + mod, Num(ledger.GetContribution(formID, mod, target)));
				any = true;
			}
		}
		if (!any) {
			Row(out, "Registered", "nothing");
		}

		Header(out, "Applied");
		const float master = ledger.MasterScale();
		if (std::abs(master - 1.0f) > 0.0001f) {
			Row(out, "Overall magnitude", std::format("{}x", Num(master)));
		}

		std::set<std::string> sliders;
		std::set<std::string> nodeTargets;
		for (const auto& target : targets) {
			if (IsMorphTarget(target)) {
				sliders.insert(SliderOf(target));
			} else if (Vocabulary::Find(target)) {
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
			Row(out, "Output", "nothing");
		}
		for (const auto& sliderLower : sliders) {
			const std::string name = ledger.SliderName(sliderLower);
			const float folded = ledger.AggregateSlider(formID, sliderLower);
			const float scaled = folded * ledger.EffectiveScale(sliderLower);
			Row(out, name, Num(scaled));
			if (Skee::IsReady()) {
				// The readback proves OUR WRITE LANDED; it is NOT an availability
				// test (see BodyProfile.h). Worth a row only when it DISAGREES.
				const float back = Skee::ReadMorph(a_actor, name);
				if (std::abs(back - scaled) > 0.001f) {
					Row(out, "  skee disagrees", Num(back));
				}
			}
		}
		for (const auto& target : nodeTargets) {
			const float folded = ledger.Aggregate(formID, target);
			const float scaled = 1.0f + (folded - 1.0f) * ledger.EffectiveScale(target);
			Row(out, target + " (node)", Num(scaled));
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
