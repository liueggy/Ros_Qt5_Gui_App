#include "widgets/ui_style.h"

namespace UiStyle {

int FontBasePx() {
  return 18;
}

int FontSmallPx() {
  return 16;
}

int FontMiniPx() {
  return 14;
}

int FontTitlePx() {
  return 22;
}

int ControlHeightPx() {
  return 44;
}

QString ApplicationStyleSheet() {
  return QStringLiteral(
             "QWidget { color:#18212f; font-size:%1px; }"
             "QMainWindow, QDialog { background:#f4f7fb; }"
             "QToolTip { color:#18212f; background:#ffffff; border:1px solid #cfd8e6; border-radius:6px; padding:6px 9px; }"
             "QMenu { background:#ffffff; border:1px solid #dce4ef; border-radius:9px; padding:6px; }"
             "QMenu::item { padding:8px 28px 8px 12px; border-radius:6px; }"
             "QMenu::item:selected { color:#174ea6; background:#eaf2ff; }"
             "QMenu::separator { height:1px; background:#e8edf5; margin:5px 8px; }"
             "QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; "
             "width:34px; border:none; border-left:1px solid #e4e9f1; }"
             "QComboBox::down-arrow { image:url(:/icons/tabler/arrow-down.svg); width:14px; height:14px; }"
             "QComboBox QAbstractItemView { background:#ffffff; color:#18212f; border:1px solid #cfd8e6; "
             "border-radius:8px; padding:6px; outline:none; selection-background-color:#eaf2ff; "
             "selection-color:#174ea6; }"
             "QComboBox QAbstractItemView::item { min-height:34px; padding:5px 10px; }"
             "QScrollBar:vertical { width:10px; margin:2px; border:none; background:transparent; }"
             "QScrollBar::handle:vertical { min-height:32px; border-radius:4px; background:#c5cfdd; }"
             "QScrollBar::handle:vertical:hover { background:#9eacbf; }"
             "QScrollBar:horizontal { height:10px; margin:2px; border:none; background:transparent; }"
             "QScrollBar::handle:horizontal { min-width:32px; border-radius:4px; background:#c5cfdd; }"
             "QScrollBar::handle:horizontal:hover { background:#9eacbf; }"
             "QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }"
             "QTabWidget::pane { border:1px solid #dce4ef; border-radius:10px; background:#ffffff; top:-1px; }"
             "QTabBar::tab { min-height:30px; padding:6px 16px; color:#526174; background:transparent; border:none; }"
             "QTabBar::tab:hover { color:#2459a9; background:#eef4fd; }"
             "QTabBar::tab:selected { color:#1f5fbf; font-weight:600; border-bottom:2px solid #2f6fed; }"
             "QSplitter::handle { background:#dce4ef; }"
             "QSplitter::handle:hover { background:#8eb5f5; }")
      .arg(FontBasePx());
}
QString PanelStyleSheet() {
  return QStringLiteral(
             "QWidget { color:#18212f; font-size:%1px; background:transparent; }"
             "QScrollArea { border:none; background:transparent; }"
             "QGroupBox { border:1px solid #dce4ef; border-radius:14px; margin-top:0; "
             "padding:14px; font-weight:600; color:#18212f; background:#ffffff; }"
             "QGroupBox::title { subcontrol-origin: padding; subcontrol-position:top left; "
             "padding:0; color:#18212f; background:transparent; }"
             "QLabel#pageTitle { font-size:%2px; font-weight:700; color:#18212f; }"
             "QLabel#pageSubtitle { font-size:%3px; color:#657386; }"
             "QPlainTextEdit { border:1px solid #cfd8e6; border-radius:10px; background:#f8fafd; padding:8px; "
             "font-family:Consolas, Menlo, monospace; font-size:%3px; }"
             "QTreeWidget { border:1px solid #cfd8e6; border-radius:10px; background:#ffffff; alternate-background-color:#f8fafd; }"
             "QHeaderView::section { background:#f0f4f9; color:#435267; border:none; padding:6px 8px; font-weight:600; }")
      .arg(FontBasePx())
      .arg(FontTitlePx())
      .arg(FontSmallPx());
}

QString MainButtonStyleSheet() {
  return QStringLiteral(
             "QPushButton { background:#2f6fed; color:#ffffff; border:none; border-radius:9px; "
             "padding:7px 14px; min-height:%2px; font-size:%1px; font-weight:600; }"
             "QPushButton:hover { background:#245fd8; }"
             "QPushButton:pressed { background:#1e4fac; }"
             "QPushButton:disabled { background:#dfe5ee; color:#8995a6; }")
      .arg(FontBasePx())
      .arg(ControlHeightPx());
}

QString SecondaryButtonStyleSheet() {
  return QStringLiteral(
             "QPushButton { border:1px solid #cfd8e6; border-radius:9px; padding:7px 12px; "
             "min-height:%2px; background:#ffffff; color:#18212f; font-size:%1px; font-weight:500; }"
             "QPushButton:hover { background:#eaf2ff; border-color:#8ab4f8; color:#174ea6; }"
             "QPushButton:pressed { background:#d2e3fc; }"
             "QPushButton:disabled { background:#f1f3f4; color:#9aa0a6; }")
      .arg(FontBasePx())
      .arg(ControlHeightPx());
}

QString DangerButtonStyleSheet() {
  return QStringLiteral(
             "QPushButton { background:#d93025; color:#ffffff; border:none; border-radius:9px; "
             "padding:7px 14px; min-height:%2px; font-size:%1px; font-weight:600; }"
             "QPushButton:hover { background:#b3261e; }"
             "QPushButton:pressed { background:#8c1d18; }")
      .arg(FontBasePx())
      .arg(ControlHeightPx());
}

QString InputStyleSheet() {
  return QStringLiteral(
             "QLineEdit, QComboBox, QDoubleSpinBox, QSpinBox { border:1px solid #cfd8e6; border-radius:9px; "
             "padding:7px 9px; min-height:%2px; background:#ffffff; color:#18212f; font-size:%1px; }"
             "QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus, QSpinBox:focus { "
             "border-color:#2f6fed; background:#ffffff; }"
             "QLineEdit:disabled, QComboBox:disabled, QDoubleSpinBox:disabled, QSpinBox:disabled { "
             "background:#f1f3f4; color:#9aa0a6; }")
      .arg(FontBasePx())
      .arg(ControlHeightPx());
}

QString ChipStyleSheet() {
  return QStringLiteral(
             "QPushButton { padding:3px 10px; border-radius:11px; border:1px solid #cfd8e6; "
             "background:#ffffff; color:#435267; font-size:%1px; }"
             "QPushButton:hover { background:#f0f4f9; }"
             "QPushButton:checked { font-weight:700; background:#eaf2ff; color:#174ea6; border-color:#2f6fed; }")
      .arg(FontSmallPx());
}

QString TableStyleSheet() {
  return QStringLiteral(
      "QTableWidget, QTableView, QTreeWidget { border:1px solid #cfd8e6; border-radius:10px; background:#ffffff; "
      "gridline-color:#edf0f4; selection-background-color:#eaf2ff; selection-color:#174ea6; "
      "alternate-background-color:#f8fafd; outline:none; }"
      "QTableWidget::item, QTableView::item, QTreeWidget::item { padding:5px 8px; border:none; }"
      "QTableWidget::item:selected, QTableView::item:selected, QTreeWidget::item:selected { color:#174ea6; background:#eaf2ff; }"
      "QHeaderView::section { background:#f0f4f9; color:#435267; border:none; padding:7px 8px; font-weight:600; }");
}

QString CardStyleSheet() {
  return QStringLiteral("QFrame { background:#ffffff; border:1px solid #dce4ef; border-radius:14px; }");
}

QString MutedLabelStyleSheet() {
  return QStringLiteral("color:#657386;font-size:%1px;").arg(FontSmallPx());
}

QString TitleLabelStyleSheet() {
  return QStringLiteral("font-size:%1px;font-weight:700;color:#18212f;").arg(FontTitlePx());
}

QString SectionLabelStyleSheet() {
  return QStringLiteral("QLabel { color:#657386; font-size:%1px; font-weight:600; letter-spacing:0.3px; padding:16px 4px 6px 4px; }")
      .arg(FontSmallPx());
}

QString ToolButtonStyleSheet() {
  return QStringLiteral(
             "QToolButton { border:1px solid transparent; border-radius:10px; background:transparent; color:#18212f; "
             "padding:6px 13px; font-size:%1px; font-weight:600; min-height:58px; max-height:58px; }"
             "QToolButton:hover { background:#f3f7ff; border-color:#dbe7fb; color:#174ea6; }"
             "QToolButton:pressed { background:#e7f0ff; border-color:#bcd3fb; }"
             "QToolButton:checked { background:#edf4ff; border-color:#bcd3fb; color:#174ea6; }"
             "QToolButton::menu-indicator { subcontrol-position:bottom right; right:5px; bottom:4px; }")
      .arg(FontBasePx());
}

QString MiniToolButtonStyleSheet() {
  return QStringLiteral(
             "QToolButton { border:1px solid #e3eaf4; border-radius:9px; background:#ffffff; color:#18212f; "
             "padding:6px; font-size:%1px; font-weight:600; }"
             "QToolButton:hover { background:#eaf2ff; border-color:#8ab4f8; color:#174ea6; }"
             "QToolButton:pressed { background:#d2e3fc; border-color:#2f6fed; }"
             "QToolButton:checked { background:#2f6fed; color:#ffffff; border-color:#2f6fed; }")
      .arg(FontMiniPx());
}

QString GhostIconButtonStyleSheet() {
  return QStringLiteral(
      "QToolButton { border:none; border-radius:9px; background:transparent; padding:5px; }"
      "QToolButton:hover { background:rgba(47,111,237,0.10); }"
      "QToolButton:pressed { background:rgba(47,111,237,0.18); }");
}

QString WindowControlButtonStyleSheet() {
  return QStringLiteral(
             "QPushButton { min-width:28px; max-width:28px; min-height:24px; max-height:24px; border:1px solid #cfd8e6; "
             "border-radius:9px; background:#ffffff; color:#18212f; font-size:%1px; font-weight:700; padding:0; }"
             "QPushButton:hover { background:#eaf2ff; border-color:#8ab4f8; color:#174ea6; }"
             "QPushButton:pressed { background:#d2e3fc; }")
      .arg(FontSmallPx());
}

QString TopStatusLabelStyleSheet(const QString& color) {
  return QStringLiteral("QLabel { color:%1; font-size:%2px; font-weight:600; padding:2px; }").arg(color).arg(FontBasePx());
}

QString VoiceStatusLabelStyleSheet() {
  return QStringLiteral(
             "QLabel { color:#ff8f00; font-size:%1px; font-weight:600; padding:2px 6px; "
             "background:#fff8e1; border:1px solid #ffe082; border-radius:6px; }")
      .arg(FontSmallPx());
}

QString CheckBoxStyleSheet() {
  return QStringLiteral(
             "QCheckBox { color:#18212f; font-size:%1px; font-weight:500; spacing:8px; }"
             "QCheckBox::indicator { width:16px; height:16px; border:2px solid #cfd8e6; border-radius:4px; background:#ffffff; }"
             "QCheckBox::indicator:checked { background:#2f6fed; border-color:#2f6fed; }")
      .arg(FontBasePx());
}

QString TransparentStatusInputStyleSheet() {
  return QStringLiteral("QLineEdit { border:none; background:transparent; color:#435267; font-size:%1px; }").arg(FontMiniPx());
}

QString LinkButtonStyleSheet(const QString& color) {
  return QStringLiteral(
             "QPushButton { border:none; background:transparent; color:%1; font-size:%2px; font-weight:500; padding:4px 8px; }"
             "QPushButton:hover { background:rgba(47,111,237,0.08); border-radius:6px; text-decoration:underline; }")
      .arg(color)
      .arg(FontBasePx());
}

}  // namespace UiStyle
