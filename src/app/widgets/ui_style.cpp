#include "widgets/ui_style.h"

#include <QAbstractButton>
#include <QColor>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPair>
#include <QPalette>
#include <QPixmap>
#include <QSet>
#include <QStringList>
#include <QStyle>
#include <QVector>
#include <QWidget>

namespace UiStyle {

namespace {
bool g_dark_theme = false;

QStringList ThemeTokens(bool dark) {
  if (dark) {
    return {QStringLiteral("#3fb5a6"), QStringLiteral("#58c7b8"),
            QStringLiteral("#2d9588"), QStringLiteral("#173b37"),
            QStringLiteral("#171b1a"), QStringLiteral("#1d2321"),
            QStringLiteral("#252d2a"), QStringLiteral("#202725"),
            QStringLiteral("#e7ece9"), QStringLiteral("#bac5c0"),
            QStringLiteral("#8d9b95"), QStringLiteral("#071c19"),
            QStringLiteral("#39433f"), QStringLiteral("#55746d"),
            QStringLiteral("#63c69f"), QStringLiteral("#17362c"),
            QStringLiteral("#356d59"), QStringLiteral("#ee817a"),
            QStringLiteral("#402321"), QStringLiteral("#78433f"),
            QStringLiteral("#ff948d"), QStringLiteral("#e4a34e"),
            QStringLiteral("#3b2e1b"), QStringLiteral("#76572f"),
            QStringLiteral("#74b9c1"), QStringLiteral("#203438"),
            QStringLiteral("#3d6870"), QStringLiteral("#1a201e"),
            QStringLiteral("#46514d"), QStringLiteral("#65736d"),
            QStringLiteral("#6f7a75"), QStringLiteral("#242a28"),
            QStringLiteral("#111715"), QStringLiteral("#34413d"),
            QStringLiteral("#dce7e2"), QStringLiteral("#285f57")};
  }
  return {QStringLiteral("#0f766e"), QStringLiteral("#0b5f59"),
          QStringLiteral("#084c47"), QStringLiteral("#e5f4f1"),
          QStringLiteral("#f2f3f1"), QStringLiteral("#fbfcfa"),
          QStringLiteral("#f0f3f0"), QStringLiteral("#f6f7f5"),
          QStringLiteral("#1b2422"), QStringLiteral("#45524f"),
          QStringLiteral("#687572"), QStringLiteral("#ffffff"),
          QStringLiteral("#d3d8d5"), QStringLiteral("#86aaa4"),
          QStringLiteral("#18775f"), QStringLiteral("#e4f3ed"),
          QStringLiteral("#91c4b5"), QStringLiteral("#b83a3a"),
          QStringLiteral("#faecea"), QStringLiteral("#dea29d"),
          QStringLiteral("#962f2f"), QStringLiteral("#b76512"),
          QStringLiteral("#fbf0df"), QStringLiteral("#d8ad70"),
          QStringLiteral("#356b73"), QStringLiteral("#e9f1f2"),
          QStringLiteral("#9cbfc3"), QStringLiteral("#f8f9f7"),
          QStringLiteral("#b9c1bd"), QStringLiteral("#87938e"),
          QStringLiteral("#99a29e"), QStringLiteral("#eceeec"),
          QStringLiteral("#1b2221"), QStringLiteral("#35413e"),
          QStringLiteral("#e2e9e6"), QStringLiteral("#315f58")};
}

QColor TranslateThemeColor(const QColor& color, bool from_dark, bool to_dark) {
  const QStringList from = ThemeTokens(from_dark);
  const QStringList to = ThemeTokens(to_dark);
  const QString source = color.name(QColor::HexRgb);
  for (int i = 0; i < from.size(); ++i) {
    if (source.compare(from.at(i), Qt::CaseInsensitive) == 0) {
      QColor translated(to.at(i));
      translated.setAlpha(color.alpha());
      return translated;
    }
  }
  return color;
}

QPixmap TranslateTintedPixmap(const QPixmap& pixmap, bool from_dark,
                              bool to_dark, bool* changed) {
  if (changed) *changed = false;
  if (pixmap.isNull()) return pixmap;
  const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
  QColor source;
  bool found = false;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const QColor pixel = image.pixelColor(x, y);
      if (pixel.alpha() == 0) continue;
      if (!found) {
        source = pixel;
        source.setAlpha(255);
        found = true;
      } else if (pixel.red() != source.red() ||
                 pixel.green() != source.green() ||
                 pixel.blue() != source.blue()) {
        return pixmap;
      }
    }
  }
  if (!found) return pixmap;
  const QColor target = TranslateThemeColor(source, from_dark, to_dark);
  if (target.rgb() == source.rgb()) return pixmap;

