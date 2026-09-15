#pragma once

// Bounds-checked co-save read/write helpers, following the SexlabArousedNG
// patterns (see PLAN.md "Serialization template"): every read decrements the
// remaining record length and throws on truncation, so a corrupt or short
// record fails the load cleanly instead of reading garbage.

namespace SLIFNG::Serialization
{
	inline void WriteString(SKSE::SerializationInterface* a_intfc, const std::string& a_string)
	{
		const auto size = static_cast<std::uint32_t>(a_string.size());
		a_intfc->WriteRecordData(&size, sizeof(size));
		if (size > 0) {
			a_intfc->WriteRecordData(a_string.data(), size);
		}
	}

	template <typename T>
	void Write(SKSE::SerializationInterface* a_intfc, const T& a_value)
	{
		a_intfc->WriteRecordData(&a_value, sizeof(T));
	}

	template <typename T>
	T Read(SKSE::SerializationInterface* a_intfc, std::uint32_t& a_length)
	{
		constexpr auto size = static_cast<std::uint32_t>(sizeof(T));
		if (size > a_length) {
			throw std::length_error("cosave record ended unexpectedly");
		}
		a_length -= size;
		T result{};
		a_intfc->ReadRecordData(&result, size);
		return result;
	}

	inline std::string ReadString(SKSE::SerializationInterface* a_intfc, std::uint32_t& a_length)
	{
		const auto size = Read<std::uint32_t>(a_intfc, a_length);
		if (size > a_length) {
			throw std::length_error("cosave string ended unexpectedly");
		}
		if (size > 0x10000) {
			throw std::length_error("cosave string implausibly long (corrupt record)");
		}
		a_length -= size;
		std::string result(size, '\0');
		if (size > 0) {
			a_intfc->ReadRecordData(result.data(), size);
		}
		return result;
	}
}
