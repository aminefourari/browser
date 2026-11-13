/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BROWSER_COMPONENTS_IMPORTER_CORE_ARC_IMPORTER_H_
#define BROWSER_COMPONENTS_IMPORTER_CORE_ARC_IMPORTER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "browser/components/sidebar/core/sidebar_model.h"

namespace importer {

struct ArcProfile {
  // Arc's profile directory name; empty for Arc's default profile.
  std::string directory;
  sidebar::SidebarModel sidebar;
};

struct ArcImport {
  std::vector<ArcProfile> profiles;
  // Items with no equivalent here yet (Easels, Notes, Arc's own pages…).
  int skipped = 0;
};

// Reads Arc's sidebar file (~/Library/Application Support/Arc/
// StorableSidebar.json) into one sidebar per Arc profile: Spaces, Favorites,
// pinned tabs and folders, and Today (unpinned) tabs. Split Views come in as
// their separate tabs. Imported tabs are marked active at `now` so
// auto-archive doesn't close them right away.
//
// Returns nullopt if `json` is not valid JSON in Arc's format.
std::optional<ArcImport> ImportArcSidebar(std::string_view json,
                                          sidebar::TimeMs now);

}  // namespace importer

#endif  // BROWSER_COMPONENTS_IMPORTER_CORE_ARC_IMPORTER_H_
