/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "browser/components/sidebar/core/sidebar_model.h"

#include <algorithm>
#include <utility>

namespace sidebar {

SpaceId SidebarModel::AddSpace(std::string name) {
  Space space;
  space.id = next_id_++;
  space.name = std::move(name);
  spaces_.push_back(std::move(space));
  return spaces_.back().id;
}

bool SidebarModel::RemoveSpace(SpaceId id) {
  auto it = std::find_if(spaces_.begin(), spaces_.end(),
                         [id](const Space& s) { return s.id == id; });
  if (it == spaces_.end() || spaces_.size() == 1) {
    return false;
  }
  for (ItemId child : it->pinned) {
    EraseTree(child);
  }
  for (ItemId child : it->today) {
    EraseTree(child);
  }
  spaces_.erase(it);
  return true;
}

ItemId SidebarModel::AddTab(Location where,
                            std::string url,
                            std::string title,
                            TimeMs now,
                            size_t index) {
  std::optional<Location> loc = Resolve(where, ItemKind::kTab);
  if (!loc) {
    return kNoItem;
  }
  Item item;
  item.kind = ItemKind::kTab;
  item.location = *loc;
  item.title = std::move(title);
  item.url = std::move(url);
  if (loc->section != Section::kToday) {
    item.pinned_url = item.url;
  }
  item.last_active = now;
  return Insert(std::move(item), index);
}

ItemId SidebarModel::AddFolder(Location where, std::string title,
                               size_t index) {
  std::optional<Location> loc = Resolve(where, ItemKind::kFolder);
  if (!loc) {
    return kNoItem;
  }
  Item item;
  item.kind = ItemKind::kFolder;
  item.location = *loc;
  item.title = std::move(title);
  return Insert(std::move(item), index);
}

bool SidebarModel::Move(ItemId id, Location where, size_t index) {
  Item* item = MutableItem(id);
  if (!item) {
    return false;
  }
  std::optional<Location> loc = Resolve(where, item->kind);
  if (!loc) {
    return false;
  }
  // A folder can't go inside itself.
  if (loc->folder != kNoItem &&
      (loc->folder == id || IsInside(loc->folder, id))) {
    return false;
  }

  Detach(*item);
  bool was_today = item->location.section == Section::kToday;
  if (loc->section == Section::kToday) {
    item->pinned_url.clear();
  } else if (was_today) {
    item->pinned_url = item->url;
  }
  SetLocation(*item, *loc);

  std::vector<ItemId>& list = *MutableChildrenOf(*loc);
  list.insert(list.begin() + static_cast<ptrdiff_t>(std::min(index, list.size())),
              id);
  return true;
}

bool SidebarModel::Remove(ItemId id) {
  const Item* item = GetItem(id);
  if (!item) {
    return false;
  }
  Detach(*item);
  EraseTree(id);
  return true;
}

bool SidebarModel::Activate(ItemId id, TimeMs now) {
  Item* item = MutableItem(id);
  if (!item || item->kind != ItemKind::kTab) {
    return false;
  }
  item->last_active = now;
  return true;
}

bool SidebarModel::Navigate(ItemId id, std::string url, std::string title) {
  Item* item = MutableItem(id);
  if (!item || item->kind != ItemKind::kTab) {
    return false;
  }
  item->url = std::move(url);
  item->title = std::move(title);
  return true;
}

bool SidebarModel::ResetToPinnedUrl(ItemId id) {
  Item* item = MutableItem(id);
  if (!item || item->pinned_url.empty()) {
    return false;
  }
  item->url = item->pinned_url;
  return true;
}

std::vector<ItemId> SidebarModel::ArchiveIdle(TimeMs now,
                                              TimeMs max_idle_ms,
                                              const std::set<ItemId>& keep) {
  std::vector<ItemId> closed;
  for (Space& space : spaces_) {
    std::erase_if(space.today, [&](ItemId id) {
      const Item& item = items_.at(id);
      if (keep.contains(id) || now - item.last_active <= max_idle_ms) {
        return false;
      }
      archive_.insert(archive_.begin(),
                      ArchivedTab{space.id, item.title, item.url, now});
      closed.push_back(id);
      return true;
    });
  }
  for (ItemId id : closed) {
    items_.erase(id);
  }
  return closed;
}

ItemId SidebarModel::Restore(size_t index, TimeMs now) {
  if (index >= archive_.size() || spaces_.empty()) {
    return kNoItem;
  }
  ArchivedTab entry = std::move(archive_[index]);
  archive_.erase(archive_.begin() + static_cast<ptrdiff_t>(index));
  SpaceId space = GetSpace(entry.space) ? entry.space : spaces_.front().id;
  return AddTab({Section::kToday, space, kNoItem}, std::move(entry.url),
                std::move(entry.title), now);
}

const Item* SidebarModel::GetItem(ItemId id) const {
  auto it = items_.find(id);
  return it == items_.end() ? nullptr : &it->second;
}

const Space* SidebarModel::GetSpace(SpaceId id) const {
  auto it = std::find_if(spaces_.begin(), spaces_.end(),
                         [id](const Space& s) { return s.id == id; });
  return it == spaces_.end() ? nullptr : &*it;
}

const std::vector<ItemId>* SidebarModel::Children(Location where) const {
  // Tabs are allowed everywhere that exists, so this only checks existence.
  std::optional<Location> loc = Resolve(where, ItemKind::kTab);
  return loc ? ChildrenOf(*loc) : nullptr;
}

std::optional<Location> SidebarModel::Resolve(Location where,
                                              ItemKind kind) const {
  if (where.folder != kNoItem) {
    const Item* folder = GetItem(where.folder);
    if (!folder || folder->kind != ItemKind::kFolder) {
      return std::nullopt;
    }
    return Location{folder->location.section, folder->location.space,
                    where.folder};
  }
  if (kind == ItemKind::kFolder && where.section != Section::kPinned) {
    return std::nullopt;
  }
  if (where.section == Section::kFavorites) {
    return Location{Section::kFavorites, kNoSpace, kNoItem};
  }
  if (!GetSpace(where.space)) {
    return std::nullopt;
  }
  return Location{where.section, where.space, kNoItem};
}

const std::vector<ItemId>* SidebarModel::ChildrenOf(
    const Location& loc) const {
  if (loc.folder != kNoItem) {
    const Item* folder = GetItem(loc.folder);
    return folder ? &folder->children : nullptr;
  }
  if (loc.section == Section::kFavorites) {
    return &favorites_;
  }
  const Space* space = GetSpace(loc.space);
  if (!space) {
    return nullptr;
  }
  return loc.section == Section::kPinned ? &space->pinned : &space->today;
}

std::vector<ItemId>* SidebarModel::MutableChildrenOf(const Location& loc) {
  return const_cast<std::vector<ItemId>*>(ChildrenOf(loc));
}

Item* SidebarModel::MutableItem(ItemId id) {
  return const_cast<Item*>(GetItem(id));
}

bool SidebarModel::IsInside(ItemId id, ItemId folder) const {
  for (const Item* item = GetItem(id);
       item && item->location.folder != kNoItem;
       item = GetItem(item->location.folder)) {
    if (item->location.folder == folder) {
      return true;
    }
  }
  return false;
}

void SidebarModel::SetLocation(Item& item, const Location& where) {
  item.location = where;
  for (ItemId child : item.children) {
    SetLocation(items_.at(child), {where.section, where.space, item.id});
  }
}

void SidebarModel::Detach(const Item& item) {
  std::vector<ItemId>* list = MutableChildrenOf(item.location);
  if (list) {
    std::erase(*list, item.id);
  }
}

void SidebarModel::EraseTree(ItemId id) {
  auto it = items_.find(id);
  if (it == items_.end()) {
    return;
  }
  std::vector<ItemId> children = std::move(it->second.children);
  items_.erase(it);
  for (ItemId child : children) {
    EraseTree(child);
  }
}

ItemId SidebarModel::Insert(Item item, size_t index) {
  item.id = next_id_++;
  ItemId id = item.id;
  Location loc = item.location;
  items_.emplace(id, std::move(item));
  std::vector<ItemId>& list = *MutableChildrenOf(loc);
  list.insert(list.begin() + static_cast<ptrdiff_t>(std::min(index, list.size())),
              id);
  return id;
}

}  // namespace sidebar
