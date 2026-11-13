/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "browser/components/importer/core/arc_importer.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace importer {
namespace {

using sidebar::ItemId;
using sidebar::kNoItem;
using sidebar::kNoSpace;
using sidebar::Location;
using sidebar::Section;
using sidebar::SidebarModel;
using sidebar::SpaceId;
using sidebar::TimeMs;

// ---- Minimal strict JSON reader --------------------------------------------
// ponytail: our own parser so core code builds without Chromium. In the
// browser layer, base::JSONReader could replace it if more importers need JSON.

struct JsonMember;

struct Json {
  enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };

  const Json* Find(std::string_view key) const;
  const std::string* FindString(std::string_view key) const {
    const Json* value = Find(key);
    return value && value->type == Type::kString ? &value->string : nullptr;
  }

  Type type = Type::kNull;
  bool boolean = false;
  std::string string;  // String contents, or a number's text.
  std::vector<Json> array;
  std::vector<JsonMember> object;
};

struct JsonMember {
  std::string key;
  Json value;
};

const Json* Json::Find(std::string_view key) const {
  for (const JsonMember& member : object) {
    if (member.key == key) {
      return &member.value;
    }
  }
  return nullptr;
}

void AppendUtf8(uint32_t cp, std::string& out) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

class JsonParser {
 public:
  explicit JsonParser(std::string_view in) : in_(in) {}

  std::optional<Json> Parse() {
    Json value;
    if (!ParseValue(value, 0)) {
      return std::nullopt;
    }
    SkipSpace();
    if (pos_ != in_.size()) {
      return std::nullopt;
    }
    return value;
  }

 private:
  static constexpr int kMaxDepth = 200;

  bool Peek(char c) const { return pos_ < in_.size() && in_[pos_] == c; }

  void SkipSpace() {
    while (Peek(' ') || Peek('\t') || Peek('\n') || Peek('\r')) {
      ++pos_;
    }
  }

  bool Consume(char c) {
    SkipSpace();
    if (!Peek(c)) {
      return false;
    }
    ++pos_;
    return true;
  }

  bool ConsumeWord(std::string_view word) {
    if (in_.substr(pos_, word.size()) != word) {
      return false;
    }
    pos_ += word.size();
    return true;
  }

  bool ParseValue(Json& out, int depth) {
    if (depth > kMaxDepth) {
      return false;
    }
    SkipSpace();
    if (pos_ >= in_.size()) {
      return false;
    }
    switch (in_[pos_]) {
      case '{':
        return ParseObject(out, depth);
      case '[':
        return ParseArray(out, depth);
      case '"':
        out.type = Json::Type::kString;
        return ParseString(out.string);
      case 't':
        out.type = Json::Type::kBool;
        out.boolean = true;
        return ConsumeWord("true");
      case 'f':
        out.type = Json::Type::kBool;
        return ConsumeWord("false");
      case 'n':
        return ConsumeWord("null");
      default:
        out.type = Json::Type::kNumber;
        return ParseNumber(out.string);
    }
  }

  bool ParseObject(Json& out, int depth) {
    out.type = Json::Type::kObject;
    ++pos_;  // '{'
    if (Consume('}')) {
      return true;
    }
    do {
      JsonMember member;
      SkipSpace();
      if (!Peek('"') || !ParseString(member.key) || !Consume(':') ||
          !ParseValue(member.value, depth + 1)) {
        return false;
      }
      out.object.push_back(std::move(member));
    } while (Consume(','));
    return Consume('}');
  }

  bool ParseArray(Json& out, int depth) {
    out.type = Json::Type::kArray;
    ++pos_;  // '['
    if (Consume(']')) {
      return true;
    }
    do {
      Json value;
      if (!ParseValue(value, depth + 1)) {
        return false;
      }
      out.array.push_back(std::move(value));
    } while (Consume(','));
    return Consume(']');
  }

  bool Digits() {
    size_t start = pos_;
    while (pos_ < in_.size() && in_[pos_] >= '0' && in_[pos_] <= '9') {
      ++pos_;
    }
    return pos_ > start;
  }

