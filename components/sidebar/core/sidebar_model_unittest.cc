/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "browser/components/sidebar/core/sidebar_model.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace sidebar {
namespace {

constexpr TimeMs kHour = 60 * 60 * 1000;

class SidebarModelTest : public testing::Test {
 protected:
  Location Today() const { return {Section::kToday, space_, kNoItem}; }
  Location Pinned() const { return {Section::kPinned, space_, kNoItem}; }
  Location Favorites() const { return {Section::kFavorites, kNoSpace, kNoItem}; }
  Location In(ItemId folder) const { return {Section::kPinned, kNoSpace, folder}; }

  SidebarModel model_;
  SpaceId space_ = model_.AddSpace("Work");
};

TEST_F(SidebarModelTest, NewTabsGoOnTopOfToday) {
  ItemId a = model_.AddTab(Today(), "https://a.com", "A", 0);
  ItemId b = model_.AddTab(Today(), "https://b.com", "B", 0);
  EXPECT_EQ(*model_.Children(Today()), (std::vector<ItemId>{b, a}));
  EXPECT_TRUE(model_.GetItem(a)->pinned_url.empty());
}

TEST_F(SidebarModelTest, PinningRemembersUrlAndResetGoesBack) {
  ItemId tab = model_.AddTab(Today(), "https://mail.com", "Mail", 0);
  ASSERT_TRUE(model_.Move(tab, Pinned(), 0));
  EXPECT_EQ(model_.GetItem(tab)->pinned_url, "https://mail.com");

  model_.Navigate(tab, "https://mail.com/inbox/42", "Message");
  ASSERT_TRUE(model_.ResetToPinnedUrl(tab));
  EXPECT_EQ(model_.GetItem(tab)->url, "https://mail.com");
}

TEST_F(SidebarModelTest, PinnedUrlSurvivesFavoritesAndClearsOnUnpin) {
  ItemId tab = model_.AddTab(Pinned(), "https://a.com", "A", 0);
  model_.Navigate(tab, "https://a.com/x", "X");
  ASSERT_TRUE(model_.Move(tab, Favorites(), 0));
  EXPECT_EQ(model_.GetItem(tab)->pinned_url, "https://a.com");

  ASSERT_TRUE(model_.Move(tab, Today(), 0));
  EXPECT_TRUE(model_.GetItem(tab)->pinned_url.empty());
  EXPECT_FALSE(model_.ResetToPinnedUrl(tab));
}

TEST_F(SidebarModelTest, FoldersOnlyLiveInPinned) {
  EXPECT_EQ(model_.AddFolder(Today(), "F"), kNoItem);
  EXPECT_EQ(model_.AddFolder(Favorites(), "F"), kNoItem);
  ItemId folder = model_.AddFolder(Pinned(), "F");
  ASSERT_NE(folder, kNoItem);
  EXPECT_FALSE(model_.Move(folder, Today(), 0));
  EXPECT_NE(model_.AddFolder(In(folder), "Nested"), kNoItem);
}

TEST_F(SidebarModelTest, FolderCannotMoveIntoItself) {
  ItemId outer = model_.AddFolder(Pinned(), "Outer");
  ItemId inner = model_.AddFolder(In(outer), "Inner");
  EXPECT_FALSE(model_.Move(outer, In(outer), 0));
  EXPECT_FALSE(model_.Move(outer, In(inner), 0));
  EXPECT_TRUE(model_.Move(inner, Pinned(), 0));
}

TEST_F(SidebarModelTest, MovingFolderToAnotherSpaceMovesContents) {
  SpaceId home = model_.AddSpace("Home");
  ItemId folder = model_.AddFolder(Pinned(), "Recipes");
  ItemId sub = model_.AddFolder(In(folder), "Desserts");
  ItemId tab = model_.AddTab(In(sub), "https://cake.com", "Cake", 0);

  ASSERT_TRUE(model_.Move(folder, {Section::kPinned, home, kNoItem}, 0));
  EXPECT_EQ(model_.GetItem(tab)->location.space, home);
  EXPECT_EQ(model_.GetItem(tab)->location.parent, sub);
  EXPECT_TRUE(model_.Children(Pinned())->empty());
}

TEST_F(SidebarModelTest, IndexIsFinalPositionWhenReordering) {
  ItemId a = model_.AddTab(Pinned(), "a", "a", 0, SidebarModel::kEnd);
  ItemId b = model_.AddTab(Pinned(), "b", "b", 0, SidebarModel::kEnd);
  ItemId c = model_.AddTab(Pinned(), "c", "c", 0, SidebarModel::kEnd);
  ASSERT_TRUE(model_.Move(a, Pinned(), 2));
  EXPECT_EQ(*model_.Children(Pinned()), (std::vector<ItemId>{b, c, a}));
  ASSERT_TRUE(model_.Move(a, Pinned(), 99));  // Clamped.
  EXPECT_EQ(*model_.Children(Pinned()), (std::vector<ItemId>{b, c, a}));
}

TEST_F(SidebarModelTest, RemovingFolderRemovesContents) {
  ItemId folder = model_.AddFolder(Pinned(), "F");
  ItemId tab = model_.AddTab(In(folder), "https://a.com", "A", 0);
  ASSERT_TRUE(model_.Remove(folder));
  EXPECT_EQ(model_.GetItem(folder), nullptr);
  EXPECT_EQ(model_.GetItem(tab), nullptr);
  EXPECT_TRUE(model_.Children(Pinned())->empty());
}

TEST_F(SidebarModelTest, ArchivesOnlyIdleTodayTabsNotOnScreen) {
  ItemId old_tab = model_.AddTab(Today(), "https://old.com", "Old", 0);
  ItemId visible = model_.AddTab(Today(), "https://vis.com", "Visible", 0);
  ItemId fresh = model_.AddTab(Today(), "https://new.com", "New", 0);
  ItemId pinned = model_.AddTab(Pinned(), "https://pin.com", "Pin", 0);
  model_.Activate(fresh, 11 * kHour);

  std::vector<ItemId> closed = model_.ArchiveIdle(13 * kHour, 12 * kHour,
                                                  {visible});
  EXPECT_EQ(closed, std::vector<ItemId>{old_tab});
  EXPECT_EQ(model_.GetItem(old_tab), nullptr);
  EXPECT_NE(model_.GetItem(visible), nullptr);
  EXPECT_NE(model_.GetItem(fresh), nullptr);
  EXPECT_NE(model_.GetItem(pinned), nullptr);
  ASSERT_EQ(model_.archive().size(), 1u);
  EXPECT_EQ(model_.archive()[0].url, "https://old.com");
}

TEST_F(SidebarModelTest, RestoreReopensInOriginalSpaceOrFirstSpace) {
  SpaceId home = model_.AddSpace("Home");
  model_.AddTab({Section::kToday, home, kNoItem}, "https://h.com", "H", 0);
  model_.AddTab(Today(), "https://w.com", "W", 0);
  model_.ArchiveIdle(2 * kHour, kHour, {});
  ASSERT_EQ(model_.archive().size(), 2u);

  ItemId restored = model_.Restore(0, 3 * kHour);
  const Item* item = model_.GetItem(restored);
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->location.space, home);
  EXPECT_EQ(item->last_active, 3 * kHour);

