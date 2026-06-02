#pragma once

#include <QWidget>
#include <QString>
#include <string>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class CommandCenterWidget : public QWidget {
  Q_OBJECT
 public:
  explicit CommandCenterWidget(QWidget *parent = nullptr);

 public slots:
  void AppendResponse(const std::string &json);
  void UpdateStatus(const std::string &json);

 private slots:
  void SendStatusRequest();
  void SendPresetCommand();
  void SendCustomJson();
  void ReadSelectedParam();
  void WriteSelectedParam();
  void ApplySpeedProfile();
  void SaveCurrentProfile();
  void LoadSavedProfile();
  void RestoreDefaultProfile();
  void UseLocalMapForNav();
  void ExitNavMode();
  void OnParamPresetChanged(int index);
  void ClearLog();

 private:
  QString MakeRequestJson(const QString &command, const QString &target, const QString &paramsJson = "{}") const;
  QString ParamPayloadToJson(const QString &command) const;
  void PublishJson(const QString &json);
  void AppendLog(const QString &prefix, const QString &text);

 private:
  QComboBox *preset_combo_{nullptr};
  QLineEdit *target_edit_{nullptr};
  QComboBox *profile_combo_{nullptr};
  QComboBox *param_combo_{nullptr};
  QLineEdit *param_value_edit_{nullptr};
  QPlainTextEdit *custom_json_edit_{nullptr};
  QPlainTextEdit *log_edit_{nullptr};
  QPlainTextEdit *status_edit_{nullptr};
  QString pending_save_profile_path_;
  QString pending_save_profile_note_;
};