  bool ParseNumber(std::string& out) {
    size_t start = pos_;
    if (Peek('-')) {
      ++pos_;
    }
    if (Peek('0')) {
      ++pos_;
    } else if (!Digits()) {
      return false;
    }
    if (Peek('.')) {
      ++pos_;
      if (!Digits()) {
        return false;
      }
    }
    if (Peek('e') || Peek('E')) {
      ++pos_;
      if (Peek('+') || Peek('-')) {
        ++pos_;
      }
      if (!Digits()) {
        return false;
      }
    }
    out = in_.substr(start, pos_ - start);
    return true;
  }

  bool Hex4(uint32_t& out) {
    if (pos_ + 4 > in_.size()) {
      return false;
    }
    out = 0;
    for (int i = 0; i < 4; ++i) {
      char c = in_[pos_ + i];
      out <<= 4;
      if (c >= '0' && c <= '9') {
        out |= c - '0';
      } else if (c >= 'a' && c <= 'f') {
        out |= c - 'a' + 10;
      } else if (c >= 'A' && c <= 'F') {
        out |= c - 'A' + 10;
      } else {
        return false;
      }
    }
    pos_ += 4;
    return true;
  }

  bool ParseString(std::string& out) {
    ++pos_;  // Opening quote.
    while (pos_ < in_.size()) {
      char c = in_[pos_++];
      if (c == '"') {
        return true;
      }
      if (static_cast<unsigned char>(c) < 0x20) {
        return false;
      }
      if (c != '\\') {
        out += c;
        continue;
      }
      if (pos_ >= in_.size()) {
        return false;
      }
      switch (in_[pos_++]) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          uint32_t cp;
          if (!Hex4(cp)) {
            return false;
          }
          if (cp >= 0xD800 && cp <= 0xDBFF && in_.substr(pos_, 2) == "\\u") {
            size_t save = pos_;
            pos_ += 2;
            uint32_t low;
            if (Hex4(low) && low >= 0xDC00 && low <= 0xDFFF) {
              cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            } else {
              pos_ = save;
            }
          }
          if (cp >= 0xD800 && cp <= 0xDFFF) {
            cp = 0xFFFD;  // Unpaired surrogate.
          }
          AppendUtf8(cp, out);
          break;
        }
        default:
          return false;
      }
    }
    return false;  // Unterminated.
  }

  std::string_view in_;
  size_t pos_ = 0;
};

// ---- Arc's format ------------------------------------------------------------

// Arc stores maps as flat arrays: [key, value, key, value, ...].
template <typename Fn>
void ForEachPair(const Json* list, Fn fn) {
  if (!list || list->type != Json::Type::kArray) {
    return;
  }
  for (size_t i = 0; i + 1 < list->array.size(); i += 2) {
    fn(list->array[i], list->array[i + 1]);
  }
}

// {"default": true} or {"custom": {"_0": {"directoryBasename": "Profile 1"}}}.
// The custom shape follows Swift's enum encoding; not yet checked against a
// real multi-profile export.
std::string ProfileDirectory(const Json* profile) {
  const Json* custom = profile ? profile->Find("custom") : nullptr;
  const Json* value = custom ? custom->Find("_0") : nullptr;
  const std::string* dir = value ? value->FindString("directoryBasename")
                                 : nullptr;
  return dir ? *dir : std::string();
}

class ArcReader {
 public:
  ArcReader(const Json& container, TimeMs now, ArcImport& out)
      : container_(container), now_(now), out_(out) {}

  void Run() {
    ForEachPair(container_.Find("items"), [&](const Json&, const Json& item) {
      if (const std::string* id = item.FindString("id")) {
        items_.emplace(*id, &item);
      }
    });
    ForEachPair(container_.Find("topAppsContainerIDs"),
                [&](const Json& profile, const Json& id) {
                  if (id.type != Json::Type::kString) {
                    return;
                  }
                  ImportChildren(ModelFor(ProfileDirectory(&profile)),
                                 {Section::kFavorites, kNoSpace, kNoItem},
                                 id.string, 0);
                });
    ForEachPair(container_.Find("spaces"), [&](const Json&, const Json& space) {
      if (space.type == Json::Type::kObject) {
        ImportSpace(space);
      }
    });
  }

 private:
  // Arc's own folders nest only a few levels; this guards hostile files.
  static constexpr int kMaxDepth = 64;

