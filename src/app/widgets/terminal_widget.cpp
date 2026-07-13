#include "terminal_widget.h"

#include <algorithm>

#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTime>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include "widgets/ui_style.h"

namespace {

struct ShortcutCommand {
  const char* label;
  const char* command;
};

const ShortcutCommand kShortcutCommands[] = {
    {"查看 ROS 节点", "rosnode list"},
    {"查看 ROS 话题", "rostopic list"},
    {"查看 ROS 服务", "rosservice list"},
    {"查看 ROS 参数", "rosparam list"},
    {"查看机器人定位", "rostopic echo -n 1 /amcl_pose"},
    {"查看速度指令", "rostopic echo -n 1 /cmd_vel"},
    {"停止自动探索", "rostopic pub -1 /auto_explore/stop std_msgs/Bool true"},
    {"查看磁盘空间", "df -h"},
    {"查看内存使用", "free -h"},
    {"查看异常服务", "systemctl list-units --no-pager --type=service --state=failed"},
};

class TerminalHighlighter final : public QSyntaxHighlighter {
 public:
  explicit TerminalHighlighter(QTextDocument* document)
      : QSyntaxHighlighter(document) {}

 protected:
  void highlightBlock(const QString& text) override {
    Highlight(text, QStringLiteral("^root@firefly:[^#]*#"),
              QStringLiteral("#5eead4"), true);
    Highlight(text,
              QStringLiteral("\\b(rosnode|rostopic|rosservice|rosparam|roslaunch|"
                             "rosrun|systemctl)\\b"),
              QStringLiteral("#8ddbd3"), true);
    Highlight(text, QStringLiteral("(^|\\s)--?[A-Za-z0-9_-]+"),
              QStringLiteral("#c4b5fd"), false);
    Highlight(text,
              QStringLiteral("\\b(error|failed|failure|denied|timeout|rejected|"
                             "异常|失败|错误|超时|拒绝)\\b"),
              QStringLiteral("#fca5a5"), true,
              QRegularExpression::CaseInsensitiveOption);
    Highlight(text,
              QStringLiteral("\\b(ok|success|completed|active|running|"
                             "成功|完成|正常|已连接)\\b"),
              QStringLiteral("#86efac"), false,
              QRegularExpression::CaseInsensitiveOption);
  }

 private:
  void Highlight(const QString& text, const QString& pattern,
                 const QString& color, bool bold,
                 QRegularExpression::PatternOption option =
                     QRegularExpression::NoPatternOption) {
    QRegularExpression expression(pattern, option);
    QTextCharFormat format;
    format.setForeground(QColor(color));
    if (bold) format.setFontWeight(QFont::DemiBold);
    auto matches = expression.globalMatch(text);
    while (matches.hasNext()) {
      const auto match = matches.next();
      setFormat(match.capturedStart(), match.capturedLength(), format);
    }
  }
};

QString TerminalControlStyle() {
  return QStringLiteral(
             "QPushButton, QComboBox { min-height:30px; padding:3px 10px;"
             " border:1px solid %1; border-radius:4px; color:%2;"
             " background:%3; font-size:%4px; }"
             "QPushButton:hover, QComboBox:hover { border-color:%5; background:%6; }"
             "QPushButton:pressed { background:%7; }"
             "QPushButton:disabled { color:#71807b; background:%3; border-color:%1; }"
             "QComboBox::drop-down { border:0; width:24px; }"
             "QComboBox QAbstractItemView { color:%2; background:%3;"
             " border:1px solid %5; selection-background-color:%8; }")
      .arg(UiStyle::Palette::TerminalBorder, UiStyle::Palette::TerminalText,
           UiStyle::Palette::TerminalBg, QString::number(UiStyle::FontMiniPx()),
           UiStyle::Palette::BorderHover, UiStyle::Palette::TerminalSelection,
           UiStyle::Palette::PrimaryPress, UiStyle::Palette::Primary);
}

}  // namespace

