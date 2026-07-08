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
  inline constexpr auto Primary      = "#1e5fd9";
  inline constexpr auto PrimaryHover = "#174eaf";
  inline constexpr auto PrimaryPress = "#123d8c";
  inline constexpr auto PrimaryLight = "#e8f0fe";
  // Surface
  inline constexpr auto Background   = "#f4f7fb";
  inline constexpr auto Surface      = "#ffffff";
  inline constexpr auto SurfaceHover = "#f8fafd";
  inline constexpr auto SurfaceAlt   = "#f8fbff";
  // Text
  inline constexpr auto Text         = "#0f172a";
  inline constexpr auto TextSecondary= "#435267";
  inline constexpr auto TextMuted    = "#64748b";
  inline constexpr auto TextOnPrimary= "#ffffff";
  // Border
  inline constexpr auto Border       = "#dce4ef";
  inline constexpr auto BorderHover  = "#bcd3fb";
  inline constexpr auto BorderFocus  = "#1e5fd9";
  // Semantic
  inline constexpr auto Success      = "#0f766e";
  inline constexpr auto SuccessBg    = "#ccfbf1";
  inline constexpr auto SuccessBorder= "#5eead4";
  inline constexpr auto Danger       = "#dc2626";
  inline constexpr auto DangerBg     = "#fef2f2";
  inline constexpr auto DangerBorder = "#fca5a5";
  inline constexpr auto DangerHover  = "#b91c1c";
  inline constexpr auto Warning      = "#ea580c";
  inline constexpr auto WarningBg    = "#fff7ed";
  inline constexpr auto WarningBorder= "#fdba74";
  // Info
  inline constexpr auto Info         = "#174ea6";
  inline constexpr auto InfoBg       = "#eef5ff";
  inline constexpr auto InfoBorder   = "#bcd3fb";
  // Chrome
  inline constexpr auto ToolbarBg    = "#fbfdff";
  inline constexpr auto Separator    = "#dce4ef";
  inline constexpr auto Scrollbar    = "#c5cfdd";
  inline constexpr auto ScrollbarHvr = "#9eacbf";
  // Disabled
  inline constexpr auto DisabledText = "#94a3b8";
  inline constexpr auto DisabledBg   = "#f1f5f9";
  // Terminal (dark theme)
  inline constexpr auto TerminalBg        = "#18212f";
  inline constexpr auto TerminalBorder    = "#2b3a4f";
  inline constexpr auto TerminalText      = "#e8eef7";
  inline constexpr auto TerminalSelection = "#315b86";
}  // namespace Palette

// ── Typography Scale ─────────────────────────

namespace Font {
  inline constexpr int  Mini    = 12;
  inline constexpr int  Small   = 14;
  inline constexpr int  Base    = 16;
  inline constexpr int  Title   = 22;
  inline constexpr int  Hero    = 28;
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
  inline constexpr int  SM   = 6;
  inline constexpr int  MD   = 9;
  inline constexpr int  LG   = 12;
  inline constexpr int  XL   = 16;
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
