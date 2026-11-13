/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "browser/components/importer/core/arc_importer.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace importer {
namespace {

using sidebar::ItemId;
using sidebar::ItemKind;
using sidebar::kNoItem;
using sidebar::Location;
using sidebar::Section;
using sidebar::SidebarModel;

constexpr sidebar::TimeMs kNow = 1'700'000'000'000;

// Same shapes as a real StorableSidebar.json, trimmed to what we read.
constexpr char kArcSidebar[] = R"json({
  "version": 1,
  "sidebar": {"containers": [
    {"global": {}},
    {
      "topAppsContainerIDs": [{"default": true}, "favs"],
      "spaces": [
        "s1", {"id": "s1", "title": "Amine The Wiz", "profile": {"default": true},
               "containerIDs": ["unpinned", "s1-today", "pinned", "s1-pinned"]},
        "s2", {"id": "s2", "title": "Medicine", "profile": {"default": true},
               "containerIDs": ["unpinned", "s2-today", "pinned", "s2-pinned"]}
      ],
      "items": [
        "favs", {"id": "favs", "title": null, "parentID": null,
                 "childrenIds": ["yt", "mail"],
                 "data": {"itemContainer": {"containerType": {"topApps": {}}}}},
        "yt", {"id": "yt", "title": null, "childrenIds": [], "parentID": "favs",
               "data": {"tab": {"savedURL": "https://youtube.com", "savedTitle": "YouTube",
                                "timeLastActiveAt": 738434348.7}}},
        "mail", {"id": "mail", "title": "Inbox", "childrenIds": [], "parentID": "favs",
                 "data": {"tab": {"savedURL": "https://mail.google.com/mail/u/1/#inbox",
                                  "savedTitle": "Gmail"}}},

        "s1-pinned", {"id": "s1-pinned", "childrenIds": ["basics"],
                      "data": {"itemContainer": {}}},
        "basics", {"id": "basics", "title": "Arc Basics",
                   "childrenIds": ["welcome", "res", "deep", "split"],
                   "data": {"list": {}}},
        "welcome", {"id": "welcome", "title": "Getting Started", "childrenIds": [],
                    "data": {"welcomeToArc": {"tabType": "legacy"}}},
        "res", {"id": "res", "childrenIds": [],
                "data": {"tab": {"savedURL": "https://resources.arc.net/",
                                 "savedTitle": "Arc Resources"}}},
        "deep", {"id": "deep", "title": "Videos", "childrenIds": ["yt-channel"],
                 "data": {"list": {}}},
        "yt-channel", {"id": "yt-channel", "childrenIds": [],
                       "data": {"tab": {"savedURL": "https://www.youtube.com/@TheBrowserCompany/videos",
                                        "savedTitle": "The Browser Company"}}},
        "split", {"id": "split", "childrenIds": ["substack", "x"],
                  "data": {"splitView": {"layoutOrientation": "horizontal"}}},
        "substack", {"id": "substack", "childrenIds": [],
                     "data": {"tab": {"savedURL": "https://browsercompany.substack.com/archive",
                                      "savedTitle": "Substack"}}},
        "x", {"id": "x", "childrenIds": [],
              "data": {"tab": {"savedURL": "https://twitter.com/arcinternet",
                               "savedTitle": "Arc on X"}}},

        "s1-today", {"id": "s1-today", "childrenIds": ["t1", "t2"],
                     "data": {"itemContainer": {}}},
        "t1", {"id": "t1", "childrenIds": [],
               "data": {"tab": {"savedURL": "https://chat.openai.com/", "savedTitle": "ChatGPT"}}},
        "t2", {"id": "t2", "childrenIds": [],
               "data": {"tab": {"savedURL": "https://notion.com", "savedTitle": "Notion"}}},

        "s2-pinned", {"id": "s2-pinned", "childrenIds": [], "data": {"itemContainer": {}}},
        "s2-today", {"id": "s2-today", "childrenIds": [], "data": {"itemContainer": {}}}
      ]
    }
  ]}
})json";

