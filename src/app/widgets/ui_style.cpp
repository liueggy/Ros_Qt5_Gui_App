#include "widgets/ui_style.h"

namespace UiStyle {

int FontBasePx() {
  return 13;
}

int FontSmallPx() {
  return 12;
}

int FontMiniPx() {
  return 11;
}

int FontTitlePx() {
  return 18;
}

int ControlHeightPx() {
  return 36;
}

QString PanelStyleSheet() {
  return QStringLiteral(
             "QWidget { color:#202124; font-size:%1px; background:transparent; }"
             "QScrollArea { border:none; background:transparent; }"
             "QGroupBox { border:1px solid rgba(32,33,36,0.10); border-radius:12px; margin-top:12px; "
             "padding:12px; font-weight:600; color:#202124; background:#ffffff; }"
             "QGroupBox::title { subcontrol-origin: margin; left:12px; padding:0 6px; background:#ffffff; }"
             "QLabel#pageTitle { font-size:%2px; font-weight:700; color:#202124; }"
             "QLabel#pageSubtitle { font-size:%3px; color:#5f6368; }"
             "QPlainTextEdit { border:1px solid #dadce0; border-radius:10px; background:#f8fafd; padding:8px; "
             "font-family:Consolas, Menlo, monospace; font-size:%3px; }"
             "QTreeWidget { border:1px solid #dadce0; border-radius:10px; background:#ffffff; alternate-background-color:#f8fafd; }"
             "QHeaderView::section { background:#f1f4f8; color:#3c4043; border:none; padding:6px 8px; font-weight:600; }")
      .arg(FontBasePx())
      .arg(FontTitlePx())
      .arg(FontSmallPx());
}

QString MainButtonStyleSheet() {
  return QStringLiteral(
             "QPushButton { background:#1a73e8; color:#ffffff; border:none; border-radius:8px; "
             "padding:7px 14px; min-height:%2px; font-size:%1px; font-weight:600; }"
             "QPushButton:hover { background:#1765cc; }"
             "QPushButton:pressed { background:#1558b0; }"
             "QPushButton:disabled { background:#dfe3ea; color:#8a9099; }")
      .arg(FontBasePx())
      .arg(ControlHeightPx());
}

QString SecondaryButtonStyleSheet() {
  return QStringLiteral(
             "QPushButton { border:1px solid #dadce0; border-radius:8px; padding:7px 12px; "
             "min-height:%2px; background:#ffffff; color:#202124; font-size:%1px; font-weight:500; }"
             "QPushButton:hover { background:#e8f0fe; border-color:#8ab4f8; color:#174ea6; }"
             "QPushButton:pressed { background:#d2e3fc; }"
             "QPushButton:disabled { background:#f1f3f4; color:#9aa0a6; }")
      .arg(FontBasePx())
      .arg(ControlHeightPx());
}

QString DangerButtonStyleSheet() {
  return QStringLiteral(
             "QPushButton { background:#d93025; color:#ffffff; border:none; border-radius:8px; "
             "padding:7px 14px; min-height:%2px; font-size:%1px; font-weight:600; }"
             "QPushButton:hover { background:#b3261e; }"
             "QPushButton:pressed { background:#8c1d18; }")
      .arg(FontBasePx())
      .arg(ControlHeightPx());
}

QString InputStyleSheet() {
  return QStringLiteral(
             "QLineEdit, QComboBox, QDoubleSpinBox, QSpinBox { border:1px solid #dadce0; border-radius:8px; "
             "padding:7px 9px; min-height:%2px; background:#ffffff; color:#202124; font-size:%1px; }"
             "QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus, QSpinBox:focus { "
             "border-color:#1a73e8; background:#ffffff; }"
             "QLineEdit:disabled, QComboBox:disabled, QDoubleSpinBox:disabled, QSpinBox:disabled { "
             "background:#f1f3f4; color:#9aa0a6; }")
      .arg(FontBasePx())
      .arg(ControlHeightPx());
}

QString ChipStyleSheet() {
  return QStringLiteral(
             "QPushButton { padding:3px 10px; border-radius:11px; border:1px solid #dadce0; "
             "background:#ffffff; color:#3c4043; font-size:%1px; }"
             "QPushButton:hover { background:#f1f4f8; }"
             "QPushButton:checked { font-weight:700; background:#e8f0fe; color:#174ea6; border-color:#1a73e8; }")
      .arg(FontSmallPx());
}

QString TableStyleSheet() {
  return QStringLiteral(
      "QTableWidget, QTableView, QTreeWidget { border:1px solid #dadce0; border-radius:10px; background:#ffffff; "
      "gridline-color:#edf0f4; selection-background-color:#e8f0fe; selection-color:#174ea6; "
      "alternate-background-color:#f8fafd; outline:none; }"
      "QTableWidget::item, QTableView::item, QTreeWidget::item { padding:5px 8px; border:none; }"
      "QTableWidget::item:selected, QTableView::item:selected, QTreeWidget::item:selected { color:#174ea6; background:#e8f0fe; }"
      "QHeaderView::section { background:#f1f4f8; color:#3c4043; border:none; padding:7px 8px; font-weight:600; }");
}

QString CardStyleSheet() {
  return QStringLiteral("QFrame { background:#ffffff; border:1px solid rgba(32,33,36,0.10); border-radius:12px; }");
}

QString MutedLabelStyleSheet() {
  return QStringLiteral("color:#5f6368;font-size:%1px;").arg(FontSmallPx());
}

QString TitleLabelStyleSheet() {
  return QStringLiteral("font-size:%1px;font-weight:700;color:#202124;").arg(FontTitlePx());
}

QString SectionLabelStyleSheet() {
  return QStringLiteral("QLabel { color:#5f6368; font-size:%1px; font-weight:600; letter-spacing:0.3px; padding:16px 4px 6px 4px; }")
      .arg(FontSmallPx());
}

QString ToolButtonStyleSheet() {
  return QStringLiteral(
             "QToolButton { border:1px solid #dadce0; border-radius:8px; background:#ffffff; color:#202124; "
             "padding:8px 14px; font-size:%1px; font-weight:600; min-height:48px; max-height:48px; }"
             "QToolButton:hover { background:#e8f0fe; border-color:#8ab4f8; color:#174ea6; }"
             "QToolButton:pressed { background:#d2e3fc; border-color:#1a73e8; }"
             "QToolButton:checked { background:#1a73e8; color:#ffffff; border-color:#1a73e8; }")
      .arg(FontBasePx());
}

QString MiniToolButtonStyleSheet() {
  return QStringLiteral(
             "QToolButton { border:1px solid #dadce0; border-radius:8px; background:#ffffff; color:#202124; "
             "padding:6px; font-size:%1px; font-weight:600; }"
             "QToolButton:hover { background:#e8f0fe; border-color:#8ab4f8; color:#174ea6; }"
             "QToolButton:pressed { background:#d2e3fc; border-color:#1a73e8; }"
             "QToolButton:checked { background:#1a73e8; color:#ffffff; border-color:#1a73e8; }")
      .arg(FontMiniPx());
}

QString GhostIconButtonStyleSheet() {
  return QStringLiteral(
      "QToolButton { border:none; border-radius:8px; background:transparent; padding:5px; }"
      "QToolButton:hover { background:rgba(26,115,232,0.10); }"
      "QToolButton:pressed { background:rgba(26,115,232,0.18); }");
}

QString WindowControlButtonStyleSheet() {
  return QStringLiteral(
             "QPushButton { min-width:28px; max-width:28px; min-height:24px; max-height:24px; border:1px solid #dadce0; "
             "border-radius:8px; background:#ffffff; color:#202124; font-size:%1px; font-weight:700; padding:0; }"
             "QPushButton:hover { background:#e8f0fe; border-color:#8ab4f8; color:#174ea6; }"
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
             "QCheckBox { color:#202124; font-size:%1px; font-weight:500; spacing:8px; }"
             "QCheckBox::indicator { width:16px; height:16px; border:2px solid #dadce0; border-radius:4px; background:#ffffff; }"
             "QCheckBox::indicator:checked { background:#1a73e8; border-color:#1a73e8; }")
      .arg(FontBasePx());
}

QString TransparentStatusInputStyleSheet() {
  return QStringLiteral("QLineEdit { border:none; background:transparent; color:#3c4043; font-size:%1px; }").arg(FontMiniPx());
}

QString LinkButtonStyleSheet(const QString& color) {
  return QStringLiteral(
             "QPushButton { border:none; background:transparent; color:%1; font-size:%2px; font-weight:500; padding:4px 8px; }"
             "QPushButton:hover { background:rgba(26,115,232,0.08); border-radius:6px; text-decoration:underline; }")
      .arg(color)
      .arg(FontBasePx());
}

}  // namespace UiStyle
