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

bool SidebarModel::EditSpace(SpaceId id,
                             std::string name,
                             SpaceIcon icon,
                             SpaceTheme theme) {
  Space* space = MutableSpace(id);
  if (!space) {
    return false;
  }
  space->name = std::move(name);
  space->icon = std::move(icon);
  space->theme = std::move(theme);
  return true;
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
  std::optional<Location> loc = Resolve(where, ItemKind::kTab, kNoItem);
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
  std::optional<Location> loc = Resolve(where, ItemKind::kFolder, kNoItem);
  if (!loc) {
    return kNoItem;
  }
  Item item;
  item.kind = ItemKind::kFolder;
  item.location = *loc;
  item.title = std::move(title);
  return Insert(std::move(item), index);
}

ItemId SidebarModel::CreateSplit(const std::vector<ItemId>& tabs,
                                 SplitLayout layout) {
  if (tabs.size() < 2 || tabs.size() > kMaxSplitTabs) {
    return kNoItem;
  }
  for (size_t i = 0; i < tabs.size(); ++i) {
    const Item* tab = GetItem(tabs[i]);
    auto earlier = tabs.begin() + static_cast<ptrdiff_t>(i);
    if (!tab || tab->kind != ItemKind::kTab ||
        std::find(tabs.begin(), earlier, tabs[i]) != earlier) {
      return kNoItem;
    }
  }
  Location where = GetItem(tabs[0])->location;
  if (!Resolve(where, ItemKind::kSplit, kNoItem)) {
    return kNoItem;  // Favorites, or the first tab is already in a split.
  }
  const std::vector<ItemId>& list = *Children(where);
  size_t index = static_cast<size_t>(
      std::find(list.begin(), list.end(), tabs[0]) - list.begin());

  Item split;
  split.kind = ItemKind::kSplit;
  split.location = where;
  split.layout = layout;
  ItemId id = Insert(std::move(split), index);
  for (ItemId tab : tabs) {
    Move(tab, {where.section, where.space, id}, kEnd);
  }
  return id;
}

bool SidebarModel::Move(ItemId id, Location where, size_t index) {
  Item* item = MutableItem(id);
  if (!item) {
    return false;
  }
  std::optional<Location> loc = Resolve(where, item->kind, id);
  if (!loc) {
    return false;
  }
  // Nothing can go inside itself.
  if (loc->parent != kNoItem &&
      (loc->parent == id || IsInside(loc->parent, id))) {
    return false;
  }

  ItemId old_parent = item->location.parent;
  Detach(*item);
  SetLocation(*item, *loc);
  std::vector<ItemId>& list = *MutableChildren(*loc);
  list.insert(list.begin() + static_cast<ptrdiff_t>(std::min(index, list.size())),
              id);
  if (old_parent != loc->parent) {
    DissolveSplitIfTooSmall(old_parent);
  }
  return true;
}

bool SidebarModel::Remove(ItemId id) {
  const Item* item = GetItem(id);
  if (!item) {
    return false;
  }
  ItemId parent = item->location.parent;
  Detach(*item);
  EraseTree(id);
  DissolveSplitIfTooSmall(parent);
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
  std::vector<ItemId> closed_tabs;
  std::vector<ItemId> closed_roots;
  for (Space& space : spaces_) {
    std::erase_if(space.today, [&](ItemId id) {
      const Item& item = items_.at(id);
      std::vector<ItemId> tabs =
          item.kind == ItemKind::kSplit ? item.children
                                        : std::vector<ItemId>{id};
      for (ItemId tab : tabs) {
        if (keep.contains(tab) ||
            now - items_.at(tab).last_active <= max_idle_ms) {
          return false;
        }
      }
      for (ItemId tab : tabs) {
        const Item& t = items_.at(tab);
        archive_.insert(archive_.begin(),
                        ArchivedTab{space.id, t.title, t.url, now});
        closed_tabs.push_back(tab);
      }
      closed_roots.push_back(id);
      return true;
    });
  }
  for (ItemId id : closed_roots) {
    EraseTree(id);
  }
  return closed_tabs;
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
  if (where.parent != kNoItem) {
    const Item* parent = GetItem(where.parent);
    return parent && parent->kind != ItemKind::kTab ? &parent->children
                                                    : nullptr;
  }
  if (where.section == Section::kFavorites) {
    return &favorites_;
  }
  const Space* space = GetSpace(where.space);
  if (!space) {
    return nullptr;
  }
  return where.section == Section::kPinned ? &space->pinned : &space->today;
}