  QImage translated = image;
  QPainter painter(&translated);
  painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
  painter.fillRect(translated.rect(), target);
  painter.end();
  if (changed) *changed = true;
  return QPixmap::fromImage(translated);
}

void TranslateTintedAssets(QWidget* widget, bool from_dark, bool to_dark) {
  if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
    const QSize size = button->iconSize().isValid() ? button->iconSize()
                                                    : QSize(24, 24);
    bool changed = false;
    const QPixmap translated = TranslateTintedPixmap(
        button->icon().pixmap(size), from_dark, to_dark, &changed);
    if (changed) button->setIcon(QIcon(translated));
  }
  if (auto* label = qobject_cast<QLabel*>(widget)) {
    const QPixmap* current = label->pixmap();
    if (current) {
      bool changed = false;
      const QPixmap translated =
          TranslateTintedPixmap(*current, from_dark, to_dark, &changed);
      if (changed) label->setPixmap(translated);
    }
  }
  if (auto* list = qobject_cast<QListWidget*>(widget)) {
    const QSize size = list->iconSize().isValid() ? list->iconSize()
                                                  : QSize(24, 24);
    for (int i = 0; i < list->count(); ++i) {
      QListWidgetItem* item = list->item(i);
      bool changed = false;
      const QPixmap translated = TranslateTintedPixmap(
          item->icon().pixmap(size), from_dark, to_dark, &changed);
      if (changed) item->setIcon(QIcon(translated));
    }
  }
}
}  // namespace

void SetDarkTheme(bool dark) {
  g_dark_theme = dark;
  using namespace Palette;
  const QStringList tokens = ThemeTokens(dark);
  int index = 0;
  Primary = tokens.at(index++);
  PrimaryHover = tokens.at(index++);
  PrimaryPress = tokens.at(index++);
  PrimaryLight = tokens.at(index++);
  Background = tokens.at(index++);
  Surface = tokens.at(index++);
  SurfaceHover = tokens.at(index++);
  SurfaceAlt = tokens.at(index++);
  Text = tokens.at(index++);
  TextSecondary = tokens.at(index++);
  TextMuted = tokens.at(index++);
  TextOnPrimary = tokens.at(index++);
  Border = tokens.at(index++);
  BorderHover = tokens.at(index++);
  BorderFocus = Primary;
  Success = tokens.at(index++);
  SuccessBg = tokens.at(index++);
  SuccessBorder = tokens.at(index++);
  Danger = tokens.at(index++);
  DangerBg = tokens.at(index++);
  DangerBorder = tokens.at(index++);
  DangerHover = tokens.at(index++);
  Warning = tokens.at(index++);
  WarningBg = tokens.at(index++);
  WarningBorder = tokens.at(index++);
  Info = tokens.at(index++);
  InfoBg = tokens.at(index++);
  InfoBorder = tokens.at(index++);
  ToolbarBg = tokens.at(index++);
  Separator = Border;
  Scrollbar = tokens.at(index++);
  ScrollbarHvr = tokens.at(index++);
  DisabledText = tokens.at(index++);
  DisabledBg = tokens.at(index++);
  TerminalBg = tokens.at(index++);
  TerminalBorder = tokens.at(index++);
  TerminalText = tokens.at(index++);
  TerminalSelection = tokens.at(index++);
  Q_ASSERT(index == tokens.size());
}

bool IsDarkTheme() { return g_dark_theme; }

