#include <gtest/gtest.h>
#include <QApplication>
#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QPushButton>
#include <QWidget>

#include "widgets/ui_style.h"

TEST(UiThemeContract, RewritesEveryKnownLightTokenWithoutCascading) {
  UiStyle::SetDarkTheme(false);
  const QString light_primary = UiStyle::Palette::Primary;
  const QString light_surface = UiStyle::Palette::Surface;
  const QString source = QStringLiteral(
                             "QWidget { color:%1; background:%2; border:1px solid %1; }")
                             .arg(light_primary, light_surface);

  const QString dark = UiStyle::TranslateStyleSheetTheme(source, false, true);
  UiStyle::SetDarkTheme(true);

  EXPECT_TRUE(dark.contains(UiStyle::Palette::Primary));
  EXPECT_TRUE(dark.contains(UiStyle::Palette::Surface));
  EXPECT_FALSE(dark.contains(light_primary));
  EXPECT_FALSE(dark.contains(light_surface));
}

TEST(UiThemeContract, RoundTripPreservesStyleSheet) {
  UiStyle::SetDarkTheme(false);
  const QString original = QStringLiteral(
                               "QLabel { color:%1; background:%2; border-color:%3; }")
                               .arg(UiStyle::Palette::Text,
                                    UiStyle::Palette::WarningBg,
                                    UiStyle::Palette::Border);

  const QString dark = UiStyle::TranslateStyleSheetTheme(original, false, true);
  const QString restored = UiStyle::TranslateStyleSheetTheme(dark, true, false);

  EXPECT_EQ(restored, original);
}

TEST(UiThemeContract, LeavesNonThemeValuesUntouched) {
  const QString source = QStringLiteral(
      "QWidget { padding:12px; background:rgba(1,2,3,0.5); }");

  EXPECT_EQ(UiStyle::TranslateStyleSheetTheme(source, false, true), source);
}

TEST(UiThemeContract, AppliesDarkThemeToExistingWidgetWithoutRecreation) {
  UiStyle::ApplyApplicationTheme(qApp, nullptr, false);
  QWidget root;
  QPushButton button(&root);
  button.setStyleSheet(QStringLiteral("color:%1;background:%2;")
                           .arg(UiStyle::Palette::Text,
                                UiStyle::Palette::Surface));
  QWidget* original = &root;

  UiStyle::ApplyApplicationTheme(qApp, &root, true);

  EXPECT_EQ(&root, original);
  EXPECT_TRUE(button.styleSheet().contains(UiStyle::Palette::Text));
  EXPECT_TRUE(button.styleSheet().contains(UiStyle::Palette::Surface));
}

TEST(UiThemeContract, RetintsCachedMonochromeButtonIcon) {
  UiStyle::ApplyApplicationTheme(qApp, nullptr, false);
  QWidget root;
  QPushButton button(&root);
  button.setIconSize(QSize(16, 16));
  QPixmap icon(16, 16);
  icon.fill(QColor(UiStyle::Palette::Primary));
  button.setIcon(QIcon(icon));

  UiStyle::ApplyApplicationTheme(qApp, &root, true);

  const QImage result = button.icon().pixmap(16, 16).toImage();
  EXPECT_EQ(result.pixelColor(8, 8).name(), UiStyle::Palette::Primary);
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