TerminalWidget::TerminalWidget(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("terminalRoot"));
  setMinimumSize(520, 260);
  setStyleSheet(QStringLiteral("#terminalRoot { background:%1; }")
                    .arg(UiStyle::Palette::TerminalBg));

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto* toolbar = new QWidget();
  toolbar->setObjectName(QStringLiteral("terminalToolbar"));
  toolbar->setStyleSheet(
      QStringLiteral("#terminalToolbar { background:%1; border-bottom:1px solid %2; }"
                     "#terminalToolbar QLabel { background:transparent; }")
          .arg(UiStyle::Palette::TerminalBg, UiStyle::Palette::TerminalBorder));
  auto* title_row = new QHBoxLayout(toolbar);
  title_row->setContentsMargins(14, 8, 10, 8);
  title_row->setSpacing(8);

  status_dot_ = new QLabel(QStringLiteral("●"));
  status_dot_->setFixedWidth(12);
  title_row->addWidget(status_dot_);

  status_label_ = new QLabel(tr("未连接"));
  status_label_->setToolTip(tr("命令通过板端白名单安全执行"));
  status_label_->setStyleSheet(
      QStringLiteral("color:#9fb0aa; font-size:%1px;")
          .arg(UiStyle::FontMiniPx()));
  title_row->addWidget(status_label_);
  title_row->addStretch();

  quick_command_combo_ = new QComboBox();
  quick_command_combo_->setMinimumWidth(190);
  quick_command_combo_->setToolTip(tr("选择后填入命令行，不会自动执行"));
  quick_command_combo_->addItem(tr("快捷命令…"), QString());
  for (const auto& shortcut : kShortcutCommands) {
    quick_command_combo_->addItem(tr(shortcut.label),
                                  QString::fromLatin1(shortcut.command));
  }
  connect(quick_command_combo_, QOverload<int>::of(&QComboBox::activated),
          this, [this](int index) {
            const QString command = quick_command_combo_->itemData(index).toString();
            if (!command.isEmpty()) SetCommandText(command);
            quick_command_combo_->setCurrentIndex(0);
          });
  title_row->addWidget(quick_command_combo_);

  terminate_button_ = new QPushButton(tr("终止"));
  terminate_button_->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/icons/tabler/player-stop.svg"), QSize(18, 18),
      UiStyle::Palette::Danger));
  terminate_button_->setToolTip(tr("终止当前命令（Ctrl+C）"));
  connect(terminate_button_, &QPushButton::clicked, this,
          &TerminalWidget::TerminateCommand);
  title_row->addWidget(terminate_button_);

  clear_button_ = new QPushButton(tr("清屏"));
  clear_button_->setToolTip(tr("清空终端输出（Ctrl+L）"));
  connect(clear_button_, &QPushButton::clicked, this,
          &TerminalWidget::ClearOutput);
  title_row->addWidget(clear_button_);
  toolbar->setStyleSheet(toolbar->styleSheet() + TerminalControlStyle());
  root->addWidget(toolbar);

  output_edit_ = new QPlainTextEdit();
  output_edit_->setReadOnly(true);
  output_edit_->setUndoRedoEnabled(false);
  output_edit_->setMaximumBlockCount(5000);
  output_edit_->setLineWrapMode(QPlainTextEdit::NoWrap);
  output_edit_->setTabStopWidth(32);
  output_edit_->setPlaceholderText(tr("暂无输出"));
  output_edit_->setStyleSheet(
      QStringLiteral("QPlainTextEdit { background:%1; color:%2; border:0;"
                     " padding:12px 14px; font-family:%3; font-size:%4px;"
                     " selection-background-color:%5; selection-color:#ffffff; }"
                     "QScrollBar:vertical { background:%1; width:10px; margin:0; }"
                     "QScrollBar::handle:vertical { background:%6; min-height:28px;"
                     " border-radius:4px; margin:2px; }"
                     "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                     " height:0; }")
          .arg(UiStyle::Palette::TerminalBg, UiStyle::Palette::TerminalText,
               QString::fromLatin1(UiStyle::Font::Mono),
               QString::number(UiStyle::FontSmallPx()),
               UiStyle::Palette::TerminalSelection,
               UiStyle::Palette::TerminalBorder));
  new TerminalHighlighter(output_edit_->document());
  output_edit_->installEventFilter(this);
  root->addWidget(output_edit_, 1);

  auto* composer = new QWidget();
  composer->setObjectName(QStringLiteral("terminalComposer"));
  composer->setStyleSheet(
      QStringLiteral("#terminalComposer { background:%1; border-top:1px solid %2; }"
                     "#terminalPrompt { color:#5eead4; font-family:%3;"
                     " font-weight:600; font-size:%4px; }"
                     "QLineEdit { min-height:38px; color:%5; background:#131a18;"
                     " border:1px solid %2; border-radius:4px; padding:0 10px;"
                     " font-family:%3; font-size:%4px; selection-background-color:%6; }"
                     "QLineEdit:focus { border-color:%7; }"
                     "QLineEdit:disabled { color:#71807b; background:#171d1b; }")
          .arg(UiStyle::Palette::TerminalBg, UiStyle::Palette::TerminalBorder,
               QString::fromLatin1(UiStyle::Font::Mono),
               QString::number(UiStyle::FontSmallPx()),
               UiStyle::Palette::TerminalText,
               UiStyle::Palette::TerminalSelection, UiStyle::Palette::Primary));
  auto* command_row = new QHBoxLayout(composer);
  command_row->setContentsMargins(14, 10, 10, 10);
  command_row->setSpacing(10);

  auto* prompt_label = new QLabel(QStringLiteral("root@firefly:~#"));
  prompt_label->setObjectName(QStringLiteral("terminalPrompt"));
  command_row->addWidget(prompt_label);

  command_edit_ = new QLineEdit();
  command_edit_->setPlaceholderText(tr("输入命令"));
  command_edit_->setToolTip(tr("仅支持板端白名单命令，按 Enter 执行"));
  command_edit_->setClearButtonEnabled(true);
  command_edit_->installEventFilter(this);
  connect(command_edit_, &QLineEdit::returnPressed, this,
          &TerminalWidget::ExecuteCommand);
  command_row->addWidget(command_edit_, 1);

  run_button_ = new QPushButton(tr("运行"));
  run_button_->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/icons/tabler/player-play.svg"), QSize(18, 18),
      UiStyle::Palette::TextOnPrimary));
  run_button_->setToolTip(tr("执行当前命令（Enter）"));
  run_button_->setStyleSheet(
      QStringLiteral("QPushButton { min-width:82px; min-height:38px; color:%1;"
                     " background:%2; border:1px solid %2; border-radius:4px;"
                     " padding:0 14px; font-weight:600; font-size:%3px; }"
                     "QPushButton:hover { background:%4; border-color:%4; }"
                     "QPushButton:pressed { background:%5; border-color:%5; }"
                     "QPushButton:disabled { color:#71807b; background:#26302d;"
                     " border-color:#35413e; }")
          .arg(UiStyle::Palette::TextOnPrimary, UiStyle::Palette::Primary,
               QString::number(UiStyle::FontSmallPx()),
               UiStyle::Palette::PrimaryHover, UiStyle::Palette::PrimaryPress));
  connect(run_button_, &QPushButton::clicked, this,
          &TerminalWidget::ExecuteCommand);
  command_row->addWidget(run_button_);
  root->addWidget(composer);

  cancel_shortcut_ = new QShortcut(QKeySequence(QStringLiteral("Ctrl+C")), this);
  cancel_shortcut_->setContext(Qt::WidgetWithChildrenShortcut);
  connect(cancel_shortcut_, &QShortcut::activated, this,
          &TerminalWidget::TerminateCommand);

  auto* clear_shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this);
  clear_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(clear_shortcut, &QShortcut::activated, this,
          &TerminalWidget::ClearOutput);

  command_watchdog_ = new QTimer(this);
  command_watchdog_->setSingleShot(true);
  command_watchdog_->setInterval(35000);
  connect(command_watchdog_, &QTimer::timeout, this, [this]() {
    if (!command_running_) return;
    SetCommandRunning(false);
    AppendStatus(tr("命令响应超时，终端已恢复"));
  });

  SetConnected(false);
}

