#pragma once

#include <QString>

namespace UiTheme {

inline QString ApplicationStyle() {
  return QStringLiteral(R"(
    QMainWindow { background:#f4f7fb; color:#172033; }
    QToolBar, QStatusBar, QMenuBar { background:#ffffff; border:none; }
    QStatusBar { border-top:1px solid #dce3ec; }
    QMenuBar { border-bottom:1px solid #dce3ec; }
    QMenuBar::item, QMenu::item { padding:7px 12px; border-radius:6px; }
    QMenuBar::item:selected, QMenu::item:selected {
      background:#eff6ff; color:#1d4ed8;
    }
    QMenu {
      background:#ffffff; color:#172033; border:1px solid #dce3ec;
      border-radius:8px; padding:5px;
    }
    QToolTip {
      background:#172033; color:#ffffff; border:none;
      border-radius:5px; padding:5px 8px;
    }
    QLineEdit, QComboBox, QPlainTextEdit, QSpinBox, QDoubleSpinBox {
      min-height:30px; padding:3px 9px; color:#172033;
      background:#ffffff; border:1px solid #d7e0eb; border-radius:7px;
      selection-background-color:#2563eb;
    }
    QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus,
    QSpinBox:focus, QDoubleSpinBox:focus { border-color:#2563eb; }
    QCheckBox { color:#334155; spacing:7px; font-weight:500; }
    QCheckBox::indicator {
      width:15px; height:15px; background:#ffffff;
      border:1px solid #cbd5e1; border-radius:4px;
    }
    QCheckBox::indicator:checked {
      background:#2563eb; border-color:#2563eb;
    }
    QTabWidget::pane { border:1px solid #dce3ec; background:#ffffff; }
    QTabBar::tab {
      padding:7px 12px; color:#64748b; background:#f8fafc;
      border-bottom:2px solid transparent;
    }
    QTabBar::tab:selected {
      color:#1d4ed8; background:#ffffff; border-bottom-color:#2563eb;
    }
    QWidget#statusChip {
      background:#f8fafc; border:1px solid #dce3ec; border-radius:10px;
    }
    QWidget#statusChip[statusState="online"] {
      background:#f0fdf4; border-color:#bbf7d0;
    }
    QWidget#statusChip[statusState="warning"] {
      background:#fffbeb; border-color:#fde68a;
    }
    QWidget#statusChip[statusState="offline"] {
      background:#fff1f2; border-color:#fecdd3;
    }
    QWidget#statusChip QLabel#statusText {
      color:#475569; font-size:11px; font-weight:600;
    }
    QWidget#statusChip QLabel#statusIndicator {
      background:#94a3b8; border-radius:3px;
    }
    QWidget#statusChip[statusState="online"] QLabel#statusIndicator { background:#16a34a; }
    QWidget#statusChip[statusState="warning"] QLabel#statusIndicator { background:#d97706; }
    QWidget#statusChip[statusState="offline"] QLabel#statusIndicator { background:#dc2626; }
    QLabel#systemNoticeBanner {
      padding:7px 11px; color:#92400e; background:#fffbeb;
      border:1px solid #fde68a; border-radius:7px;
      font-size:11px; font-weight:600;
    }
  )");
}

inline QString ToolButtonStyle() {
  return QStringLiteral(R"(
    QToolButton {
      min-height:36px; max-height:36px; padding:6px 10px;
      color:#334155; background:#ffffff; border:1px solid #d7e0eb;
      border-radius:7px; font-size:11px; font-weight:600;
    }
    QToolButton:hover { color:#1d4ed8; background:#f8fbff; border-color:#93b4f5; }
    QToolButton:pressed, QToolButton:checked {
      color:#ffffff; background:#2563eb; border-color:#2563eb;
    }
  )");
}

inline QString EditToolButtonStyle() {
  return QStringLiteral(R"(
    QToolButton {
      min-width:34px; min-height:34px; padding:5px;
      color:#334155; background:#ffffff; border:1px solid #d7e0eb;
      border-radius:7px;
    }
    QToolButton:hover { background:#eff6ff; border-color:#93b4f5; }
    QToolButton:checked {
      color:#ffffff; background:#2563eb; border-color:#2563eb;
    }
  )");
}

inline QString PrimaryButtonStyle() {
  return QStringLiteral(R"(
    QPushButton {
      min-height:32px; padding:5px 12px; color:#ffffff;
      background:#2563eb; border:none; border-radius:7px;
      font-size:12px; font-weight:600;
    }
    QPushButton:hover { background:#1d4ed8; }
    QPushButton:pressed { background:#1e40af; }
    QPushButton:disabled { color:#94a3b8; background:#e2e8f0; }
  )");
}

}  // namespace UiTheme