QString TranslateStyleSheetTheme(const QString& style_sheet, bool from_dark,
                                 bool to_dark) {
  if (style_sheet.isEmpty() || from_dark == to_dark) {
    return style_sheet;
  }
  const QStringList from = ThemeTokens(from_dark);
  const QStringList to = ThemeTokens(to_dark);
  QString translated = style_sheet;
  QSet<QString> replaced;
  QVector<QPair<QString, QString>> placeholders;
  for (int i = 0; i < from.size(); ++i) {
    const QString key = from.at(i).toLower();
    if (replaced.contains(key)) {
      continue;
    }
    replaced.insert(key);
    const QString placeholder =
        QStringLiteral("__qt_theme_token_%1__").arg(placeholders.size());
    translated.replace(from.at(i), placeholder, Qt::CaseInsensitive);
    placeholders.append(qMakePair(placeholder, to.at(i)));
  }
  for (const auto& replacement : placeholders) {
    translated.replace(replacement.first, replacement.second);
  }
  return translated;
}

void ApplyApplicationTheme(QApplication* app, QWidget* root, bool dark) {
  if (!app) {
    return;
  }
  const bool previous_dark = IsDarkTheme();
  QList<QPair<QWidget*, QString>> local_styles;
  if (root && previous_dark != dark) {
    QList<QWidget*> widgets{root};
    widgets.append(root->findChildren<QWidget*>());
    for (QWidget* widget : widgets) {
      if (widget && !widget->styleSheet().isEmpty()) {
        local_styles.append(qMakePair(widget, widget->styleSheet()));
      }
    }
  }

  SetDarkTheme(dark);
  QPalette palette;
  palette.setColor(QPalette::Window, QColor(Palette::Background));
  palette.setColor(QPalette::WindowText, QColor(Palette::Text));
  palette.setColor(QPalette::Base, QColor(Palette::Surface));
  palette.setColor(QPalette::AlternateBase, QColor(Palette::SurfaceAlt));
  palette.setColor(QPalette::Text, QColor(Palette::Text));
  palette.setColor(QPalette::Button, QColor(Palette::Surface));
  palette.setColor(QPalette::ButtonText, QColor(Palette::Text));
  palette.setColor(QPalette::Highlight, QColor(Palette::Primary));
  palette.setColor(QPalette::HighlightedText, QColor(Palette::TextOnPrimary));
  palette.setColor(QPalette::Disabled, QPalette::Text,
                   QColor(Palette::DisabledText));
  palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                   QColor(Palette::DisabledText));
  palette.setColor(QPalette::Disabled, QPalette::Base,
                   QColor(Palette::DisabledBg));
  app->setPalette(palette);
  app->setStyleSheet(ApplicationStyleSheet());

  for (const auto& entry : local_styles) {
    entry.first->setStyleSheet(
        TranslateStyleSheetTheme(entry.second, previous_dark, dark));
  }
  if (root) {
    QList<QWidget*> widgets{root};
    widgets.append(root->findChildren<QWidget*>());
    for (QWidget* widget : widgets) {
      if (!widget) continue;
      TranslateTintedAssets(widget, previous_dark, dark);
      widget->style()->unpolish(widget);
      widget->style()->polish(widget);
      widget->update();
    }
  }
}

QIcon TintedIcon(const QString& resource_path, const QSize& size,
                 const QString& color) {
  const QPixmap source = QIcon(resource_path).pixmap(size);
  if (source.isNull()) {
    return QIcon(resource_path);
  }
  QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
  QPainter painter(&image);
  painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
  painter.fillRect(image.rect(), QColor(color));
  painter.end();
  return QIcon(QPixmap::fromImage(image));
}

// ── Legacy compatibility shims ─────────────

int FontBasePx() { return Font::Base; }
int FontSmallPx() { return Font::Small; }
int FontMiniPx() { return Font::Mini; }
int FontTitlePx() { return Font::Title; }
int ControlHeightPx() { return Control::Height; }

// ── Application Style Sheet ────────────────

