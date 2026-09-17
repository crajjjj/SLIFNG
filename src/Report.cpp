#include "Report.h"

#include "API/SKEE.h"
#include "BodyProfile.h"
#include "Calc.h"
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

	}

	// LAYOUT RULES - this renders in a TWO-COLUMN SkyUI MCM:
	//   * a label past ~30 chars collides with its own value column;
	//   * a value past ~20 chars runs left across the label;
	//   * an EMPTY value means "section header", so a placeholder line must
	//     still carry a value or it draws as a header complete with divider.
	// The log reuses these rows, so short helps there too.
	//
	// The report is split in two so the MCM can pair the halves into real
	// columns: TOP_TO_BOTTOM only flows right once the LEFT column is FULL, and
	// this page is never that long, so it was sitting one-sided.

	std::vector<RE::BSFixedString> IdentityRows(RE::Actor* a_actor)
	{
		std::vector<RE::BSFixedString> out;
		if (!a_actor) {
			Row(out, "Actor", "none selected");
			return out;
		}
		Header(out, "ACTOR");
		Row(out, "Name", a_actor->GetName() ? a_actor->GetName() : "(unnamed)");
		Row(out, "FormID", std::format("{:08X}", a_actor->GetFormID()));
		const auto* race = a_actor->GetRace();
		Row(out, "Race", race && race->GetFormEditorID() ? race->GetFormEditorID() : "(unknown)");
		const auto* base = a_actor->GetActorBase();
		Row(out, "Sex", base && base->GetSex() == RE::SEX::kFemale ? "Female" : "Male");
		// NOTE: no "3D loaded" row. This page can only ever show the player or a
		// crosshair target, both of which are loaded by definition, so it read
		// "yes" forever. The per-apply [Apply] log lines still carry 3D state,
		// which is where an unloaded actor actually shows up.

		Header(out, "BODY");
		Row(out, "Profile", BodyProfile::ResolvedName(a_actor));
		int found = 0;
		int total = 0;
		const auto missing = SkeletonNodes(a_actor, found, total);
		Row(out, "Skeleton nodes", std::format("{} / {}", found, total));
		for (const auto& name : missing) {
			Row(out, "  missing", name);   // one per row: a long name cannot overflow
		}
		return out;
	}

	std::vector<RE::BSFixedString> StateRows(RE::Actor* a_actor)
	{
		std::vector<RE::BSFixedString> out;
		if (!a_actor) {
			return out;
		}
		auto& ledger = Ledger::GetSingleton();
		const auto formID = a_actor->GetFormID();

		Header(out, "CONTRIBUTIONS");
		const auto targets = ledger.TargetsOf(formID);
		bool any = false;
		for (const auto& target : targets) {
			const auto mods = ledger.ModsDriving(formID, target);
			if (mods.empty()) {
				continue;
			}
			// Sub-header per target, one short row per mod beneath it. Node
			// keys render decrypted: "node Belly (NPC Belly)", not slif_belly.
			Header(out, IsMorphTarget(target)
					? "morph " + ledger.SliderName(SliderOf(target))
					: "node " + Vocabulary::Describe(target));
			for (const auto& mod : mods) {
				Row(out, "  " + mod, Num(ledger.GetContribution(formID, mod, target)));
				any = true;
			}
			// What the node value BECOMES on this actor's body, so the page
			// answers "how does slif_belly relate to my morphs" by itself:
			// either the profile transforms it into sliders (weight per +1.0
			// of scale), or it stays a skeleton bone scale.
			if (!IsMorphTarget(target)) {
				const auto* blends = BodyProfile::BlendFor(a_actor, target);
				if (blends && !blends->empty()) {
					for (const auto& blend : *blends) {
						Row(out, "  > drives " + blend.slider,
							std::format("{} / +1.0", Num(blend.weight)));
					}
				} else {
					Row(out, "  > drives", "bone scale");
				}
			}
		}
		if (!any) {
			Row(out, "Registered", "nothing");
		}

		Header(out, "APPLIED");
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
			Row(out, Vocabulary::Describe(target) + " [node]", Num(scaled));
		}

		Header(out, "AGGREGATION");
		// "Across mods": the calc type folds per-mod values on one target -
		// node or slider alike; one mod's own node+morph layers still add.
		Row(out, "Calc (across mods)", Calc::TypeName(ledger.GetMode()));
		return out;
	}

	std::vector<RE::BSFixedString> ForActor(RE::Actor* a_actor)
	{
		auto out = IdentityRows(a_actor);
		for (auto& row : StateRows(a_actor)) {
			out.push_back(std::move(row));
		}
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
