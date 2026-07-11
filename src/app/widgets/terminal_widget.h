#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QComboBox;
class QTimer;

class TerminalWidget : public QWidget {
  Q_OBJECT

 public:
  explicit TerminalWidget(QWidget* parent = nullptr);

  void AppendOutput(const QString& text);
  void AppendStatus(const QString& text);
  void SetCommandRunning(bool running);
  void SetConnected(bool connected);

 signals:
  void CommandRequested(const QString& request_json);
  void TerminateRequested();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private slots:
  void ExecuteCommand();
  void TerminateCommand();
  void ClearOutput();

 private:
  QString MakeRequestJson(const QString& command) const;
  void AddPrompt();
  QString CurrentCommand() const;
  void SetCommandText(const QString& command);
  void NavigateHistory(int direction);
  void UpdateStatus(bool running, const QString& text);
  void RefreshControls();

  QPlainTextEdit* output_edit_{nullptr};
  QPushButton* terminate_button_{nullptr};
  QPushButton* clear_button_{nullptr};
  QLabel* status_label_{nullptr};
  QLabel* status_dot_{nullptr};
  QComboBox* quick_command_combo_{nullptr};
  QTimer* command_watchdog_{nullptr};
  QStringList command_history_;
  int history_index_ = {0};
  int prompt_position_ = {0};
  QString pending_command_;
  bool command_running_ = {false};
  bool connected_ = {false};
  bool prompt_active_ = {false};
};