std::vector<std::string> Urls(const SidebarModel& model,
                              const std::vector<ItemId>& ids) {
  std::vector<std::string> urls;
  for (ItemId id : ids) {
    urls.push_back(model.GetItem(id)->url);
  }
  return urls;
}

TEST(ArcImporterTest, ImportsSpacesFavoritesPinnedAndToday) {
  std::optional<ArcImport> result = ImportArcSidebar(kArcSidebar, kNow);
  ASSERT_TRUE(result);
  ASSERT_EQ(result->profiles.size(), 1u);
  EXPECT_EQ(result->profiles[0].directory, "");
  EXPECT_EQ(result->skipped, 1);  // Arc's welcome page.
  const SidebarModel& model = result->profiles[0].sidebar;

  ASSERT_EQ(model.spaces().size(), 2u);
  EXPECT_EQ(model.spaces()[0].name, "Amine The Wiz");
  EXPECT_EQ(model.spaces()[1].name, "Medicine");
  sidebar::SpaceId wiz = model.spaces()[0].id;

  // Favorites keep order, and a renamed tab keeps its custom title.
  EXPECT_EQ(Urls(model, model.favorites()),
            (std::vector<std::string>{"https://youtube.com",
                                      "https://mail.google.com/mail/u/1/#inbox"}));
  EXPECT_EQ(model.GetItem(model.favorites()[0])->title, "YouTube");
  EXPECT_EQ(model.GetItem(model.favorites()[1])->title, "Inbox");
  EXPECT_EQ(model.GetItem(model.favorites()[0])->pinned_url,
            "https://youtube.com");

  // Pinned: the folder, with the nested folder kept and the Split View
  // flattened into its tabs, in order.
  const auto& pinned = *model.Children({Section::kPinned, wiz, kNoItem});
  ASSERT_EQ(pinned.size(), 1u);
  const sidebar::Item* basics = model.GetItem(pinned[0]);
  EXPECT_EQ(basics->kind, ItemKind::kFolder);
  EXPECT_EQ(basics->title, "Arc Basics");
  ASSERT_EQ(basics->children.size(), 4u);
  const sidebar::Item* videos = model.GetItem(basics->children[1]);
  EXPECT_EQ(videos->kind, ItemKind::kFolder);
  EXPECT_EQ(videos->title, "Videos");
  EXPECT_EQ(Urls(model, videos->children),
            std::vector<std::string>{"https://www.youtube.com/@TheBrowserCompany/videos"});
  EXPECT_EQ(model.GetItem(basics->children[2])->url,
            "https://browsercompany.substack.com/archive");
  EXPECT_EQ(model.GetItem(basics->children[3])->url,
            "https://twitter.com/arcinternet");

  // Today keeps Arc's top-to-bottom order; imported tabs count as active now.
  const auto& today = *model.Children({Section::kToday, wiz, kNoItem});
  EXPECT_EQ(Urls(model, today),
            (std::vector<std::string>{"https://chat.openai.com/",
                                      "https://notion.com"}));
  EXPECT_EQ(model.GetItem(today[0])->last_active, kNow);
  EXPECT_TRUE(model.GetItem(today[0])->pinned_url.empty());
}

