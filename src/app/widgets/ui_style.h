#pragma once

#include <QString>

namespace UiStyle {

int FontBasePx();
int FontSmallPx();
int FontMiniPx();
int FontTitlePx();
int ControlHeightPx();
QString PanelStyleSheet();
QString MainButtonStyleSheet();
QString SecondaryButtonStyleSheet();
QString DangerButtonStyleSheet();
QString InputStyleSheet();
QString ChipStyleSheet();
QString TableStyleSheet();
QString CardStyleSheet();
QString MutedLabelStyleSheet();
QString TitleLabelStyleSheet();
QString SectionLabelStyleSheet();
QString ToolButtonStyleSheet();
QString MiniToolButtonStyleSheet();
QString GhostIconButtonStyleSheet();
QString WindowControlButtonStyleSheet();
QString TopStatusLabelStyleSheet(const QString& color = QStringLiteral("#3c4043"));
QString VoiceStatusLabelStyleSheet();
QString CheckBoxStyleSheet();
QString TransparentStatusInputStyleSheet();
QString LinkButtonStyleSheet(const QString& color = QStringLiteral("#1a73e8"));

}  // namespace UiStyle
