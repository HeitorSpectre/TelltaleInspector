#include "../TelltaleInspector.h"
#include <inttypes.h>
#include "imgui.h"
#include "../imstd/imgui_stdlib.h"

#include <unordered_map>
#include <vector>

bool gPropAnsiMode = false;

struct PropTextEntry {
	std::string path;
	String* pValue;
};

static std::string PropTextUnescape(const std::string& src) {
	std::string out;
	out.reserve(src.size());
	for (size_t i = 0; i < src.size(); i++) {
		char c = src[i];
		if (c == '\\' && (i + 1) < src.size()) {
			char n = src[++i];
			if (n == 'n') out += '\n';
			else if (n == 'r') out += '\r';
			else if (n == 't') out += '\t';
			else out += n;
		}
		else out += c;
	}
	return out;
}


static std::string GuessLanguageFromPath(const std::string& path) {
	std::string lower = lowercase(path);
	if (lower.find("english") != std::string::npos) return "English";
	if (lower.find("french") != std::string::npos) return "French";
	if (lower.find("german") != std::string::npos) return "German";
	if (lower.find("italian") != std::string::npos) return "Italian";
	if (lower.find("spanish") != std::string::npos) return "Spanish";
	if (lower.find("portuguese") != std::string::npos) return "Portuguese";
	if (lower.find("russian") != std::string::npos) return "Russian";
	if (lower.find("japanese") != std::string::npos) return "Japanese";
	if (lower.find("chinese") != std::string::npos) return "Chinese";
	if (lower.find("korean") != std::string::npos) return "Korean";
	return "Unknown";
}

static void CollectPropStringsRecursive(MetaClassDescription* pType, void* pValue, const std::string& path, std::vector<PropTextEntry>& out, int depth = 0) {
	if (!pType || !pValue || depth > 32)
		return;

	if (pType->mHash == hash_str) {
		out.push_back({ path, (String*)pValue });
		return;
	}

	if (IsType(pType, "PropertySet")) {
		PropertySet* pSet = (PropertySet*)pValue;
		for (int i = 0; i < pSet->mKeyMap.mSize; i++) {
			auto* pKey = pSet->mKeyMap.mpStorage + i;
			if (!pKey->mpValue)
				continue;
			std::string keyName = to_symbol(pKey->mKeyName.GetCRC());
			CollectPropStringsRecursive(pKey->mpValue->mpDataDescription, pKey->mpValue->mpValue, path + "." + keyName, out, depth + 1);
		}
		return;
	}

	if (starts_with("DCArray", pType->mpTypeInfoName)) {
		auto* pArray = (DCArray<void*>*)pValue;
		MetaClassDescription* pElem = pArray->GetContainerDataClassDescription();
		if (!pElem)
			return;
		for (int i = 0; i < pArray->GetSize(); i++) {
			CollectPropStringsRecursive(pElem, pArray->GetElement(i), path + "[" + std::to_string(i) + "]", out, depth + 1);
		}
		return;
	}

	if (starts_with("SArray", pType->mpTypeInfoName)) {
		auto* pArray = (SArray<void*, 1>*)pValue;
		MetaClassDescription* pElem = pArray->GetContainerDataDescription();
		if (!pElem)
			return;
		for (int i = 0; i < pArray->NUM_DATA_ELEM; i++) {
			void* pElemValue = (void*)((char*)pArray->mData + i * pElem->mClassSize);
			CollectPropStringsRecursive(pElem, pElemValue, path + "[" + std::to_string(i) + "]", out, depth + 1);
		}
		return;
	}

	if (starts_with("Map", pType->mpTypeInfoName)) {
		auto* pMap = (Map<void*, void*>*)pValue;
		MetaClassDescription* pValDesc = pMap->GetContainerDataClassDescription();
		if (!pValDesc)
			return;
		for (int i = 0; i < pMap->GetSize(); i++) {
			CollectPropStringsRecursive(pValDesc, pMap->GetVal(i), path + "{" + std::to_string(i) + "}", out, depth + 1);
		}
		return;
	}

	for (MetaMemberDescription* pMem = pType->mpFirstMember; pMem; pMem = pMem->mpNextMember) {
		CollectPropStringsRecursive(pMem->mpMemberDesc, (char*)pValue + pMem->mOffset, path + "." + pMem->mpName, out, depth + 1);
	}
}