  ASSERT_TRUE(model_.RemoveSpace(space_));
  ItemId orphan = model_.Restore(0, 3 * kHour);
  EXPECT_EQ(model_.GetItem(orphan)->location.space, home);
  EXPECT_TRUE(model_.archive().empty());
}

TEST_F(SidebarModelTest, RemoveSpaceDeletesItemsButKeepsLastSpace) {
  SpaceId other = model_.AddSpace("Other");
  ItemId tab = model_.AddTab(Today(), "https://a.com", "A", 0);
  ItemId folder = model_.AddFolder(Pinned(), "F");
  ItemId nested = model_.AddTab(In(folder), "https://b.com", "B", 0);

  ASSERT_TRUE(model_.RemoveSpace(space_));
  EXPECT_EQ(model_.GetItem(tab), nullptr);
  EXPECT_EQ(model_.GetItem(nested), nullptr);
  EXPECT_FALSE(model_.RemoveSpace(other));
}

TEST_F(SidebarModelTest, RejectsUnknownIdsAndLocations) {
  EXPECT_EQ(model_.AddTab({Section::kToday, 999, kNoItem}, "u", "t", 0),
            kNoItem);
  EXPECT_EQ(model_.AddTab(In(999), "u", "t", 0), kNoItem);
  ItemId tab = model_.AddTab(Today(), "u", "t", 0);
  EXPECT_EQ(model_.AddTab(In(tab), "u", "t", 0), kNoItem);  // Not a folder.
  EXPECT_FALSE(model_.Move(999, Today(), 0));
  EXPECT_FALSE(model_.Remove(999));
  EXPECT_FALSE(model_.Activate(999, 0));
  EXPECT_EQ(model_.Restore(0, 0), kNoItem);
  EXPECT_EQ(model_.Children(In(999)), nullptr);
}