QString ApplicationStyleSheet() {
  using namespace Palette;
  const QString dock_tab = IsDarkTheme() ? SurfaceAlt : QStringLiteral("#eef1ee");
  return QStringLiteral(
             "QWidget { color:%1; font-size:%2px; }"
             "QMainWindow, QDialog { background:%3; }"
             "QToolTip { color:%1; background:%4; border:1px solid %5; border-radius:%6px; padding:6px %8px; }"
             "QMenu { background:%4; border:1px solid %5; border-radius:%7px; padding:6px; }"
             "QMenu::item { padding:%8px 28px %8px %9px; border-radius:%6px; }"
             "QMenu::item:selected { color:%10; background:%11; }"
             "QMenu::separator { height:1px; background:%5; margin:5px %8px; }"
             "QComboBox { color:%1; background:%4; border:1px solid %5; border-radius:%6px;"
             " padding:5px 36px 5px %9px; min-height:24px; }"
             "QComboBox:hover { border-color:%14; background:%4; }"
             "QComboBox:focus { border-color:%10; }"
             "QComboBox:disabled { color:%16; background:%17; border-color:%5; }"
             "QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; "
             "width:34px; border:none; border-left:1px solid %5; }"
             "QComboBox::down-arrow { image:url(:/icons/tabler/arrow-down.svg); width:14px; height:14px; }"
             "QComboBox QAbstractItemView { background:%4; color:%1; border:1px solid %5; "
             "border-radius:%8px; padding:6px; outline:none; selection-background-color:%11; "
             "selection-color:%10; }"
             "QComboBox QAbstractItemView::item { min-height:34px; padding:5px 10px; }"
             "QScrollBar:vertical { width:10px; margin:2px; border:none; background:transparent; }"
             "QScrollBar::handle:vertical { min-height:32px; border-radius:4px; background:%12; }"
             "QScrollBar::handle:vertical:hover { background:%13; }"
             "QScrollBar:horizontal { height:10px; margin:2px; border:none; background:transparent; }"
             "QScrollBar::handle:horizontal { min-width:32px; border-radius:4px; background:%12; }"
             "QScrollBar::handle:horizontal:hover { background:%13; }"
             "QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }"
             "QTabWidget::pane { border:1px solid %5; border-radius:5px; background:%4; top:-1px; }"
             "QTabBar::tab { min-height:30px; padding:6px 16px; color:%18; background:transparent; border:none; }"
             "QTabBar::tab:hover { color:%10; background:%11; }"
             "QTabBar::tab:selected { color:%10; font-weight:600; border-bottom:2px solid %14; }"
             "ads--CDockAreaTitleBar, ads--CDockAreaWidget, ads--CDockContainerWidget { background:%3; }"
             "ads--CDockWidgetTab { background:%15; color:%18; border:none; border-bottom:1px solid %5; padding:4px 10px; }"
             "ads--CDockWidgetTab[activeTab=\"true\"] { background:%11; color:%10; border-bottom:2px solid %14; font-weight:700; }"
             "ads--CDockWidgetTab QLabel { background:transparent; color:inherit; }"
             "ads--CTitleBarButton { background:transparent; border:none; border-radius:5px; padding:2px; }"
             "ads--CTitleBarButton:hover { background:%11; }"
             "QSplitter::handle { background:%5; }"
             "QSplitter::handle:hover { background:%14; }")
      .arg(Text, QString::number(Font::Base), Background, Surface, Border,
           QString::number(Radius::SM), QString::number(Radius::LG),
           QString::number(Space::SM),
           QString::number(Space::MD), Primary, PrimaryLight,
           Scrollbar, ScrollbarHvr, BorderHover, dock_tab, DisabledText,
           DisabledBg, TextSecondary);
}

