#include "pch.h"
#include "RpakLib.h"
#include "Path.h"
#include "Directory.h"

void RpakLib::BuildLocalizationInfo(const RpakLoadAsset& Asset, ApexAsset& Info)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));

	Info.Name = string::Format("locl_0x%llx", Asset.NameHash);
	Info.Type = ApexAssetType::Localization;
	Info.Status = ApexAssetStatus::Loaded;
	Info.Info = "N/A";
}

// credit: https://stackoverflow.com/a/33799784 (vog & malat)
static std::string EscapeString(const std::string& s) {
	std::ostringstream o;
	for (auto c = s.cbegin(); c != s.cend(); c++) {
		switch (*c) {
		case '"': o << "\\\""; break;
		case '\\': o << "\\\\"; break;
		case '\b': o << "\\b"; break;
		case '\f': o << "\\f"; break;
		case '\n': o << "\\n"; break;
		case '\r': o << "\\r"; break;
		case '\t': o << "\\t"; break;
		default:
			if ('\x00' <= *c && *c <= '\x1f') {
				o << "\\u"
					<< std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(*c);
			}
			else {
				o << *c;
			}
		}
	}
	return o.str();
}

void RpakLib::ExportLocalization(const RpakLoadAsset& Asset, const string& Path)
{
	string DestinationPath = IO::Path::Combine(Path, string::Format("locl_0x%llx", Asset.NameHash) + ".json");

	if (!Utils::ShouldWriteFile(DestinationPath))
		return;

	List<LocalizationEntry> Entries = this->ExtractLocalization(Asset);

	std::ofstream locl_out(DestinationPath.ToCString(), std::ios::out);

	locl_out << "{\n";
	for (int i = 0; i < Entries.Count(); i++)
	{
		auto const& Entry = Entries[i];
		locl_out << "  \"" << string::Format("%llx", Entry.Hash) << "\": \"" << EscapeString(Entry.Text.ToCString()) << "\"";

		if (i != (Entries.Count() - 1)) {
			locl_out << ",";
		}

		locl_out << "\n";
	}

	locl_out << "}";

	locl_out.close();
}

List<LocalizationEntry> RpakLib::ExtractLocalization(const RpakLoadAsset& Asset)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));

	LocalizationHeader LoclHdr = Reader.Read<LocalizationHeader>();

	RpakStream->SetPosition(this->GetFileOffset(Asset, LoclHdr.KeyEntriesIndex, LoclHdr.KeyEntriesOffset));

	auto values = List<LocalizationEntry>(LoclHdr.ValueCount);

	auto valueArrBaseOffset = this->GetFileOffset(Asset, LoclHdr.ValueEntriesIndex, LoclHdr.ValueEntriesOffset);

	for (int i = 0; i < LoclHdr.KeyCount; i++) {
		auto keyEntry = Reader.Read<LocalizationEntryMeta>();

		auto oldOffset = RpakStream->GetPosition();

		if (keyEntry.Hash == 0uL) {
			continue;
		}

		RpakStream->SetPosition(valueArrBaseOffset + (static_cast<uint64_t>(keyEntry.OffsetHalf) * 2));

		LocalizationEntry le;

		le.Hash = keyEntry.Hash;
		le.Text = Reader.ReadWCString().ToString();
		le.unk1 = keyEntry.unk1;

		values.EmplaceBack(le);

		RpakStream->SetPosition(oldOffset);
	}

	return values;
}