#pragma once

// SLIF's own arithmetic, ported one-for-one from SLIF_Math.psc and
// SLIF_Calc.addCalculationType. These formulas are the part of the reference
// that is genuinely well tested, so they are reproduced rather than improved:
// same bounds handling, same six calculation types, same "floor to 1.0" on an
// empty or non-positive result, same Top X default.

namespace SLIFNG::Calc
{
	// SLIF_Math.SetBounds: tolerates an inverted pair by swapping it, which is
	// also what keeps a consumer sending minimum > maximum from being UB here.
	inline float SetBounds(float a_value, float a_min, float a_max)
	{
		const float lo = (std::min)(a_min, a_max);
		const float hi = (std::max)(a_min, a_max);
		return (std::clamp)(a_value, lo, hi);
	}

	inline float Average(float a_first, float a_second)
	{
		return (a_first + a_second) / 2.0f;
	}

	// Config.json "calculation_type". The reference's default is 0 = Top X, NOT
	// highest wins - a detail worth keeping, because it is what a migrating save
	// has been looking at.
	enum class Type : std::uint32_t
	{
		kTopX = 0,
		kHighestWins = 1,
		kSubtractOne = 2,
		kSquareRoot = 3,
		kAverage = 4,
		kAdditive = 5,
	};

	inline constexpr std::uint32_t kDefaultTopX = 3;

	inline const char* TypeName(Type a_type)
	{
		switch (a_type) {
		case Type::kTopX:
			return "top_x";
		case Type::kHighestWins:
			return "highest_wins";
		case Type::kSubtractOne:
			return "substract_one";
		case Type::kSquareRoot:
			return "square_root";
		case Type::kAverage:
			return "average";
		case Type::kAdditive:
			return "additive";
		default:
			return "?";
		}
	}

	inline bool IsValidType(std::uint32_t a_raw)
	{
		return a_raw <= static_cast<std::uint32_t>(Type::kAdditive);
	}

	// SLIF_Calc.addCalculationType over the already-bounded, already-multiplied
	// per-mod contributions. Contributions <= 0 are skipped exactly as the
	// reference skips them (`if tempValue > 0.0`), and every type floors to the
	// neutral 1.0 when nothing positive contributed.
	inline float Fold(Type a_type, std::vector<float> a_values, std::uint32_t a_topX = kDefaultTopX)
	{
		std::erase_if(a_values, [](float v) { return v <= 0.0f; });
		if (a_values.empty()) {
			return 1.0f;
		}
		// Descending, which is all addToValues' insertion sort achieves.
		std::sort(a_values.begin(), a_values.end(), std::greater<float>());

		switch (a_type) {
		case Type::kHighestWins:
			return a_values.front();

		case Type::kTopX:
			{
				const auto count = (std::min)(static_cast<std::size_t>(a_topX), a_values.size());
				float total = a_values[0];
				for (std::size_t x = 1; x < count; ++x) {
					// arr[x] / (3 * x): second place counts a third, third a sixth.
					total += a_values[x] / (3.0f * static_cast<float>(x));
				}
				return total > 0.0f ? total : 1.0f;
			}

		case Type::kSubtractOne:
			{
				float total = 1.0f;
				for (const float v : a_values) {
					total += v - 1.0f;
				}
				// MaxFloat(0.0, ...) - note the reference does NOT floor this one
				// to 1.0, so a stack of shrink values can legitimately reach 0.
				return (std::max)(0.0f, total);
			}

		case Type::kSquareRoot:
			{
				float total = 0.0f;
				for (const float v : a_values) {
					total += v * v;
				}
				total = total > 0.0f ? std::sqrt(total) : 0.0f;
				return total > 0.0f ? total : 1.0f;
			}

		case Type::kAverage:
			{
				float total = 0.0f;
				for (const float v : a_values) {
					total += v / static_cast<float>(a_values.size());
				}
				return total > 0.0f ? total : 1.0f;
			}

		case Type::kAdditive:
			{
				// A plain sum of scales, as in the reference (1.5 + 1.4 -> 2.9).
				float total = 0.0f;
				for (const float v : a_values) {
					total += v;
				}
				return total > 0.0f ? total : 1.0f;
			}

		default:
			return 1.0f;
		}
	}
}