TEST(ArcImporterTest, SplitsArcProfilesIntoSeparateSidebars) {
  std::optional<ArcImport> result = ImportArcSidebar(R"({"sidebar": {"containers": [{
    "topAppsContainerIDs": [
      {"default": true}, "fa",
      {"custom": {"_0": {"directoryBasename": "Profile 1"}}}, "fb"],
    "spaces": [
      "w", {"id": "w", "title": "Work",
            "profile": {"custom": {"_0": {"directoryBasename": "Profile 1"}}},
            "containerIDs": []},
      "h", {"id": "h", "title": "Home", "profile": {"default": true}, "containerIDs": []}],
    "items": [
      "fa", {"id": "fa", "childrenIds": ["a"]},
      "fb", {"id": "fb", "childrenIds": ["b"]},
      "a", {"id": "a", "data": {"tab": {"savedURL": "https://a.com"}}},
      "b", {"id": "b", "data": {"tab": {"savedURL": "https://b.com"}}}]
  }]}})", kNow);
  ASSERT_TRUE(result);
  ASSERT_EQ(result->profiles.size(), 2u);
  const ArcProfile& home = result->profiles[0];
  const ArcProfile& work = result->profiles[1];
  EXPECT_EQ(home.directory, "");
  EXPECT_EQ(work.directory, "Profile 1");
  EXPECT_EQ(home.sidebar.spaces()[0].name, "Home");
  EXPECT_EQ(work.sidebar.spaces()[0].name, "Work");
  EXPECT_EQ(Urls(home.sidebar, home.sidebar.favorites()),
            std::vector<std::string>{"https://a.com"});
  EXPECT_EQ(Urls(work.sidebar, work.sidebar.favorites()),
            std::vector<std::string>{"https://b.com"});
}

TEST(ArcImporterTest, SurvivesCyclesAndMissingItems) {
  std::optional<ArcImport> result = ImportArcSidebar(R"({"sidebar": {"containers": [{
    "topAppsContainerIDs": [],
    "spaces": ["s", {"title": "", "containerIDs": ["pinned", "p", "weird", "p"]}],
    "items": [
      "p", {"id": "p", "childrenIds": ["loop", "ghost", 42, "nourl"]},
      "loop", {"id": "loop", "title": "Loop", "childrenIds": ["loop", "p"],
               "data": {"list": {}}},
      "nourl", {"id": "nourl", "data": {"tab": {}}}]
  }]}})", kNow);
  ASSERT_TRUE(result);
  const SidebarModel& model = result->profiles[0].sidebar;
  EXPECT_EQ(model.spaces()[0].name, "Space");
  const auto& pinned =
      *model.Children({Section::kPinned, model.spaces()[0].id, kNoItem});
  ASSERT_EQ(pinned.size(), 1u);
  EXPECT_TRUE(model.GetItem(pinned[0])->children.empty());
  // "ghost" doesn't exist, "nourl" has no URL, and the cycle back to the
  // container "p" is not an importable item.
  EXPECT_EQ(result->skipped, 3);
}

TEST(ArcImporterTest, DecodesJsonEscapes) {
  std::optional<ArcImport> result = ImportArcSidebar(R"({"sidebar": {"containers": [{
    "topAppsContainerIDs": [{"default": true}, "f"],
    "spaces": [],
    "items": ["f", {"id": "f", "childrenIds": ["t"]},
              "t", {"id": "t", "data": {"tab": {
                "savedURL": "https:\/\/a.com\/?q=\"x\"",
                "savedTitle": "Café 😀 \ud800 tab\there"}}}]
  }]}})", kNow);
  ASSERT_TRUE(result);
  const SidebarModel& model = result->profiles[0].sidebar;
  const sidebar::Item* tab = model.GetItem(model.favorites()[0]);
  EXPECT_EQ(tab->url, "https://a.com/?q=\"x\"");
  EXPECT_EQ(tab->title, "Caf\xC3\xA9 \xF0\x9F\x98\x80 \xEF\xBF\xBD tab\there");
}

TEST(ArcImporterTest, RejectsInvalidInput) {
  for (const char* bad : {"", "{", "not json", "[1,]", "{\"a\" 1}", "{} x",
                          "\"unterminated", "\"bad \\q escape\"", "01",
                          "{\"sidebar\": {}}", "{\"sidebar\": {\"containers\": [{}]}}"}) {
    EXPECT_FALSE(ImportArcSidebar(bad, kNow)) << bad;
  }
  // Deep nesting fails cleanly instead of overflowing the stack.
  std::string deep(100000, '[');
  EXPECT_FALSE(ImportArcSidebar(deep, kNow));
}

}  // namespace
}  // namespace importer