QString DockStyleSheet() {
  using namespace Palette;
  const QString dock_tab = IsDarkTheme() ? SurfaceAlt : QStringLiteral("#eef1ee");
  return QStringLiteral(
      "ads--CDockManager, ads--CDockContainerWidget, ads--CDockAreaWidget,"
      " ads--CDockAreaTitleBar, ads--CDockAreaTabBar, ads--CDockWidget {"
      " background:%1; color:%2; border-color:%3; }"
      "ads--CDockAreaTitleBar { border-bottom:1px solid %3; }"
      "ads--CDockWidgetTab { background:%4; color:%5; border:none;"
      " border-bottom:1px solid %3; padding:4px 10px; }"
      "ads--CDockWidgetTab[activeTab=\"true\"] { background:%6; color:%7;"
      " border-bottom:2px solid %8; font-weight:700; }"
      "ads--CDockWidgetTab QLabel { background:transparent; color:inherit; }"
      "ads--CTitleBarButton { background:transparent; border:none;"
      " border-radius:3px; padding:2px; color:%2; }"
      "ads--CTitleBarButton:hover { background:%6; }")
      .arg(Background, Text, Border, dock_tab, TextSecondary,
           PrimaryLight, Primary, BorderHover);
}

// ── Panel ───────────────────────────────────

QString PanelStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QWidget { color:%1; font-size:%2px; background:transparent; }"
    "QScrollArea { border:none; background:transparent; }"
    "QGroupBox { border:1px solid %3; border-radius:5px; margin-top:0; "
    "padding:14px; font-weight:600; color:%1; background:%4; }"
    "QGroupBox::title { subcontrol-origin:padding; subcontrol-position:top left; "
    "padding:0; color:%1; background:transparent; }"
    "QLabel#pageTitle { font-size:%5px; font-weight:700; color:%1; }"
    "QLabel#pageSubtitle { font-size:%6px; color:%7; }"
    "QPlainTextEdit { border:1px solid %3; border-radius:5px; background:%8; padding:%9px; "
    "font-family:%10; font-size:%6px; }"
    "QTreeWidget { border:1px solid %3; border-radius:5px; background:%4; alternate-background-color:%8; }"
    "QHeaderView::section { background:%8; color:%2; border:none; padding:6px %9px; font-weight:600; }"
  ).arg(Text, TextSecondary, Border, Surface,
        QString::number(Font::Title), QString::number(Font::Small),
        TextMuted, SurfaceHover,
        QString::number(Space::SM), Font::Mono);
}

// ── Cards ───────────────────────────────────

QString CardStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QFrame[uiCard=\"true\"] { background:%1; border:1px solid %2; border-radius:%3px; }"
    "QFrame[uiCard=\"true\"]:hover { border-color:%4; background:%5; }"
  ).arg(Surface, Border, QString::number(Radius::XL), BorderHover, SurfaceHover);
}

// ── Buttons ─────────────────────────────────

QString MainButtonStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QPushButton { background:%1; color:%2; border:none; border-radius:%3px; "
    "padding:7px 14px; min-height:%4px; font-size:%5px; font-weight:600; }"
    "QPushButton:hover { background:%6; }"
    "QPushButton:pressed { background:%7; }"
    "QPushButton:focus { outline:2px solid %1; outline-offset:2px; }"
    "QPushButton:disabled { background:%8; color:%9; }"
  ).arg(Primary, TextOnPrimary, QString::number(Radius::MD),
        QString::number(Control::Height), QString::number(Font::Base),
        PrimaryHover, PrimaryPress, DisabledBg, DisabledText);
}

QString SecondaryButtonStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:%4px; "
    "padding:7px 14px; min-height:%5px; font-size:%6px; font-weight:600; }"
    "QPushButton:hover { background:%7; border-color:%8; color:%9; }"
    "QPushButton:pressed { background:%10; border-color:%9; }"
    "QPushButton:focus { outline:2px solid %9; outline-offset:2px; }"
    "QPushButton:disabled { background:%11; color:%12; border-color:%3; }"
  ).arg(Surface, Text, Border, QString::number(Radius::MD),
        QString::number(Control::Height), QString::number(Font::Base),
        SurfaceHover, BorderHover, Primary,
        PrimaryLight,
        DisabledBg, DisabledText);
}

QString DangerButtonStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QPushButton { background:%1; color:%2; border:none; border-radius:%3px; "
    "padding:7px 14px; min-height:%4px; font-size:%5px; font-weight:600; }"
    "QPushButton:hover { background:%6; }"
    "QPushButton:pressed { background:%7; }"
    "QPushButton:focus { outline:2px solid %1; outline-offset:2px; }"
    "QPushButton:disabled { background:%8; color:%9; }"
  ).arg(Danger, TextOnPrimary, QString::number(Radius::MD),
        QString::number(Control::Height), QString::number(Font::Base),
        DangerHover, Palette::DangerHover,
        DisabledBg, DisabledText);
}

