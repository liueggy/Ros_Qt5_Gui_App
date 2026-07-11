#include "terminal_widget.h"

#include <QDateTime>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTime>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtGlobal>

#include "widgets/ui_style.h"

namespace {

struct ShortcutCommand {
  const char* label;
  const char* command;
};

const ShortcutCommand kShortcutCommands[] = {
    {"ROS 节点", "rosnode list"},
    {"ROS 话题", "rostopic list"},
    {"ROS 服务", "rosservice list"},
    {"ROS 参数", "rosparam list"},
    {"磁盘空间", "df -h"},
    {"内存使用", "free -h"},
    {"系统服务", "systemctl list-units --no-pager --type=service --state=running,failed"},
};

class TerminalHighlighter final : public QSyntaxHighlighter {
 public:
  explicit TerminalHighlighter(QTextDocument* document)
      : QSyntaxHighlighter(document) {}

 protected:
  void highlightBlock(const QString& text) override {
    Highlight(text, QStringLiteral("^root@firefly:[^#]*#"),
              QStringLiteral("#4ADE80"), true);
    Highlight(text, QStringLiteral(
                  "\\b(rosnode|rostopic|rosservice|rosparam|roslaunch|rosrun|systemctl)\\b"),
              QStringLiteral("#67E8F9"), true);
    Highlight(text, QStringLiteral("(^|\\s)--?[A-Za-z0-9_-]+"),
              QStringLiteral("#C4B5FD"), false);
    Highlight(text, QStringLiteral(
                  "\\b(error|failed|failure|denied|timeout|异常|失败|错误|超时)\\b"),
              QStringLiteral("#FCA5A5"), true,
              QRegularExpression::CaseInsensitiveOption);
    Highlight(text, QStringLiteral(
                  "\\b(ok|success|completed|active|running|成功|完成|正常)\\b"),
              QStringLiteral("#86EFAC"), false,
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

}  // namespace

TerminalWidget::TerminalWidget(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("terminalRoot"));
  setStyleSheet(QStringLiteral(
      "#terminalRoot { background:#111827; }"
      "QLabel { color:#CBD5E1; background:transparent; }"
      "QPushButton, QComboBox { min-height:26px; padding:2px 9px;"
      " border:1px solid #334155; border-radius:4px; color:#DCE7F3;"
      " background:#1E293B; font-size:14px; }"
      "QPushButton:hover, QComboBox:hover { border-color:#64748B; background:#273449; }"
      "QPushButton:pressed { background:#334155; }"
      "QPushButton:disabled { color:#64748B; background:#172033; }"
      "QComboBox::drop-down { border:0; width:22px; }"
      "QComboBox QAbstractItemView { color:#DCE7F3; background:#1E293B;"
      " border:1px solid #475569; selection-background-color:#2563EB; }"));

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto* toolbar = new QWidget();
  toolbar->setObjectName(QStringLiteral("terminalToolbar"));
  toolbar->setStyleSheet(QStringLiteral(
      "#terminalToolbar { background:#182235; border-bottom:1px solid #2B3A51; }"));
  auto* title_row = new QHBoxLayout(toolbar);
  title_row->setContentsMargins(10, 5, 8, 5);
  title_row->setSpacing(7);

  status_dot_ = new QLabel(QStringLiteral("●"));
  status_dot_->setFixedWidth(12);
  title_row->addWidget(status_dot_);

  auto* session_label = new QLabel(tr("root@firefly  ·  rosbridge shell"));
  session_label->setStyleSheet(QStringLiteral(
      "font-family:Consolas,'Microsoft YaHei UI'; font-size:14px; color:#E2E8F0;"));
  title_row->addWidget(session_label);

  status_label_ = new QLabel(tr("就绪"));
  status_label_->setStyleSheet(QStringLiteral("font-size:12px; color:#94A3B8;"));
  title_row->addWidget(status_label_);
  title_row->addStretch();

  quick_command_combo_ = new QComboBox();
  quick_command_combo_->setMinimumWidth(150);
  quick_command_combo_->addItem(tr("快捷命令…"), QString());
  for (const auto& shortcut : kShortcutCommands) {
    quick_command_combo_->addItem(tr(shortcut.label),
                                  QString::fromLatin1(shortcut.command));
  }
  connect(quick_command_combo_, QOverload<int>::of(&QComboBox::activated),
          this, [this](int index) {
            const QString command = quick_command_combo_->itemData(index).toString();
            if (!command.isEmpty()) {
              SetCommandText(command);
            }
            quick_command_combo_->setCurrentIndex(0);
          });
  title_row->addWidget(quick_command_combo_);

  terminate_button_ = new QPushButton(tr("■ 终止"));
  terminate_button_->setToolTip(tr("终止当前命令（Ctrl+C）"));
  terminate_button_->setEnabled(false);
  connect(terminate_button_, &QPushButton::clicked, this,
          &TerminalWidget::TerminateCommand);
  title_row->addWidget(terminate_button_);

  clear_button_ = new QPushButton(tr("清屏"));
  clear_button_->setToolTip(tr("清空终端（Ctrl+L）"));
  connect(clear_button_, &QPushButton::clicked, this,
          &TerminalWidget::ClearOutput);
  title_row->addWidget(clear_button_);
  root->addWidget(toolbar);

  output_edit_ = new QPlainTextEdit();
  output_edit_->setReadOnly(false);
  output_edit_->setUndoRedoEnabled(false);
  output_edit_->setMaximumBlockCount(5000);
  output_edit_->setLineWrapMode(QPlainTextEdit::NoWrap);
  output_edit_->setTabStopWidth(32);
  output_edit_->setPlaceholderText(tr("输入板端白名单命令，按 Enter 执行"));
  output_edit_->setStyleSheet(QStringLiteral(
      "QPlainTextEdit { background:#111827; color:#D7E2EE; border:0;"
      " padding:9px 12px; font-family:Consolas,'Cascadia Mono','Microsoft YaHei UI';"
      " font-size:16px; selection-background-color:#264F78;"
      " selection-color:#FFFFFF; }"));
  new TerminalHighlighter(output_edit_->document());
  root->addWidget(output_edit_, 1);
  output_edit_->installEventFilter(this);
  AddPrompt();
  command_watchdog_ = new QTimer(this);
  command_watchdog_->setSingleShot(true);
  command_watchdog_->setInterval(35000);
  connect(command_watchdog_, &QTimer::timeout, this, [this]() {
    if (!command_running_) return;
    AppendStatus(tr("命令响应超时，终端已恢复"));
    SetCommandRunning(false);
  });
  SetConnected(false);
}

void TerminalWidget::AppendOutput(const QString& text) {
  if (text.isEmpty()) {
    return;
  }
  QTextCursor cursor = output_edit_->textCursor();
  cursor.movePosition(QTextCursor::End);
  cursor.insertText(text);
  output_edit_->setTextCursor(cursor);
  output_edit_->ensureCursorVisible();
}

void TerminalWidget::AppendStatus(const QString& text) {
  if (text.isEmpty()) {
    return;
  }
  const QString timestamp = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
  AppendOutput(QStringLiteral("[%1] %2\n").arg(timestamp, text));
  UpdateStatus(command_running_, text);
}

void TerminalWidget::SetCommandRunning(bool running) {
  const bool was_running = command_running_;
  command_running_ = running;
  if (running) {
    command_watchdog_->start();
  } else {
    command_watchdog_->stop();
  }
  RefreshControls();
  UpdateStatus(running, running ? tr("命令执行中") : tr("就绪"));
  if (was_running && !running) {
    AddPrompt();
  } else if (!running) {
    output_edit_->setFocus();
  }
}

void TerminalWidget::SetConnected(bool connected) {
  const bool needs_prompt = connected && !prompt_active_;
  connected_ = connected;
  if (!connected_) {
    command_running_ = false;
    command_watchdog_->stop();
  }
  RefreshControls();
  UpdateStatus(false, connected_ ? tr("已连接，可执行命令")
                                 : tr("未连接，终端不可用"));
  if (needs_prompt) AddPrompt();
}

bool TerminalWidget::eventFilter(QObject* watched, QEvent* event) {
  if (watched == output_edit_ && event->type() == QEvent::KeyPress) {
    auto* key_event = static_cast<QKeyEvent*>(event);
    if (!command_running_ && key_event->modifiers().testFlag(Qt::ControlModifier) &&
        key_event->key() == Qt::Key_L) {
      ClearOutput();
      return true;
    }
    if (command_running_ && key_event->modifiers().testFlag(Qt::ControlModifier) &&
        key_event->key() == Qt::Key_C) {
      TerminateCommand();
      return true;
    }
    if (command_running_) {
      return key_event->matches(QKeySequence::Paste) ||
             key_event->key() == Qt::Key_Return ||
             key_event->key() == Qt::Key_Enter;
    }
    QTextCursor cursor = output_edit_->textCursor();
    if (!cursor.hasSelection() && cursor.position() < prompt_position_) {
      cursor.movePosition(QTextCursor::End);
      output_edit_->setTextCursor(cursor);
    }
    if (key_event->key() == Qt::Key_Return ||
        key_event->key() == Qt::Key_Enter) {
      ExecuteCommand();
      return true;
    }
    if (key_event->key() == Qt::Key_Up) {
      NavigateHistory(-1);
      return true;
    }
    if (key_event->key() == Qt::Key_Down) {
      NavigateHistory(1);
      return true;
    }
    if ((key_event->key() == Qt::Key_Backspace ||
         key_event->key() == Qt::Key_Left) &&
        !cursor.hasSelection() && cursor.position() <= prompt_position_) {
      return true;
    }
    if (key_event->key() == Qt::Key_Home) {
      cursor.setPosition(prompt_position_);
      output_edit_->setTextCursor(cursor);
      return true;
    }
    if (cursor.hasSelection() &&
        cursor.selectionStart() < prompt_position_ &&
        (key_event->key() == Qt::Key_Backspace ||
         key_event->key() == Qt::Key_Delete ||
         !key_event->text().isEmpty())) {
      cursor.clearSelection();
      cursor.movePosition(QTextCursor::End);
      output_edit_->setTextCursor(cursor);
    }
  }
  return QWidget::eventFilter(watched, event);
}

void TerminalWidget::ExecuteCommand() {
  if (command_running_ || !connected_) {
    if (!connected_) UpdateStatus(false, tr("请先连接小车"));
    return;
  }
  const QString command = CurrentCommand().trimmed();
  if (command.isEmpty()) {
    UpdateStatus(false, tr("请输入命令"));
    return;
  }

  if (command_history_.isEmpty() || command_history_.constLast() != command) {
    command_history_.append(command);
  }
  history_index_ = command_history_.size();
  pending_command_.clear();
  QTextCursor cursor = output_edit_->textCursor();
  cursor.movePosition(QTextCursor::End);
  cursor.insertText(QStringLiteral("\n"));
  output_edit_->setTextCursor(cursor);
  prompt_active_ = false;
  SetCommandRunning(true);
  emit CommandRequested(MakeRequestJson(command));
}

void TerminalWidget::TerminateCommand() {
  if (!command_running_) {
    return;
  }
  UpdateStatus(true, tr("正在终止"));
  terminate_button_->setEnabled(false);
  emit TerminateRequested();
}

void TerminalWidget::ClearOutput() {
  if (command_running_) return;
  output_edit_->clear();
  prompt_active_ = false;
  if (!command_running_) {
    AddPrompt();
  }
  UpdateStatus(command_running_, command_running_ ? tr("命令执行中") : tr("就绪"));
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

void TerminalWidget::SetCommandText(const QString& command) {
  if (command_running_) {
    return;
  }
  QTextCursor cursor(output_edit_->document());
  cursor.setPosition(prompt_position_);
  cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
  cursor.insertText(command);
  output_edit_->setTextCursor(cursor);
  output_edit_->setFocus();
}

void TerminalWidget::NavigateHistory(int direction) {
  if (command_history_.isEmpty()) {
    return;
  }

  if (history_index_ == command_history_.size() && direction < 0) {
    pending_command_ = CurrentCommand();
  }

  history_index_ += direction;
  history_index_ = std::clamp(history_index_, 0, static_cast<int>(command_history_.size()));
  if (history_index_ == command_history_.size()) {
    SetCommandText(pending_command_);
  } else {
    SetCommandText(command_history_.at(history_index_));
  }
}

void TerminalWidget::AddPrompt() {
  QTextCursor cursor = output_edit_->textCursor();
  cursor.movePosition(QTextCursor::End);
  if (!output_edit_->toPlainText().isEmpty() &&
      !output_edit_->toPlainText().endsWith(QLatin1Char('\n'))) {
    cursor.insertText(QStringLiteral("\n"));
  }
  cursor.insertText(QStringLiteral("root@firefly:~# "));
  prompt_position_ = cursor.position();
  prompt_active_ = true;
  output_edit_->setTextCursor(cursor);
  output_edit_->setFocus();
}

QString TerminalWidget::CurrentCommand() const {
  if (!prompt_active_) {
    return QString();
  }
  return output_edit_->toPlainText().mid(prompt_position_);
}

void TerminalWidget::UpdateStatus(bool running, const QString& text) {
  status_label_->setText(text);
  status_dot_->setStyleSheet(
      running ? QStringLiteral("color:#FBBF24; font-size:12px;")
              : (connected_ ? QStringLiteral("color:#34D399; font-size:12px;")
                            : QStringLiteral("color:#64748B; font-size:12px;")));
}

void TerminalWidget::RefreshControls() {
  const bool can_enter_command = connected_ && !command_running_;
  output_edit_->setEnabled(connected_);
  output_edit_->setTextInteractionFlags(
      can_enter_command ? Qt::TextEditorInteraction
                        : Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
  quick_command_combo_->setEnabled(can_enter_command);
  terminate_button_->setEnabled(connected_ && command_running_);
  clear_button_->setEnabled(can_enter_command);
}




