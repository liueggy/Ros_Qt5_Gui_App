#include "widgets/ui_style.h"

namespace UiStyle {

// ── Legacy compatibility shims ─────────────

int FontBasePx()  { return Font::Base; }
int FontSmallPx() { return Font::Small; }
int FontMiniPx()  { return Font::Mini; }
int FontTitlePx() { return Font::Title; }
int ControlHeightPx() { return Control::Height; }

// ── Application Style Sheet ────────────────

QString ApplicationStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QWidget { color:%1; font-size:%2px; }"
    "QMainWindow, QDialog { background:%3; }"
    "QToolTip { color:%1; background:%4; border:1px solid %5; border-radius:%6px; padding:6px %8px; }"
    "QMenu { background:%4; border:1px solid %5; border-radius:%7px; padding:6px; }"
    "QMenu::item { padding:%8px 28px %8px %9px; border-radius:%6px; }"
    "QMenu::item:selected { color:%10; background:%11; }"
    "QMenu::separator { height:1px; background:#e8edf5; margin:5px %8px; }"
    "QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; "
    "width:34px; border:none; border-left:1px solid #e4e9f1; }"
    "QComboBox::down-arrow { image:url(:/icons/tabler/arrow-down.svg); width:14px; height:14px; }"
    "QComboBox QAbstractItemView { background:%4; color:%1; border:1px solid #cfd8e6; "
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
    "QTabWidget::pane { border:1px solid %5; border-radius:10px; background:%4; top:-1px; }"
    "QTabBar::tab { min-height:30px; padding:6px 16px; color:%2; background:transparent; border:none; }"
    "QTabBar::tab:hover { color:%10; background:%11; }"
    "QTabBar::tab:selected { color:%10; font-weight:600; border-bottom:2px solid %14; }"
    "ads--CDockAreaTitleBar, ads--CDockAreaWidget, ads--CDockContainerWidget { background:%3; }"
    "ads--CDockWidgetTab { background:#f1f5fb; color:%2; border:none; border-bottom:1px solid %5; padding:4px 10px; }"
    "ads--CDockWidgetTab[activeTab=\"true\"] { background:%11; color:%10; border-bottom:2px solid %14; font-weight:700; }"
    "ads--CDockWidgetTab QLabel { background:transparent; color:inherit; }"
    "ads--CTitleBarButton { background:transparent; border:none; border-radius:5px; padding:2px; }"
    "ads--CTitleBarButton:hover { background:%11; }"
    "QSplitter::handle { background:%5; }"
    "QSplitter::handle:hover { background:%14; }"
  ).arg(Text, QString::number(Font::Base), Background, Surface, Border,
        QString::number(Radius::SM), QString::number(Radius::LG),
        QString::number(Space::SM),
        QString::number(Space::MD), Primary, PrimaryLight,
        Scrollbar, ScrollbarHvr, BorderHover);
}

// ── Panel ───────────────────────────────────

QString PanelStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QWidget { color:%1; font-size:%2px; background:transparent; }"
    "QScrollArea { border:none; background:transparent; }"
    "QGroupBox { border:1px solid %3; border-radius:14px; margin-top:0; "
    "padding:14px; font-weight:600; color:%1; background:%4; }"
    "QGroupBox::title { subcontrol-origin:padding; subcontrol-position:top left; "
    "padding:0; color:%1; background:transparent; }"
    "QLabel#pageTitle { font-size:%5px; font-weight:700; color:%1; }"
    "QLabel#pageSubtitle { font-size:%6px; color:%7; }"
    "QPlainTextEdit { border:1px solid #cfd8e6; border-radius:10px; background:%8; padding:%9px; "
    "font-family:%10; font-size:%6px; }"
    "QTreeWidget { border:1px solid #cfd8e6; border-radius:10px; background:%4; alternate-background-color:%8; }"
    "QHeaderView::section { background:#f0f4f9; color:%2; border:none; padding:6px %9px; font-weight:600; }"
  ).arg(Text, TextSecondary, Border, Surface,
        QString::number(Font::Title), QString::number(Font::Small),
        TextMuted, SurfaceHover,
        QString::number(Space::SM), Font::Mono);
}

// ── Cards ───────────────────────────────────

QString CardStyleSheet() {
  using namespace Palette;
  return QStringLiteral(
    "QFrame { background:%1; border:1px solid %2; border-radius:%3px; }"
    "QFrame:hover { border-color:%4; background:%5; }"
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
    "QTableWidget, QTableView, QTreeWidget { border:1px solid #cfd8e6; border-radius:10px; "
    "background:%1; gridline-color:#edf0f4; selection-background-color:%2; "
    "selection-color:%3; alternate-background-color:%4; outline:none; }"
    "QTableWidget::item, QTableView::item, QTreeWidget::item { padding:5px %5px; border:none; }"
    "QTableWidget::item:selected, QTableView::item:selected, QTreeWidget::item:selected "
    "{ color:%3; background:%2; }"
    "QHeaderView::section { background:#f0f4f9; color:%6; border:none; "
    "padding:7px %5px; font-weight:600; }"
  ).arg(Surface, PrimaryLight, Primary, SurfaceHover,
        QString::number(Space::SM), TextSecondary);
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