QString GhostIconButtonStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QToolButton { border:none; border-radius:%1px; background:transparent; padding:5px; }"
    "QToolButton:hover { background:rgba(30,95,217,0.10); }"
    "QToolButton:pressed { background:rgba(30,95,217,0.18); }"
    "QToolButton:focus { outline:2px solid %2; outline-offset:1px; }"
  ).arg(QString::number(Radius::MD), Primary);
}

QString WindowControlButtonStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QPushButton { min-width:28px; max-width:28px; min-height:24px; max-height:24px; "
    "border:1px solid %1; border-radius:%2px; background:%3; color:%4; font-size:%5px; "
    "font-weight:700; padding:0; }"
    "QPushButton:hover { background:%6; border-color:%7; color:%8; }"
    "QPushButton:pressed { background:%9; }"
  ).arg(Border, QString::number(Radius::MD), ToolbarBg, TextSecondary,
        QString::number(Font::Small), PrimaryLight, BorderHover, Primary,
        Palette::PrimaryLight);
}

QString CloseButtonStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QPushButton { min-width:28px; max-width:28px; min-height:24px; max-height:24px; "
    "border:1px solid %1; border-radius:%2px; background:%3; color:%4; font-size:%5px; "
    "font-weight:700; padding:0; }"
    "QPushButton:hover { background:%6; color:%7; border-color:%8; }"
    "QPushButton:pressed { background:%9; }"
  ).arg(Border, QString::number(Radius::MD), ToolbarBg, TextSecondary,
        QString::number(Font::Small), Danger, TextOnPrimary, Danger,
        DangerHover);
}

QString LinkButtonStyleSheet(const QString& color) {
  return QStringLiteral(
    "QPushButton { border:none; background:transparent; color:%1; font-size:%2px; "
    "font-weight:500; padding:4px %3px; }"
    "QPushButton:hover { background:rgba(30,95,217,0.08); border-radius:6px; text-decoration:underline; }"
  ).arg(color, QString::number(Font::Base), QString::number(Space::SM));
}

// ── Tool Buttons ────────────────────────────

QString ToolButtonStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QToolButton { border:1px solid transparent; border-radius:%1px; background:transparent; "
    "color:%2; padding:5px 14px; font-size:%3px; font-weight:700; "
    "min-height:%4px; max-height:%4px; }"
    "QToolButton:hover { background:%5; border-color:%1; color:%6; }"
    "QToolButton:pressed { background:%7; border-color:%8; }"
    "QToolButton:checked { background:%7; border-color:%8; color:%9; }"
    "QToolButton:focus { outline:2px solid %6; outline-offset:2px; }"
    "QToolButton::menu-indicator { subcontrol-position:bottom right; right:6px; bottom:3px; }"
  ).arg(QString::number(Radius::LG),
        Text, QString::number(Font::Base), QString::number(Control::ToolBtnHeight),
        SurfaceHover, Primary, PrimaryLight, BorderHover, PrimaryHover);
}

QString MiniToolButtonStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QToolButton { border:1px solid %9; border-radius:%1px; background:%2; color:%3; "
    "padding:6px; font-size:%4px; font-weight:600; }"
    "QToolButton:hover { background:%5; border-color:%6; color:%7; }"
    "QToolButton:pressed { background:%8; border-color:%7; }"
    "QToolButton:checked { background:%7; color:%9; border-color:%7; }"
    "QToolButton:focus { outline:2px solid %7; outline-offset:1px; }"
  ).arg(QString::number(Radius::MD),
        Surface, Text, QString::number(Font::Mini),
        PrimaryLight, Palette::PrimaryLight, Primary, Palette::PrimaryLight, TextOnPrimary);
}

