/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BROWSER_COMPONENTS_SIDEBAR_CORE_SIDEBAR_MODEL_H_
#define BROWSER_COMPONENTS_SIDEBAR_CORE_SIDEBAR_MODEL_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace sidebar {

using ItemId = int64_t;
using SpaceId = int64_t;
// Milliseconds since the Unix epoch. The browser layer converts base::Time.
using TimeMs = int64_t;

inline constexpr ItemId kNoItem = 0;
inline constexpr SpaceId kNoSpace = 0;

enum class Section { kFavorites, kPinned, kToday };
enum class ItemKind { kTab, kFolder };

// Where an item lives. `space` is ignored for kFavorites. `folder` is kNoItem
// for the section root. Folders only exist in kPinned; when `folder` is set,
// `section` and `space` are taken from the folder.
struct Location {
  Section section = Section::kToday;
  SpaceId space = kNoSpace;
  ItemId folder = kNoItem;

  bool operator==(const Location&) const = default;
};

struct Item {
  ItemId id = kNoItem;
  ItemKind kind = ItemKind::kTab;
  Location location;
  std::string title;
  std::string url;         // Tabs only.
  std::string pinned_url;  // Pinned tabs and favorites: where "reset" goes.
  TimeMs last_active = 0;
  std::vector<ItemId> children;  // Folders only, in display order.
};

struct Space {
  SpaceId id = kNoSpace;
  std::string name;
  std::vector<ItemId> pinned;  // Tabs and folders.
  std::vector<ItemId> today;   // Tabs only, newest on top.
};

struct ArchivedTab {
  SpaceId space = kNoSpace;
  std::string title;
  std::string url;
  TimeMs archived_at = 0;
};

// The sidebar of one profile: Favorites shared by every Space, and per Space a
// pinned tree (tabs and nested folders) plus a flat list of Today tabs that
// archive themselves when idle.
//
// Pure data with no Chromium dependencies; the browser layer maps live tabs
// onto items. Unknown ids and invalid locations are rejected (kNoItem/false).
class SidebarModel {
 public:
  static constexpr size_t kEnd = static_cast<size_t>(-1);

  SpaceId AddSpace(std::string name);
  // Deletes the Space and everything in it. The last Space can't be removed.
  bool RemoveSpace(SpaceId space);

  // `index` is clamped to the list; kEnd appends. New tabs go to the top of
  // Today by default.
  ItemId AddTab(Location where,
                std::string url,
                std::string title,
                TimeMs now,
                size_t index = 0);
  ItemId AddFolder(Location where, std::string title, size_t index = kEnd);

  // Moves an item (a folder keeps its contents). `index` is the position the
  // item ends up at. Leaving Today pins the tab (pinned_url = url); entering
  // Today unpins it.
  bool Move(ItemId id, Location where, size_t index);
  // Removes a tab, or a folder and everything inside it.
  bool Remove(ItemId id);

  bool Activate(ItemId id, TimeMs now);
  bool Navigate(ItemId id, std::string url, std::string title);
  // Sends a pinned tab or favorite back to the URL it was pinned with.
  bool ResetToPinnedUrl(ItemId id);

  // Closes Today tabs idle for longer than `max_idle_ms`, except those in
  // `keep` (tabs currently on screen), and adds them to the archive, newest
  // first. Returns the closed ids so the browser layer can close live tabs.
  std::vector<ItemId> ArchiveIdle(TimeMs now,
                                  TimeMs max_idle_ms,
                                  const std::set<ItemId>& keep);
  // Reopens archive entry `index` at the top of Today in its Space, or in the
  // first Space if that one was deleted.
  ItemId Restore(size_t index, TimeMs now);

  const Item* GetItem(ItemId id) const;
  const Space* GetSpace(SpaceId id) const;
  // Children of `where`, or nullptr if it doesn't exist.
  const std::vector<ItemId>* Children(Location where) const;
  const std::vector<Space>& spaces() const { return spaces_; }
  const std::vector<ItemId>& favorites() const { return favorites_; }
  const std::vector<ArchivedTab>& archive() const { return archive_; }

 private:
  // Validates `where` for an item of `kind` and fills section/space from the
  // folder if one is given.
  std::optional<Location> Resolve(Location where, ItemKind kind) const;
  const std::vector<ItemId>* ChildrenOf(const Location& resolved) const;
  std::vector<ItemId>* MutableChildrenOf(const Location& resolved);
  Item* MutableItem(ItemId id);
  bool IsInside(ItemId id, ItemId folder) const;
  void SetLocation(Item& item, const Location& where);
  void Detach(const Item& item);
  void EraseTree(ItemId id);
  ItemId Insert(Item item, size_t index);

  int64_t next_id_ = 1;
  std::unordered_map<ItemId, Item> items_;
  std::vector<Space> spaces_;
  std::vector<ItemId> favorites_;
  // ponytail: unbounded in memory; the browser layer persists it and should
  // prune entries older than ~30 days, like Arc.
  std::vector<ArchivedTab> archive_;
};

}  // namespace sidebar

#endif  // BROWSER_COMPONENTS_SIDEBAR_CORE_SIDEBAR_MODEL_H_