TEST_F(SidebarModelTest, SplitTakesFirstTabsPlace) {
  ItemId a = model_.AddTab(Today(), "https://a.com", "A", 0);
  ItemId b = model_.AddTab(Today(), "https://b.com", "B", 0);
  ItemId c = model_.AddTab(Today(), "https://c.com", "C", 0);  // Today: c b a.
  ItemId split = model_.CreateSplit({b, a}, SplitLayout::kStacked);
  ASSERT_NE(split, kNoItem);
  EXPECT_EQ(*model_.Children(Today()), (std::vector<ItemId>{c, split}));
  const Item* item = model_.GetItem(split);
  EXPECT_EQ(item->kind, ItemKind::kSplit);
  EXPECT_EQ(item->layout, SplitLayout::kStacked);
  EXPECT_EQ(item->children, (std::vector<ItemId>{b, a}));
  EXPECT_EQ(model_.GetItem(a)->location.parent, split);
}

TEST_F(SidebarModelTest, SplitRules) {
  std::vector<ItemId> tabs;
  for (int i = 0; i < 5; ++i) {
    tabs.push_back(model_.AddTab(Today(), "u", "t", 0));
  }
  ItemId fav = model_.AddTab(Favorites(), "f", "f", 0);
  ItemId folder = model_.AddFolder(Pinned(), "F");

  EXPECT_EQ(model_.CreateSplit({tabs[0]}, SplitLayout::kSideBySide), kNoItem);
  EXPECT_EQ(model_.CreateSplit(tabs, SplitLayout::kSideBySide), kNoItem);
  EXPECT_EQ(model_.CreateSplit({tabs[0], tabs[0]}, SplitLayout::kSideBySide),
            kNoItem);
  EXPECT_EQ(model_.CreateSplit({tabs[0], folder}, SplitLayout::kSideBySide),
            kNoItem);
  EXPECT_EQ(model_.CreateSplit({fav, tabs[0]}, SplitLayout::kSideBySide),
            kNoItem);

  ItemId split = model_.CreateSplit({tabs[0], tabs[1], tabs[2], tabs[3]},
                                    SplitLayout::kSideBySide);
  ASSERT_NE(split, kNoItem);
  Location inside{Section::kToday, kNoSpace, split};
  EXPECT_FALSE(model_.Move(tabs[4], inside, 0));  // Full.
  EXPECT_EQ(model_.AddTab(inside, "u", "t", 0), kNoItem);
  EXPECT_TRUE(model_.Move(tabs[3], inside, 0));   // Reordering is fine.
  EXPECT_EQ(model_.GetItem(split)->children.front(), tabs[3]);
  EXPECT_FALSE(model_.Move(folder, inside, 0));
  EXPECT_EQ(model_.AddFolder(inside, "F"), kNoItem);
  EXPECT_EQ(model_.CreateSplit({tabs[0], tabs[4]}, SplitLayout::kSideBySide),
            kNoItem);  // tabs[0] is already in a split.

  // Split Views can live in pinned folders, but not in Favorites.
  EXPECT_TRUE(model_.Move(split, In(folder), 0));
  EXPECT_EQ(model_.GetItem(tabs[0])->location.section, Section::kPinned);
  EXPECT_FALSE(model_.Move(split, Favorites(), 0));
}