void TerminalWidget::AppendOutput(const QString& text, const QString& stream) {
  if (text.isEmpty()) return;

  QScrollBar* scroll_bar = output_edit_->verticalScrollBar();
  const bool follow_output = scroll_bar->value() >= scroll_bar->maximum() - 2;
  QTextCursor cursor(output_edit_->document());
  cursor.movePosition(QTextCursor::End);
  QTextCharFormat format;
  format.setForeground(stream.compare(QStringLiteral("stderr"),
                                      Qt::CaseInsensitive) == 0
                           ? QColor(QStringLiteral("#fca5a5"))
                           : QColor(UiStyle::Palette::TerminalText));
  cursor.insertText(text, format);
  if (follow_output) {
    output_edit_->setTextCursor(cursor);
    output_edit_->ensureCursorVisible();
  }
}

void TerminalWidget::AppendStatus(const QString& text) {
  if (text.isEmpty()) return;
  const QString timestamp = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
  AppendOutput(QStringLiteral("[%1] %2\n").arg(timestamp, text));
  UpdateStatus(command_running_, text);
}

void TerminalWidget::SetCommandRunning(bool running) {
  command_running_ = running;
  if (running) {
    command_watchdog_->start();
  } else {
    command_watchdog_->stop();
  }
  RefreshControls();
  UpdateStatus(running, running ? tr("命令执行中")
                                : (connected_ ? tr("已连接，可执行命令")
                                              : tr("未连接")));
  if (running) {
    output_edit_->setFocus();
  } else if (connected_) {
    command_edit_->setFocus();
  }
}