QString MoveButtonStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:%4px; "
    "min-width:40px; min-height:36px; font-size:16px; font-weight:700; }"
    "QPushButton:hover { background:%5; border-color:%6; color:%7; }"
    "QPushButton:pressed { background:%8; }"
    "QPushButton:focus { outline:2px solid %7; outline-offset:1px; }"
  ).arg(Surface, Text, Border, QString::number(Radius::MD),
        PrimaryLight, BorderHover, Primary, Palette::PrimaryLight);
}

QString EditToolButtonStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QToolButton { border:1px solid %1; border-radius:%2px; background:%3; "
    "color:%4; padding:4px %5px; font-size:%6px; font-weight:600; "
    "min-height:30px; max-height:30px; }"
    "QToolButton:hover { background:%7; border-color:%8; color:%9; }"
    "QToolButton:pressed { background:%10; border-color:%9; }"
    "QToolButton:checked { background:%9; color:%11; border-color:%9; }"
    "QToolButton:focus { outline:2px solid %9; outline-offset:1px; }"
  ).arg(Border, QString::number(Radius::SM),
        Surface, Text, QString::number(Space::MD),
        QString::number(Font::Small),
        PrimaryLight, BorderHover, Primary, Palette::PrimaryLight, TextOnPrimary);
}

// ── Input ───────────────────────────────────

QString InputStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QLineEdit, QSpinBox, QDoubleSpinBox { border:1px solid %1; border-radius:%2px; "
    "background:%3; color:%4; font-size:%5px; padding:6px %6px; min-height:20px; }"
    "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus { "
    "border-color:%7; background:%3; }"
    "QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { "
    "background:%8; color:%9; }"
  ).arg(Border, QString::number(Radius::SM),
        Surface, Text, QString::number(Font::Base),
        QString::number(Space::SM),
        BorderFocus, DisabledBg, DisabledText);
}

QString CheckBoxStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QCheckBox { color:%1; font-size:%2px; font-weight:500; spacing:%3px; }"
    "QCheckBox::indicator { width:16px; height:16px; border:2px solid %4; "
    "border-radius:4px; background:%5; }"
    "QCheckBox::indicator:checked { background:%6; border-color:%6; }"
    "QCheckBox:focus { outline:2px solid %6; outline-offset:2px; }"
  ).arg(Text, QString::number(Font::Base), QString::number(Space::SM),
        Border, Surface, Primary);
}

QString CompactCheckBoxStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QCheckBox { color:%1; font-size:%2px; font-weight:500; spacing:6px; }"
    "QCheckBox::indicator { width:14px; height:14px; border:2px solid %3; "
    "border-radius:3px; background:%4; }"
    "QCheckBox::indicator:checked { background:%5; border-color:%5; }"
  ).arg(Text, QString::number(Font::Small),
        Border, Surface, Primary);
}

QString TransparentStatusInputStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QLineEdit { border:none; background:transparent; color:%1; font-size:%2px; }"
  ).arg(TextSecondary, QString::number(Font::Mini));
}

// ── Labels ─────────────────────────────────

QString MutedLabelStyleSheet() {
  using namespace Palette;
  return QStringLiteral("color:%1;font-size:%2px;").arg(TextMuted, QString::number(Font::Small));
}

QString TitleLabelStyleSheet() {
  using namespace Palette;
  return QStringLiteral("font-size:%1px;font-weight:700;color:%2;").arg(QString::number(Font::Title), Text);
}

QString SectionLabelStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QLabel { color:%1; font-size:%2px; font-weight:600; letter-spacing:0.3px; padding:%3px %4px 6px %4px; }"
  ).arg(TextMuted, QString::number(Font::Small), QString::number(Space::LG), QString::number(Space::XS));
}

QString HintLabelStyleSheet() {
  using namespace Palette;
  return QStringLiteral("color:%1; font-size:%2px;").arg(TextMuted, QString::number(Font::Mini));
}

QString FieldLabelStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QLabel { color:%1; font-size:%2px; font-weight:500; }"
  ).arg(TextSecondary, QString::number(Font::Small));
}

QString CaptionLabelStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QLabel { color:%1; font-size:%2px; font-weight:700; letter-spacing:0.3px; "
    "padding:%3px 0 2px 0; }"
  ).arg(TextMuted, QString::number(Font::Mini), QString::number(Space::SM));
}