static bool ExportPropStringsToTextFile(PropertySet& prop, const std::string& outPath, int* outCount, bool bAnsiMode) {
	std::vector<PropTextEntry> entries;
	CollectPropStringsRecursive(GetMetaClassDescription<PropertySet>(), &prop, "prop", entries);
	std::ofstream out(outPath, std::ios::out | std::ios::trunc);
	if (!out.is_open())
		return false;

	out << "# TelltaleInspector PROP text export v2\n";
	out << "# ENCODING: " << (bAnsiMode ? "ANSI" : "UTF8") << "\n";
	out << "# Edit only between [TEXT] and [/TEXT].\n\n";

	for (size_t i = 0; i < entries.size(); i++) {
		auto& entry = entries[i];
		out << "===== ENTRY " << (i + 1) << " =====\n";
		out << "# PATH: " << entry.path << "\n";
		out << "# LANGUAGE: " << GuessLanguageFromPath(entry.path) << "\n";
		out << "[TEXT]\n";
		out << std::string(*entry.pValue) << "\n";
		out << "[/TEXT]\n\n";
	}
	out.close();
	if (outCount)
		*outCount = (int)entries.size();
	return true;
}

static bool ImportPropStringsFromTextFile(PropertySet& prop, const std::string& inPath, int* outUpdated, bool bForceAnsiMode) {
	std::ifstream in(inPath);
	if (!in.is_open())
		return false;

	std::unordered_map<std::string, std::string> updates;
	std::string line;
	std::string currentPath;
	std::string currentText;
	bool readingText = false;
	bool bAnsiMode = bForceAnsiMode;

	while (std::getline(in, line)) {
		if (readingText) {
			if (line == "[/TEXT]") {
				if (!currentPath.empty())
					updates[currentPath] = currentText;
				currentPath.clear();
				currentText.clear();
				readingText = false;
			}
			else {
				if (!currentText.empty())
					currentText += "\n";
				currentText += line;
			}
			continue;
		}

		if (starts_with("# PATH:", line.c_str())) {
			currentPath = line.substr(7);
			while (!currentPath.empty() && currentPath[0] == ' ')
				currentPath.erase(currentPath.begin());
			continue;
		}

		if (starts_with("# ENCODING:", line.c_str())) {
			std::string enc = lowercase(line.substr(11));
			bAnsiMode = (enc.find("ansi") != std::string::npos);
			continue;
		}

		if (line == "[TEXT]") {
			readingText = true;
			currentText.clear();
			continue;
		}

		if (!line.empty() && line[0] != '#') {
			size_t tab = line.find('	');
			if (tab != std::string::npos)
				updates[line.substr(0, tab)] = PropTextUnescape(line.substr(tab + 1));
		}
	}
	in.close();

	std::vector<PropTextEntry> entries;
	CollectPropStringsRecursive(GetMetaClassDescription<PropertySet>(), &prop, "prop", entries);
	int changed = 0;
	for (auto& entry : entries) {
		auto it = updates.find(entry.path);
		if (it != updates.end()) {
			std::string decoded = bAnsiMode ? it->second : it->second;
			if (*entry.pValue == decoded.c_str())
				continue;
			*entry.pValue = decoded.c_str();
			changed++;
		}
	}

	if (outUpdated)
		*outUpdated = changed;
	return true;
}

