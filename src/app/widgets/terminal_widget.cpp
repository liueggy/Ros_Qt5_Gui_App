#include "terminal_widget.h"

#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
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

}  // namespace

TerminalWidget::TerminalWidget(QWidget* parent) : QWidget(parent) {
  setStyleSheet(UiStyle::PanelStyleSheet() + UiStyle::InputStyleSheet());

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(12, 12, 12, 12);
  root->setSpacing(10);

  auto* title_row = new QHBoxLayout();
  auto* title_label = new QLabel(tr("模拟终端"));
  title_label->setStyleSheet(UiStyle::TitleLabelStyleSheet());
  title_row->addWidget(title_label);
  title_row->addStretch();

  status_label_ = new QLabel(tr("就绪"));
  status_label_->setStyleSheet(UiStyle::MutedLabelStyleSheet());
  title_row->addWidget(status_label_);
  root->addLayout(title_row);

  output_edit_ = new QPlainTextEdit();
  output_edit_->setReadOnly(false);
  output_edit_->setUndoRedoEnabled(false);
  output_edit_->setPlaceholderText(tr("在 $ 提示符后输入命令，按回车执行"));
  output_edit_->setStyleSheet(
      UiStyle::InputStyleSheet() +
      QStringLiteral(
          "QPlainTextEdit { background:#18212f; color:#e8eef7; border:1px solid #2b3a4f;"
          " border-radius:8px; padding:10px; font-family:Consolas,\"Microsoft YaHei Mono\",monospace;"
          " font-size:%1px; selection-background-color:#315b86; }")
          .arg(UiStyle::FontBasePx()));
  root->addWidget(output_edit_, 1);
  output_edit_->installEventFilter(this);
  AddPrompt();

  auto* shortcut_label = new QLabel(tr("快捷命令"));
  shortcut_label->setStyleSheet(UiStyle::SectionLabelStyleSheet());
  root->addWidget(shortcut_label);

  auto* shortcut_row = new QHBoxLayout();
  shortcut_row->setSpacing(6);
  for (const auto& shortcut : kShortcutCommands) {
    auto* button = new QPushButton(tr(shortcut.label));
    button->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
    button->setToolTip(QString::fromLatin1(shortcut.command));
    connect(button, &QPushButton::clicked, this,
            [this, shortcut]() { SetCommandText(QString::fromLatin1(shortcut.command)); });
    shortcut_row->addWidget(button);
  }
  shortcut_row->addStretch();
  root->addLayout(shortcut_row);

  auto* command_row = new QHBoxLayout();
  command_row->addStretch();
  terminate_button_ = new QPushButton(tr("终止"));
  terminate_button_->setStyleSheet(UiStyle::DangerButtonStyleSheet());
  terminate_button_->setEnabled(false);
  connect(terminate_button_, &QPushButton::clicked, this, &TerminalWidget::TerminateCommand);
  command_row->addWidget(terminate_button_);

  auto* clear_button = new QPushButton(tr("清空"));
  clear_button->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  connect(clear_button, &QPushButton::clicked, this, &TerminalWidget::ClearOutput);
  command_row->addWidget(clear_button);
  root->addLayout(command_row);
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
  const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
  AppendOutput(QStringLiteral("[%1] %2\n").arg(timestamp, text));
  status_label_->setText(text);
}

void TerminalWidget::SetCommandRunning(bool running) {
  const bool was_running = command_running_;
  command_running_ = running;
  terminate_button_->setEnabled(running);
  output_edit_->setTextInteractionFlags(
      running ? Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse
              : Qt::TextEditorInteraction);
  status_label_->setText(running ? tr("命令执行中") : tr("就绪"));
  if (was_running && !running) {
    AddPrompt();
  } else if (!running) {
    output_edit_->setFocus();
  }
}

bool TerminalWidget::eventFilter(QObject* watched, QEvent* event) {
  if (watched == output_edit_ && event->type() == QEvent::KeyPress) {
    auto* key_event = static_cast<QKeyEvent*>(event);
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
  if (command_running_) {
    return;
  }
  const QString command = CurrentCommand().trimmed();
  if (command.isEmpty()) {
    status_label_->setText(tr("请输入命令"));
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
  status_label_->setText(tr("正在终止"));
  terminate_button_->setEnabled(false);
  emit TerminateRequested();
}

void TerminalWidget::ClearOutput() {
  output_edit_->clear();
  prompt_active_ = false;
  if (!command_running_) {
    AddPrompt();
  }
  status_label_->setText(command_running_ ? tr("命令执行中") : tr("就绪"));
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
  history_index_ = qBound(0, history_index_, command_history_.size());
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
  cursor.insertText(QStringLiteral("$ "));
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