TEST_F(SidebarModelTest, SplitWithOneTabLeftTurnsBackIntoThatTab) {
  ItemId x = model_.AddTab(Pinned(), "https://x.com", "X", 0);
  ItemId a = model_.AddTab(Pinned(), "https://a.com", "A", 0, SidebarModel::kEnd);
  ItemId b = model_.AddTab(Pinned(), "https://b.com", "B", 0, SidebarModel::kEnd);
  ItemId split = model_.CreateSplit({a, b}, SplitLayout::kSideBySide);
  ASSERT_TRUE(model_.Remove(a));
  EXPECT_EQ(model_.GetItem(split), nullptr);
  EXPECT_EQ(*model_.Children(Pinned()), (std::vector<ItemId>{x, b}));
  EXPECT_EQ(model_.GetItem(b)->location.parent, kNoItem);

  ItemId c = model_.AddTab(Pinned(), "https://c.com", "C", 0, SidebarModel::kEnd);
  split = model_.CreateSplit({b, c}, SplitLayout::kSideBySide);
  ASSERT_TRUE(model_.Move(b, Today(), 0));
  EXPECT_EQ(model_.GetItem(split), nullptr);
  EXPECT_EQ(*model_.Children(Pinned()), (std::vector<ItemId>{x, c}));
}

TEST_F(SidebarModelTest, MovingSplitOutOfTodayPinsItsTabs) {
  ItemId a = model_.AddTab(Today(), "https://a.com", "A", 0);
  ItemId b = model_.AddTab(Today(), "https://b.com", "B", 0);
  ItemId split = model_.CreateSplit({a, b}, SplitLayout::kSideBySide);
  ASSERT_TRUE(model_.Move(split, Pinned(), 0));
  EXPECT_EQ(model_.GetItem(a)->pinned_url, "https://a.com");
  EXPECT_EQ(model_.GetItem(b)->location.section, Section::kPinned);
  ASSERT_TRUE(model_.Move(split, Today(), 0));
  EXPECT_TRUE(model_.GetItem(a)->pinned_url.empty());
}

TEST_F(SidebarModelTest, SplitArchivesOnlyWhenAllTabsAreIdle) {
  ItemId a = model_.AddTab(Today(), "https://a.com", "A", 0);
  ItemId b = model_.AddTab(Today(), "https://b.com", "B", 0);
  ItemId split = model_.CreateSplit({a, b}, SplitLayout::kSideBySide);
  model_.Activate(b, 2 * kHour);
  EXPECT_TRUE(model_.ArchiveIdle(2 * kHour, kHour, {}).empty());

  std::vector<ItemId> closed = model_.ArchiveIdle(4 * kHour, kHour, {});
  EXPECT_EQ(closed, (std::vector<ItemId>{a, b}));
  EXPECT_EQ(model_.GetItem(split), nullptr);
  EXPECT_EQ(model_.GetItem(a), nullptr);
  EXPECT_EQ(model_.archive().size(), 2u);
}

TEST_F(SidebarModelTest, EditSpaceSetsNameIconAndTheme) {
  SpaceTheme theme{{Color{106, 86, 218, 255}}, true, 0.5};
  SpaceIcon icon{SpaceIcon::Kind::kEmoji, "\xF0\x9F\x8C\x8D"};
  ASSERT_TRUE(model_.EditSpace(space_, "Planet", icon, theme));
  const Space* space = model_.GetSpace(space_);
  EXPECT_EQ(space->name, "Planet");
  EXPECT_EQ(space->icon, icon);
  EXPECT_EQ(space->theme, theme);
  EXPECT_FALSE(model_.EditSpace(999, "x", {}, {}));
}

}  // namespace
}  // namespace sidebar