void PropTask::_render() {
	//TODO EMBEDDED PROPS
	lister.frame_check_addrem = true;
	if (ext_access_gate && *ext_access_gate == false) {
		ImGui::Text("This PropertySet set is not available as the parent file was unloaded.");
		return;
	}
	resolve_buf[0] = 0;
	if (selected) {
		ImGui::Text("PropertySet Editor for: %s", game_n);
		ImGui::Text("Use Ctrl+N to create a new property in the current tree branch");
		ImGui::Text("Use Ctrl+D to delete the highlighted property   ");
		ImGui::Text("For lists/sets/maps: when the tooltip appears, Ctrl+N to add new mapping, Ctrl+D to delete hovered.");
		ImGui::Text("NOTE! The tooltip must display 'Map of' or 'Array of' when hovering to be able to create/delete!");
		ImGui::Text("Name:");
		ImGui::SameLine();
		ImGui::InputText("##pname", &prop_name, save_changes == nullptr ? 0 : ImGuiInputTextFlags_ReadOnly);
		ImGui::Checkbox("Replace Existing On Import", &replace_all);
		ImGui::Checkbox("Update Parents On Import", &update_parents);
		if (ImGui::Button("Import Property Set")) {
			TelltaleToolLib_SetBlowfishKey(game);
			nfdchar_t* outp = nullptr;
			if (NFD_OpenDialog("prop", 0, &outp) == NFD_OKAY) {
				std::string propPath = std::filesystem::path{ outp }.string();
				free(outp);
				prop_file_path = propPath;
				MetaStream mTempStream{};
				DataStreamFileDisc* prop = _OpenDataStreamFromDisc(propPath.c_str(), READ);
				if (!prop) {
					MessageBoxA(0, "Could not open the selected prop file for reading.", "Error", MB_ICONERROR);
					return;
				}
				mTempStream.Open(prop, MetaStreamMode::eMetaStream_Read, {});
				if (mTempStream.mbErrored) {
					MessageBoxA(0, "Could not open the prop file (meta stream error). Please contact me, using the contact tab above.", "Error", MB_ICONERROR);
				}
				else {
					PropertySet tempProp{};
					if (PerformMetaSerializeAsync<PropertySet>(&mTempStream, &tempProp) != eMetaOp_Succeed) {
						MessageBoxA(0, "Could not open the prop file (prop error). Please contact me, using the contact tab above.", "Error", MB_ICONERROR);
					}
					else {
						PropertySet* pDestProps = &Props();
						if (!imp_yet) {
							imp_yet = true;
							prop_name = std::filesystem::path{ propPath }.filename().string();
							pDestProps->mPropVersion = tempProp.mPropVersion;
							pDestProps->mPropertyFlags = 0;
						}
						/*update and import parent props if needed*/
						if (update_parents) {
							for (int i = 0; i < tempProp.mParentList.mSize; i++) {
								PropertySet::ParentInfo* parent = tempProp.mParentList.mpStorage + i;
								if (DCArray_Contains(pDestProps->mParentList, *parent))
									continue;
								DCArray_Push(pDestProps->mParentList, parent);
							}
						}
						/*import props*/
						for (int i = 0; i < tempProp.mKeyMap.mSize; i++) {
							PropertySet::KeyInfo* ky = tempProp.mKeyMap.mpStorage + i;
							//change prop val
							bool updated = false;
							for (int j = 0; j < pDestProps->mKeyMap.mSize; j++) {
								PropertySet::KeyInfo* existing = pDestProps->mKeyMap.mpStorage + j;
								if (existing->mKeyName == ky->mKeyName) {
									updated = true;
									if (replace_all) {
										existing->mFlags = ky->mFlags;
										existing->mpValue = std::move(ky->mpValue);
									}
									break;
								}
							}
							if (!updated) {
								pDestProps->mKeyMap.AddElementMove(0, 0, ky);
							}
						}
						versionInfo.InjectVersionInfo(mTempStream);
						//done
						return;//wait till next frame
					}
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Save To File")) {
			nfdchar_t* outp{ 0 };
			if (NFD_PickFolder(0, &outp, L"Select output folder") == NFD_OKAY) {
				std::string pth{ outp };
				pth += "/";
				std::string nm{ prop_name };
				if (nm.size() == 0) {
					MessageBoxA(0, "Please enter a file name into the property file name text field!", "Enter name", MB_ICONINFORMATION);
				}
				else {
					if (!ends_with(nm, ".prop"))
						nm += ".prop";
					pth += _STD move(nm);
					DataStreamFileDisc* ds = _OpenDataStreamFromDisc(pth.c_str(), WRITE);
					if (ds == nullptr) {
						MessageBoxA(0, "The file could not be opened for writing!", "Error", MB_ICONERROR);
					}
					else {
						MetaStream mTempStream{};
						mTempStream.InjectVersionInfo(versionInfo);
						mTempStream.Open(ds, MetaStreamMode::eMetaStream_Write, {});
						if (PerformMetaSerializeAsync<PropertySet>(&mTempStream, &Props()) != eMetaOp_Succeed) {
							MessageBoxA(0, "There was a problem serializing the property set to the meta stream. Please contact me (PROPTASK)", "Error", MB_ICONERROR);
						}
						else {
							mTempStream.Close();
							prop_file_path = pth;
							MessageBoxA(0, "The property set was exported!", "Success!", MB_ICONINFORMATION);
						}
					}
				}
				free(outp);
			}
		}
		PropertySet& prop = Props();
		ImGui::SameLine();
		if (ImGui::Button("Extract PROP to TXT")) {
			std::filesystem::path propPath{ prop_file_path };
			std::string txtName = prop_name.empty() ? "prop_export" : prop_name;
			if (ends_with(txtName, ".prop"))
				txtName = txtName.substr(0, txtName.size() - 5);
			txtName += ".txt";

			if (!propPath.empty()) {
				propPath = propPath.parent_path() / txtName;
			}
			else {
				propPath = txtName;
			}

			int exportedCount = 0;
			if (ExportPropStringsToTextFile(prop, propPath.string(), &exportedCount, gPropAnsiMode)) {
				std::string msg = "Exported ";
				msg += std::to_string(exportedCount);
				msg += " text entries to TXT: ";
				msg += propPath.string();
				MessageBoxA(0, msg.c_str(), "Success", MB_ICONINFORMATION);
			}
			else {
				MessageBoxA(0, "Could not write the output TXT file.", "Error", MB_ICONERROR);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reinsert TXT to PROP")) {
			nfdchar_t* inPath{};
			if (NFD_OpenDialog("txt", 0, &inPath) == NFD_OKAY) {
				int updatedCount = 0;
				if (ImportPropStringsFromTextFile(prop, std::filesystem::path{ inPath }.string(), &updatedCount, gPropAnsiMode)) {
					std::string msg = "Updated ";
					msg += std::to_string(updatedCount);
					msg += " text entries from TXT.";
					MessageBoxA(0, msg.c_str(), "Success", MB_ICONINFORMATION);
				}
				else {
					MessageBoxA(0, "Could not open/read the selected TXT file.", "Error", MB_ICONERROR);
				}
				free(inPath);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button(gPropAnsiMode ? "ANSI Mode: ON" : "ANSI Mode: OFF")) {
			gPropAnsiMode = !gPropAnsiMode;
		}
		ImGui::Separator();
		ImGui::SetWindowFontScale(1.3f);
		ImGui::Text("Parent Property Files");
		ImGui::SameLine();
		ImGui::InputText("##addpar", &nparent);
		ImGui::SameLine();
		bool sel = ImGui::Button("Add Parent");
		if (sel) {
			if (nparent.size() == 0) {
				MessageBoxA(0, "Please enter a name into the text box!", "!!", MB_ICONINFORMATION);
			}
			else {
				if (!ends_with(nparent, ".prop"))
					nparent += ".prop";
				bool b = false;
				Symbol sym{ nparent.c_str() };
				for (int i = 0; i < prop.mParentList.mSize; i++) {
					if (sym == prop.mParentList.mpStorage[i].mhParent.mHandleObjectInfo.mObjectName) {
						b = true;
						MessageBoxA(0, "That file is already a parent!", "!!", MB_ICONINFORMATION);
						break;
					}
				}
				if (!b) {
					Handle<PropertySet> hprop{};
					hprop.SetObjectName(sym);
					prop.AddParent(hprop);
					nparent = "";
				}
			}
		}
		ImGui::SameLine();
		sel = ImGui::Button("Remove Parent");
		if (sel && prop.mParentList.mSize > 0) {
			if (nparent.size() == 0) {
				MessageBoxA(0, "Please enter a name into the text box!", "!!", MB_ICONINFORMATION);
			}
			else {
				if (!ends_with(nparent, ".prop"))
					nparent += ".prop";
				Symbol sym{ nparent.c_str() };
				for (int i = 0; i < prop.mParentList.mSize; i++) {
					if (sym == prop.mParentList.mpStorage[i].mhParent.mHandleObjectInfo.mObjectName) {
						Handle<PropertySet> h{};
						h.SetObjectName(sym);
						prop.RemoveParent(h);
						break;
					}
				}
				nparent = "";
			}
		}
		ImGui::SetWindowFontScale(1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
		if (ImGui::BeginTable("split1", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable))
		{
			for (int i = 0; i < prop.mParentList.mSize; i++) {
				Handle<PropertySet> hProp = prop.mParentList.mpStorage[i].mhParent;
				std::string* kn;
				std::string* keyName = SymMap_ResolveHash(resolve_buf, sym_map, fn_map, hProp.mHandleObjectInfo.mObjectName.GetCRC(), file_name);
				if (keyName == nullptr) {
					kn = &(sym_map[hProp.mHandleObjectInfo.mObjectName.GetCRC()] = to_symbol(hProp.mHandleObjectInfo.mObjectName.GetCRC()));
				}
				else
					kn = keyName;
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text(kn->c_str());
			}
			ImGui::EndTable();
		}
		ImGui::PopStyleVar();
		ImGui::Separator();
		ImGui::SetWindowFontScale(1.3f);
		ImGui::Text("Properties");
		ImGui::SetWindowFontScale(1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
		if (ImGui::BeginTable("split", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable))
		{
			lister.PropTree(&prop, "Properties", (int)this & 0x7FFFFFF);
			ImGui::EndTable();
		}
		ImGui::PopStyleVar();
		lister.ignore_ctrln = false;
		//SHOULD NOT BE ABLE TO OPEN ANOTHER PROP AND IMPORT WHEN CURRENTLY INSPECTING ELEMENTS, IF ELEMENT IS REPLACED WITJ DIFFERENT TYPE THEN LOTS OF PROBLEMS HAPPEN
	}
	else {
		game = select_gameid_dropdown(game);
		if (game && ImGui::Button("Select")) {
			selected = true;
			game_n = sBlowfishKeys[TelltaleToolLib_GetGameKeyIndex(game)].game_name;
		}
	}
}
