#pragma once

#include <QString>
#include <QApplication>

// ═══════════════════════════════════════════════
//  UiStyle — centralized design system
//  To change the theme: edit Palette below,
//  then rebuild. All widgets follow automatically.
// ═══════════════════════════════════════════════

namespace UiStyle {

// ── Design Tokens ────────────────────────────

namespace Palette {
  // Primary
  inline QString Primary      = QStringLiteral("#0f766e");
  inline QString PrimaryHover = QStringLiteral("#0b5f59");
  inline QString PrimaryPress = QStringLiteral("#084c47");
  inline QString PrimaryLight = QStringLiteral("#e5f4f1");
  // Surface
  inline QString Background   = QStringLiteral("#f2f3f1");
  inline QString Surface      = QStringLiteral("#fbfcfa");
  inline QString SurfaceHover = QStringLiteral("#f0f3f0");
  inline QString SurfaceAlt   = QStringLiteral("#f6f7f5");
  // Text
  inline QString Text         = QStringLiteral("#1b2422");
  inline QString TextSecondary= QStringLiteral("#45524f");
  inline QString TextMuted    = QStringLiteral("#687572");
  inline QString TextOnPrimary= QStringLiteral("#ffffff");
  // Border
  inline QString Border       = QStringLiteral("#d3d8d5");
  inline QString BorderHover  = QStringLiteral("#86aaa4");
  inline QString BorderFocus  = QStringLiteral("#0f766e");
  // Semantic
  inline QString Success      = QStringLiteral("#18775f");
  inline QString SuccessBg    = QStringLiteral("#e4f3ed");
  inline QString SuccessBorder= QStringLiteral("#91c4b5");
  inline QString Danger       = QStringLiteral("#b83a3a");
  inline QString DangerBg     = QStringLiteral("#faecea");
  inline QString DangerBorder = QStringLiteral("#dea29d");
  inline QString DangerHover  = QStringLiteral("#962f2f");
  inline QString Warning      = QStringLiteral("#b76512");
  inline QString WarningBg    = QStringLiteral("#fbf0df");
  inline QString WarningBorder= QStringLiteral("#d8ad70");
  // Info
  inline QString Info         = QStringLiteral("#356b73");
  inline QString InfoBg       = QStringLiteral("#e9f1f2");
  inline QString InfoBorder   = QStringLiteral("#9cbfc3");
  // Chrome
  inline QString ToolbarBg    = QStringLiteral("#f8f9f7");
  inline QString Separator    = QStringLiteral("#d3d8d5");
  inline QString Scrollbar    = QStringLiteral("#b9c1bd");
  inline QString ScrollbarHvr = QStringLiteral("#87938e");
  // Disabled
  inline QString DisabledText = QStringLiteral("#99a29e");
  inline QString DisabledBg   = QStringLiteral("#eceeec");
  // Terminal (dark theme)
  inline QString TerminalBg        = QStringLiteral("#1b2221");
  inline QString TerminalBorder    = QStringLiteral("#35413e");
  inline QString TerminalText      = QStringLiteral("#e2e9e6");
  inline QString TerminalSelection = QStringLiteral("#315f58");
}  // namespace Palette

// ── Typography Scale ─────────────────────────

namespace Font {
  inline constexpr int  Mini    = 14;
  inline constexpr int  Small   = 16;
  inline constexpr int  Base    = 18;
  inline constexpr int  Title   = 24;
  inline constexpr int  Hero    = 30;
  inline constexpr auto Family  = "";  // system default
  inline constexpr auto Mono    = "Consolas, Menlo, monospace";
  // semantic shorthands
  inline int Body()   { return Base; }
  inline int Caption(){ return Mini; }
}  // namespace Font

// ── Spacing Scale ────────────────────────────

namespace Space {
  inline constexpr int  XS   = 4;
  inline constexpr int  SM   = 8;
  inline constexpr int  MD   = 12;
  inline constexpr int  LG   = 16;
  inline constexpr int  XL   = 20;
  inline constexpr int  XXL  = 24;
  inline constexpr int  XXXL = 32;
}  // namespace Space

// ── Radii ────────────────────────────────────

namespace Radius {
  inline constexpr int  SM   = 3;
  inline constexpr int  MD   = 4;
  inline constexpr int  LG   = 5;
  inline constexpr int  XL   = 6;
}  // namespace Radius

// ── Control Heights ──────────────────────────

namespace Control {
  inline constexpr int  Height    = 44;
  inline constexpr int  ToolBtnHeight = 54;
  inline constexpr int  CompactHeight = 36;
}  // namespace Control

// ── Typography helpers ───────────────────────
int FontBasePx();
int FontSmallPx();
int FontMiniPx();
int FontTitlePx();
int ControlHeightPx();

// ── App-level sheet ──────────────────────────
QString ApplicationStyleSheet();
QString DockStyleSheet();
void SetDarkTheme(bool dark);
bool IsDarkTheme();

// ── Panel / Card / Group ─────────────────────
QString PanelStyleSheet();
QString CardStyleSheet();

// ── Buttons ──────────────────────────────────
QString MainButtonStyleSheet();
QString SecondaryButtonStyleSheet();
QString DangerButtonStyleSheet();
QString GhostIconButtonStyleSheet();
QString WindowControlButtonStyleSheet();
QString CloseButtonStyleSheet();             // window-close with danger hover
QString LinkButtonStyleSheet(const QString& color = Palette::Primary);

// ── Tool Buttons ─────────────────────────────
QString ToolButtonStyleSheet();
QString MiniToolButtonStyleSheet();
QString MoveButtonStyleSheet();              // directional pad buttons
QString EditToolButtonStyleSheet();          // map-editing toolbar buttons

// ── Input ────────────────────────────────────
QString InputStyleSheet();
QString CheckBoxStyleSheet();
QString CompactCheckBoxStyleSheet();
QString TransparentStatusInputStyleSheet();

// ── Labels ───────────────────────────────────
QString MutedLabelStyleSheet();
QString TitleLabelStyleSheet();
QString SectionLabelStyleSheet();
QString HintLabelStyleSheet();               // subtle guidance text
QString FieldLabelStyleSheet();              // form field label
QString CaptionLabelStyleSheet();            // small heading for groups

// ── Status / Semantic ────────────────────────
QString TopStatusLabelStyleSheet(const QString& color = Palette::TextSecondary);
QString VoiceStatusLabelStyleSheet();
QString StatusSuccessStyleSheet();           // green success badge
QString StatusDangerStyleSheet();            // red danger badge
QString StatusWarningStyleSheet();           // orange warning badge
QString StatusInfoStyleSheet();              // blue info badge
QString ConnectionStatusStyleSheet(bool connected);  // connected→green, disconnected→muted

// ── Table ────────────────────────────────────
QString TableStyleSheet();

// ── Layout chrome ────────────────────────────
QString ToolStripStyleSheet();               // toolbar background
QString SeparatorStyleSheet();               // thin horizontal rule
QString FocusBorderStyle();                  // :focus-visible ring

}  // namespace UiStyle
