#pragma once

#include <QString>
#include <QWidget>
#include <string>

#include "msg/diagnostic_snapshot.h"

class DiagnosticDockWidget;
class QLabel;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;

class CommandCenterWidget : public QWidget {
  Q_OBJECT

 public:
  explicit CommandCenterWidget(QWidget *parent = nullptr);

  void SetDiagnosticSnapshot(const basic::DiagnosticSnapshot &snapshot);

 public slots:
  void AppendResponse(const std::string &json);
  void UpdateStatus(const std::string &json);

 private slots:
  void SendStatusRequest();
  void StartCamera();
  void StopCamera();
  void ReadSelectedSpeedParam();
  void WriteSelectedSpeedParam();
  void ApplySpeedProfile();
  void ClearLog();

 private:
  QString MakeRequestJson(const QString &command, const QString &target,
                          const QString &paramsJson = "{}") const;
  QString SpeedParamPayloadToJson(const QString &command) const;
  void PublishJson(const QString &json);
  void AppendLog(const QString &prefix, const QString &text);
  void SetCameraStateText(const QString &text);

  QComboBox *profile_combo_{nullptr};
  QComboBox *speed_param_combo_{nullptr};
  QLineEdit *speed_value_edit_{nullptr};
  QLabel *camera_state_label_{nullptr};
  QPlainTextEdit *status_edit_{nullptr};
  QPlainTextEdit *log_edit_{nullptr};
  DiagnosticDockWidget *diagnostic_widget_{nullptr};
};
