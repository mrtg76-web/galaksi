#include "Adg.h"

#include <sstream>

namespace gk {

std::string xmlEscape(const std::string& s)
{
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            case '\'': o += "&apos;"; break;
            default:
                if ((unsigned char) c >= 0x20 || c == '\t') o += c; // kontrol karakterleri XML'de geçersiz
        }
    }
    return o;
}

namespace {

std::vector<std::string> splitPath(const std::string& p)
{
    std::vector<std::string> parts;
    std::string cur;
    for (char c : p) {
        if (c == '/' || c == '\\') {
            if (!cur.empty()) parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}

void range(std::ostringstream& x, const char* tag, int mn, int mx, const std::string& ind)
{
    x << ind << "<" << tag << ">\n"
      << ind << "\t<Min Value=\"" << mn << "\" />\n"
      << ind << "\t<Max Value=\"" << mx << "\" />\n"
      << ind << "\t<CrossfadeMin Value=\"" << mn << "\" />\n"
      << ind << "\t<CrossfadeMax Value=\"" << mx << "\" />\n"
      << ind << "</" << tag << ">\n";
}

void onParam(std::ostringstream& x, const std::string& ind)
{
    x << ind << "<On>\n"
      << ind << "\t<LomId Value=\"0\" />\n"
      << ind << "\t<Manual Value=\"true\" />\n"
      << ind << "</On>\n";
}

void simpler(std::ostringstream& x, const AdgPad& p, const std::string& ind)
{
    const long long end = p.frames > 1 ? p.frames - 1 : 1;
    const std::string i1 = ind + "\t", i2 = i1 + "\t", i3 = i2 + "\t", i4 = i3 + "\t", i5 = i4 + "\t", i6 = i5 + "\t";
    x << ind << "<OriginalSimpler Id=\"0\">\n";
    x << i1 << "<LomId Value=\"0\" />\n" << i1 << "<IsExpanded Value=\"true\" />\n";
    onParam(x, i1);
    x << i1 << "<UserName Value=\"\" />\n";
    x << i1 << "<Player>\n";
    x << i2 << "<MultiSampleMap>\n";
    x << i3 << "<SampleParts>\n";
    x << i4 << "<MultiSamplePart Id=\"0\" HasImportedSlicePoints=\"false\" NeedsAnalysisData=\"true\">\n";
    x << i5 << "<LomId Value=\"0\" />\n";
    x << i5 << "<Name Value=\"" << xmlEscape(p.name) << "\" />\n";
    x << i5 << "<Selection Value=\"true\" />\n";
    x << i5 << "<IsActive Value=\"true\" />\n";
    x << i5 << "<Solo Value=\"false\" />\n";
    range(x, "KeyRange", 0, 127, i5);
    range(x, "VelocityRange", 1, 127, i5);
    range(x, "SelectorRange", 0, 127, i5);
    x << i5 << "<RootKey Value=\"60\" />\n";
    x << i5 << "<Detune Value=\"0\" />\n";
    x << i5 << "<TuneScale Value=\"100\" />\n";
    x << i5 << "<Panorama Value=\"0\" />\n";
    x << i5 << "<Volume Value=\"1\" />\n";
    x << i5 << "<Link Value=\"false\" />\n";
    x << i5 << "<SampleStart Value=\"0\" />\n";
    x << i5 << "<SampleEnd Value=\"" << end << "\" />\n";
    x << i5 << "<SustainLoop>\n"
      << i6 << "<Start Value=\"0\" />\n" << i6 << "<End Value=\"" << end << "\" />\n"
      << i6 << "<Mode Value=\"0\" />\n" << i6 << "<Crossfade Value=\"0\" />\n" << i6 << "<Detune Value=\"0\" />\n"
      << i5 << "</SustainLoop>\n";
    x << i5 << "<ReleaseLoop>\n"
      << i6 << "<Start Value=\"0\" />\n" << i6 << "<End Value=\"" << end << "\" />\n"
      << i6 << "<Mode Value=\"3\" />\n" << i6 << "<Crossfade Value=\"0\" />\n" << i6 << "<Detune Value=\"0\" />\n"
      << i5 << "</ReleaseLoop>\n";

    // Live 10 dosya referansı: .adg'ye göre göreli yol (aynı klasör) + Live'ın kendi araması için ipucu
    const std::string i7 = i6 + "\t", i8 = i7 + "\t";
    x << i5 << "<SampleRef>\n";
    x << i6 << "<FileRef>\n";
    x << i7 << "<HasRelativePath Value=\"true\" />\n";
    x << i7 << "<RelativePathType Value=\"1\" />\n";
    x << i7 << "<RelativePath />\n";
    x << i7 << "<Name Value=\"" << xmlEscape(p.fileName) << "\" />\n";
    x << i7 << "<Type Value=\"1\" />\n";
    x << i7 << "<Data />\n";
    x << i7 << "<RefersToFolder Value=\"false\" />\n";
    x << i7 << "<SearchHint>\n";
    const auto dirs = splitPath(p.absoluteDir);
    if (dirs.empty()) {
        x << i8 << "<PathHint />\n";
    } else {
        x << i8 << "<PathHint>\n";
        for (size_t k = 0; k < dirs.size(); ++k)
            x << i8 << "\t<RelativePathElement Id=\"" << k << "\" Dir=\"" << xmlEscape(dirs[k]) << "\" />\n";
        x << i8 << "</PathHint>\n";
    }
    x << i8 << "<FileSize Value=\"" << p.fileSize << "\" />\n";
    x << i8 << "<Crc Value=\"0\" />\n";
    x << i8 << "<MaxCrcSize Value=\"16384\" />\n";
    x << i8 << "<HasExtendedInfo Value=\"false\" />\n";
    x << i7 << "</SearchHint>\n";
    x << i7 << "<LivePackName Value=\"\" />\n";
    x << i7 << "<LivePackId Value=\"\" />\n";
    x << i6 << "</FileRef>\n";
    x << i6 << "<LastModDate Value=\"0\" />\n";
    x << i6 << "<SourceContext />\n";
    x << i6 << "<SampleUsageHint Value=\"0\" />\n";
    x << i6 << "<DefaultDuration Value=\"" << p.frames << "\" />\n";
    x << i6 << "<DefaultSampleRate Value=\"" << (long long) (p.sampleRate + 0.5) << "\" />\n";
    x << i5 << "</SampleRef>\n";
    x << i4 << "</MultiSamplePart>\n";
    x << i3 << "</SampleParts>\n";
    x << i2 << "</MultiSampleMap>\n";
    x << i1 << "</Player>\n";
    x << ind << "</OriginalSimpler>\n";
}

} // namespace

std::string drumRackXml(const std::vector<AdgPad>& pads, const std::string& creator)
{
    std::ostringstream x;
    x << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    x << "<Ableton MajorVersion=\"5\" MinorVersion=\"10.0_377\" SchemaChangeCount=\"3\" Creator=\""
      << xmlEscape(creator) << "\" Revision=\"\">\n";
    x << "\t<GroupDevicePreset>\n";
    x << "\t\t<OverwriteProtectionNumber Value=\"2561\" />\n";
    x << "\t\t<Device>\n";
    x << "\t\t\t<DrumGroupDevice Id=\"0\">\n";
    x << "\t\t\t\t<LomId Value=\"0\" />\n";
    x << "\t\t\t\t<IsExpanded Value=\"true\" />\n";
    onParam(x, "\t\t\t\t");
    x << "\t\t\t\t<UserName Value=\"Galaksi\" />\n";
    x << "\t\t\t\t<Branches />\n";
    x << "\t\t\t\t<IsBranchesListVisible Value=\"false\" />\n";
    x << "\t\t\t\t<AreDevicesVisible Value=\"true\" />\n";
    x << "\t\t\t\t<ReturnBranches />\n";
    x << "\t\t\t</DrumGroupDevice>\n";
    x << "\t\t</Device>\n";
    x << "\t\t<PresetRef />\n";
    x << "\t\t<BranchPresets>\n";
    int id = 0;
    for (const auto& p : pads) {
        const std::string b = "\t\t\t", b1 = b + "\t", b2 = b1 + "\t", b3 = b2 + "\t", b4 = b3 + "\t";
        x << b << "<DrumBranchPreset Id=\"" << id++ << "\">\n";
        x << b1 << "<Name Value=\"" << xmlEscape(p.name) << "\" />\n";
        x << b1 << "<IsSoloed Value=\"false\" />\n";
        x << b1 << "<DevicePresets>\n";
        x << b2 << "<AbletonDevicePreset Id=\"0\">\n";
        x << b3 << "<OverwriteProtectionNumber Value=\"2561\" />\n";
        x << b3 << "<Device>\n";
        simpler(x, p, b4);
        x << b3 << "</Device>\n";
        x << b3 << "<PresetRef />\n";
        x << b2 << "</AbletonDevicePreset>\n";
        x << b1 << "</DevicePresets>\n";
        x << b1 << "<MixerPreset>\n";
        x << b2 << "<AbletonDevicePreset Id=\"0\">\n";
        x << b3 << "<OverwriteProtectionNumber Value=\"2561\" />\n";
        x << b3 << "<Device>\n";
        x << b4 << "<AudioBranchMixerDevice Id=\"0\">\n";
        x << b4 << "\t<LomId Value=\"0\" />\n";
        onParam(x, b4 + "\t");
        x << b4 << "</AudioBranchMixerDevice>\n";
        x << b3 << "</Device>\n";
        x << b3 << "<PresetRef />\n";
        x << b2 << "</AbletonDevicePreset>\n";
        x << b1 << "</MixerPreset>\n";
        x << b1 << "<BranchSelectorRange>\n"
          << b2 << "<Min Value=\"0\" />\n" << b2 << "<Max Value=\"0\" />\n"
          << b2 << "<CrossfadeMin Value=\"0\" />\n" << b2 << "<CrossfadeMax Value=\"0\" />\n"
          << b1 << "</BranchSelectorRange>\n";
        x << b1 << "<ZoneSettings>\n";
        // Live, pad notasını ters sırada saklar: ReceivingNote = 128 - MIDI notası (C1=36 -> 92)
        x << b2 << "<ReceivingNote Value=\"" << (128 - p.midiNote) << "\" />\n";
        x << b2 << "<SendingNote Value=\"60\" />\n";
        x << b2 << "<ChokeGroup Value=\"0\" />\n";
        x << b1 << "</ZoneSettings>\n";
        x << b << "</DrumBranchPreset>\n";
    }
    x << "\t\t</BranchPresets>\n";
    x << "\t\t<ReturnBranchPresets />\n";
    x << "\t</GroupDevicePreset>\n";
    x << "</Ableton>\n";
    return x.str();
}

} // namespace gk