void TerminalWidget::SetConnected(bool connected) {
  connected_ = connected;
  if (!connected_) {
    command_running_ = false;
    command_watchdog_->stop();
  }
  RefreshControls();
  UpdateStatus(command_running_,
               connected_ ? (command_running_ ? tr("命令执行中")
                                              : tr("已连接，可执行命令"))
                          : tr("未连接，等待 ROSBridge"));
}

bool TerminalWidget::eventFilter(QObject* watched, QEvent* event) {
  if (watched == command_edit_ && event->type() == QEvent::KeyPress) {
    auto* key_event = static_cast<QKeyEvent*>(event);
    if (key_event->key() == Qt::Key_Up) {
      NavigateHistory(-1);
      return true;
    }
    if (key_event->key() == Qt::Key_Down) {
      NavigateHistory(1);
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void TerminalWidget::ExecuteCommand() {
  if (command_running_ || !connected_) {
    if (!connected_) UpdateStatus(false, tr("请先连接小车"));
    return;
  }
  const QString command = command_edit_->text().trimmed();
  if (command.isEmpty()) {
    UpdateStatus(false, tr("请输入命令"));
    command_edit_->setFocus();
    return;
  }

  if (command_history_.isEmpty() || command_history_.constLast() != command) {
    command_history_.append(command);
  }
  history_index_ = command_history_.size();
  pending_command_.clear();
  AppendCommandEcho(command);
  command_edit_->clear();
  SetCommandRunning(true);
  emit CommandRequested(MakeRequestJson(command));
}

void TerminalWidget::TerminateCommand() {
  if (!command_running_) return;
  UpdateStatus(true, tr("正在终止命令…"));
  terminate_button_->setEnabled(false);
  emit TerminateRequested();
}

void TerminalWidget::ClearOutput() {
  output_edit_->clear();
  UpdateStatus(command_running_, command_running_ ? tr("命令执行中")
                                                  : (connected_ ? tr("已连接，可执行命令")
                                                                : tr("未连接")));
}

QString TerminalWidget::MakeRequestJson(const QString& command) const {
  QJsonObject request;
  request[QStringLiteral("command_id")] =
      QStringLiteral("terminal-%1-%2")
          .arg(QDateTime::currentMSecsSinceEpoch())
          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  request[QStringLiteral("command")] = command;
  request[QStringLiteral("timeout")] = 30;
  return QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact));
}

void TerminalWidget::AppendCommandEcho(const QString& command) {
  QTextCursor cursor(output_edit_->document());
  cursor.movePosition(QTextCursor::End);
  if (!output_edit_->toPlainText().isEmpty() &&
      !output_edit_->toPlainText().endsWith(QLatin1Char('\n'))) {
    cursor.insertText(QStringLiteral("\n"));
  }
  QTextCharFormat prompt_format;
  prompt_format.setForeground(QColor(QStringLiteral("#5eead4")));
  prompt_format.setFontWeight(QFont::DemiBold);
  cursor.insertText(QStringLiteral("root@firefly:~# "), prompt_format);
  QTextCharFormat command_format;
  command_format.setForeground(QColor(UiStyle::Palette::TerminalText));
  cursor.insertText(command + QLatin1Char('\n'), command_format);
  output_edit_->setTextCursor(cursor);
  output_edit_->ensureCursorVisible();
}

void TerminalWidget::SetCommandText(const QString& command) {
  if (command_running_) return;
  command_edit_->setText(command);
  command_edit_->setFocus();
  command_edit_->setCursorPosition(command.size());
}

void TerminalWidget::NavigateHistory(int direction) {
  if (command_history_.isEmpty()) return;
  if (history_index_ == command_history_.size() && direction < 0) {
    pending_command_ = command_edit_->text();
  }
  history_index_ = std::clamp(history_index_ + direction, 0,
                              static_cast<int>(command_history_.size()));
  SetCommandText(history_index_ == command_history_.size()
                     ? pending_command_
                     : command_history_.at(history_index_));
}

void TerminalWidget::UpdateStatus(bool running, const QString& text) {
  status_label_->setText(text);
  const QString dot_color = running
                                ? UiStyle::Palette::Warning
                                : (connected_ ? QStringLiteral("#34d399")
                                              : QStringLiteral("#71807b"));
  status_dot_->setStyleSheet(
      QStringLiteral("color:%1; font-size:12px;").arg(dot_color));
}

void TerminalWidget::RefreshControls() {
  const bool can_enter_command = connected_ && !command_running_;
  command_edit_->setEnabled(can_enter_command);
  run_button_->setEnabled(can_enter_command);
  quick_command_combo_->setEnabled(can_enter_command);
  terminate_button_->setEnabled(connected_ && command_running_);
  cancel_shortcut_->setEnabled(connected_ && command_running_);
  clear_button_->setEnabled(true);
}