// ── Status ──────────────────────────────────

QString TopStatusLabelStyleSheet(const QString& color) {
  return QStringLiteral(
    "QLabel { color:%1; font-size:%2px; font-weight:700; padding:3px 5px; "
    "background:transparent; border:none; }"
  ).arg(color, QString::number(Font::Small));
}

QString VoiceStatusLabelStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QLabel { color:%1; font-size:%2px; font-weight:600; padding:2px 6px; "
    "background:%3; border:1px solid %4; border-radius:%5px; }"
  ).arg(Warning, QString::number(Font::Small), WarningBg, WarningBorder, QString::number(Radius::SM));
}

QString StatusSuccessStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "font-weight:600; color:%1; background:%2; border:1px solid %3; "
    "border-radius:%4px; padding:6px 10px; font-size:%5px;"
  ).arg(Success, SuccessBg, SuccessBorder, QString::number(Radius::SM), QString::number(Font::Small));
}

QString StatusDangerStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "font-weight:600; color:%1; background:%2; border:1px solid %3; "
    "border-radius:%4px; padding:6px 10px; font-size:%5px;"
  ).arg(Danger, DangerBg, DangerBorder, QString::number(Radius::SM), QString::number(Font::Small));
}

QString StatusWarningStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "font-weight:600; color:%1; background:%2; border:1px solid %3; "
    "border-radius:%4px; padding:6px 10px; font-size:%5px;"
  ).arg(Warning, WarningBg, WarningBorder, QString::number(Radius::SM), QString::number(Font::Small));
}

QString StatusInfoStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "font-weight:600; color:%1; background:%2; border:1px solid %3; "
    "border-radius:%4px; padding:6px 10px; font-size:%5px;"
  ).arg(Primary, PrimaryLight, BorderHover, QString::number(Radius::SM), QString::number(Font::Small));
}

// ── Table ───────────────────────────────────

QString TableStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QTableWidget, QTableView, QTreeWidget { border:1px solid %7; border-radius:%8px; "
    "background:%1; gridline-color:%7; selection-background-color:%2; "
    "selection-color:%3; alternate-background-color:%4; outline:none; }"
    "QTableWidget::item, QTableView::item, QTreeWidget::item { padding:5px %5px; border:none; }"
    "QTableWidget::item:selected, QTableView::item:selected, QTreeWidget::item:selected "
    "{ color:%3; background:%2; }"
    "QHeaderView::section { background:%4; color:%6; border:none; "
    "padding:7px %5px; font-weight:600; }"
  ).arg(Surface, PrimaryLight, Primary, SurfaceHover,
        QString::number(Space::SM), TextSecondary, Border,
        QString::number(Radius::LG));
}

// ── Layout chrome ──────────────────────────

QString ToolStripStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QWidget { background-color:%1; border-bottom:1px solid %2; }"
  ).arg(ToolbarBg, Border);
}

QString SeparatorStyleSheet() {
  using namespace Palette;
  return QStringLiteral("QFrame { background-color:%1; }").arg(Separator);
}

QString FocusBorderStyle() {
  using namespace Palette;
  return QStringLiteral("outline:2px solid %1; outline-offset:2px;").arg(Primary);
}

QString ConnectionStatusStyleSheet(bool connected) {
  if (connected) {
    return QStringLiteral("QToolButton { color:%1; background:%2; border:1px solid %3; "
                          "border-radius:11px; padding:8px 12px; text-align:left; "
                          "font-size:%4px; font-weight:700; }")
        .arg(Palette::Success, Palette::SuccessBg, Palette::SuccessBorder)
        .arg(FontSmallPx());
  }
  return QStringLiteral("QToolButton { color:%1; background:%2; border:1px solid %3; "
                        "border-radius:11px; padding:8px 12px; text-align:left; "
                        "font-size:%4px; font-weight:700; }")
      .arg(Palette::TextSecondary, Palette::SurfaceAlt, Palette::Border)
      .arg(FontSmallPx());
}

}  // namespace UiStyle