  // The returned reference is invalidated by the next call; use it at once.
  SidebarModel& ModelFor(const std::string& directory) {
    for (ArcProfile& profile : out_.profiles) {
      if (profile.directory == directory) {
        return profile.sidebar;
      }
    }
    out_.profiles.push_back({directory, SidebarModel()});
    return out_.profiles.back().sidebar;
  }

  void ImportSpace(const Json& space) {
    SidebarModel& model = ModelFor(ProfileDirectory(space.Find("profile")));
    const std::string* title = space.FindString("title");
    SpaceId id = model.AddSpace(title && !title->empty() ? *title : "Space");
    ForEachPair(space.Find("containerIDs"),
                [&](const Json& kind, const Json& container) {
                  if (kind.type != Json::Type::kString ||
                      container.type != Json::Type::kString) {
                    return;
                  }
                  if (kind.string == "pinned") {
                    ImportChildren(model, {Section::kPinned, id, kNoItem},
                                   container.string, 0);
                  } else if (kind.string == "unpinned") {
                    ImportChildren(model, {Section::kToday, id, kNoItem},
                                   container.string, 0);
                  }
                });
  }

  void ImportChildren(SidebarModel& model,
                      const Location& where,
                      const std::string& parent_id,
                      int depth) {
    const Json* parent = FindItem(parent_id);
    const Json* children = parent ? parent->Find("childrenIds") : nullptr;
    if (!children) {
      return;
    }
    for (const Json& child : children->array) {
      if (child.type == Json::Type::kString) {
        ImportItem(model, where, child.string, depth);
      }
    }
  }

  void ImportItem(SidebarModel& model,
                  const Location& where,
                  const std::string& id,
                  int depth) {
    // Each item is imported once, which also breaks childrenIds cycles.
    if (depth > kMaxDepth || !visited_.insert(id).second) {
      return;
    }
    const Json* item = FindItem(id);
    const Json* data = item ? item->Find("data") : nullptr;
    if (!data) {
      ++out_.skipped;
      return;
    }
    if (const Json* tab = data->Find("tab")) {
      const std::string* url = tab->FindString("savedURL");
      if (!url) {
        ++out_.skipped;
        return;
      }
      const std::string* saved_title = tab->FindString("savedTitle");
      model.AddTab(where, *url, Title(*item, saved_title ? *saved_title : ""),
                   now_, SidebarModel::kEnd);
      return;
    }
    bool is_folder = data->Find("list") != nullptr;
    if (is_folder && where.section == Section::kPinned) {
      ItemId folder =
          model.AddFolder(where, Title(*item, "Folder"), SidebarModel::kEnd);
      ImportChildren(model, {Section::kPinned, kNoSpace, folder}, id,
                     depth + 1);
      return;
    }
    if (is_folder || data->Find("splitView")) {
      // No split items yet, and folders only live in Pinned: keep the tabs,
      // drop the grouping.
      ImportChildren(model, where, id, depth + 1);
      return;
    }
    ++out_.skipped;  // Easels, Notes, Arc's own pages, ...
  }

  const Json* FindItem(const std::string& id) const {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second;
  }

  // A tab or folder the user renamed has a "title"; otherwise use `fallback`.
  static std::string Title(const Json& item, const std::string& fallback) {
    const std::string* title = item.FindString("title");
    return title && !title->empty() ? *title : fallback;
  }

  const Json& container_;
  const TimeMs now_;
  ArcImport& out_;
  std::unordered_map<std::string, const Json*> items_;
  std::unordered_set<std::string> visited_;
};

}  // namespace

std::optional<ArcImport> ImportArcSidebar(std::string_view json,
                                          sidebar::TimeMs now) {
  std::optional<Json> root = JsonParser(json).Parse();
  if (!root) {
    return std::nullopt;
  }
  const Json* sidebar = root->Find("sidebar");
  const Json* containers = sidebar ? sidebar->Find("containers") : nullptr;
  if (!containers) {
    return std::nullopt;
  }
  // One container holds the data; the other is an empty {"global": {}}.
  for (const Json& container : containers->array) {
    if (container.Find("items") && container.Find("spaces")) {
      ArcImport result;
      ArcReader(container, now, result).Run();
      return result;
    }
  }
  return std::nullopt;
}

}  // namespace importer