std::optional<Location> SidebarModel::Resolve(Location where,
                                              ItemKind kind,
                                              ItemId moving) const {
  if (where.parent != kNoItem) {
    const Item* parent = GetItem(where.parent);
    if (!parent || parent->kind == ItemKind::kTab) {
      return std::nullopt;
    }
    if (parent->kind == ItemKind::kSplit) {
      if (kind != ItemKind::kTab) {
        return std::nullopt;
      }
      const Item* item = GetItem(moving);
      bool already_inside = item && item->location.parent == where.parent;
      if (!already_inside && parent->children.size() >= kMaxSplitTabs) {
        return std::nullopt;
      }
    }
    return Location{parent->location.section, parent->location.space,
                    where.parent};
  }
  if (kind == ItemKind::kFolder && where.section != Section::kPinned) {
    return std::nullopt;
  }
  if (where.section == Section::kFavorites) {
    if (kind != ItemKind::kTab) {
      return std::nullopt;
    }
    return Location{Section::kFavorites, kNoSpace, kNoItem};
  }
  if (!GetSpace(where.space)) {
    return std::nullopt;
  }
  return Location{where.section, where.space, kNoItem};
}

std::vector<ItemId>* SidebarModel::MutableChildren(const Location& where) {
  return const_cast<std::vector<ItemId>*>(Children(where));
}

Item* SidebarModel::MutableItem(ItemId id) {
  return const_cast<Item*>(GetItem(id));
}

Space* SidebarModel::MutableSpace(SpaceId id) {
  return const_cast<Space*>(GetSpace(id));
}

bool SidebarModel::IsInside(ItemId id, ItemId ancestor) const {
  for (const Item* item = GetItem(id);
       item && item->location.parent != kNoItem;
       item = GetItem(item->location.parent)) {
    if (item->location.parent == ancestor) {
      return true;
    }
  }
  return false;
}

void SidebarModel::SetLocation(Item& item, const Location& where) {
  if (item.kind == ItemKind::kTab) {
    if (where.section == Section::kToday) {
      item.pinned_url.clear();
    } else if (item.location.section == Section::kToday) {
      item.pinned_url = item.url;
    }
  }
  item.location = where;
  for (ItemId child : item.children) {
    SetLocation(items_.at(child), {where.section, where.space, item.id});
  }
}

void SidebarModel::Detach(const Item& item) {
  if (std::vector<ItemId>* list = MutableChildren(item.location)) {
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

void SidebarModel::DissolveSplitIfTooSmall(ItemId id) {
  Item* split = MutableItem(id);
  if (!split || split->kind != ItemKind::kSplit ||
      split->children.size() >= 2) {
    return;
  }
  Location where = split->location;
  std::vector<ItemId> children = split->children;
  std::vector<ItemId>& list = *MutableChildren(where);
  auto pos = std::find(list.begin(), list.end(), id);
  if (children.empty()) {
    list.erase(pos);
  } else {
    *pos = children[0];  // The last tab takes the split's place.
    SetLocation(items_.at(children[0]), where);
  }
  items_.erase(id);
}

ItemId SidebarModel::Insert(Item item, size_t index) {
  item.id = next_id_++;
  ItemId id = item.id;
  Location loc = item.location;
  items_.emplace(id, std::move(item));
  std::vector<ItemId>& list = *MutableChildren(loc);
  list.insert(list.begin() + static_cast<ptrdiff_t>(std::min(index, list.size())),
              id);
  return id;
}

}  // namespace sidebar
