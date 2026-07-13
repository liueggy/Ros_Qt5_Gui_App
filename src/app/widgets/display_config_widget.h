#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QColorDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QFrame>
#include <map>
#include <string>
#include <vector>
#include "config/config_define.h"

namespace Display {
class DisplayManager;
}

class DisplayConfigWidget : public QWidget {
  Q_OBJECT

 public:
  explicit DisplayConfigWidget(QWidget *parent = nullptr);
  ~DisplayConfigWidget();

  void SetDisplayManager(Display::DisplayManager *manager);
  void SetChannelList(const std::vector<std::string> &channel_list);
  void LoadConfig();
  void SaveConfig();
  void SetConnectionState(bool connected, bool connecting, const QString &message);

signals:
  void ConnectRequested();
  void DisconnectRequested();
  void ConnectionStateChanged(bool connected, bool connecting,
                              const QString &message);

 private slots:
  void OnToggleDisplay(const std::string &display_name, bool visible);
  void OnDisplayTopicChanged(const std::string &display_name, const QString &topic);
  void OnAddImageConfig();
  void OnRemoveImageConfig(int row);
  void OnImageConfigChanged(int row);
  void OnRobotShapePointChanged();
  void OnRobotShapeIsEllipseChanged(bool checked);
  void OnRobotShapeColorChanged();
  void OnRobotShapeOpacityChanged(int value);
  void OnMapStyleChanged();
  void OnResetMapStyle();

 private:
  void InitUI();
  void ApplyGlobalStyle();
  QWidget *CreateChannelPage();
  QWidget *CreateLayersPage();
  QWidget *CreateImagePage();
  QWidget *CreateRobotPage();
  QWidget *CreateMapStylePage();
  void ApplyRobotAppearance();
  void ApplyMapStyle();
  void ChooseMapStyleColor(const QString &role, const QString &title);
  void UpdateMapStyleColorButtons(const Config::MapStyleConfig &style);
  void UpdateImageTableHeight();
  void UpdateDisplayVisibility(const std::string &display_name, bool visible);
  void AutoSaveConfig();

  static QFrame *CreateSettingsCard(QWidget *parent);
  static QLabel *AddSectionHeader(QVBoxLayout *layout, const QString &title);

  Display::DisplayManager *display_manager_{nullptr};
  QVBoxLayout *main_layout_{nullptr};
  QListWidget *nav_list_{nullptr};
  QStackedWidget *page_stack_{nullptr};

  std::map<std::string, QCheckBox *> display_toggle_buttons_;
  std::map<std::string, QLineEdit *> display_topic_edits_;
  QTableWidget *image_table_{nullptr};

  QTableWidget *robot_points_table_{nullptr};
  QCheckBox *robot_is_ellipse_checkbox_{nullptr};
  QPushButton *robot_color_button_{nullptr};
  QSlider *robot_opacity_slider_{nullptr};
  QLabel *robot_opacity_label_{nullptr};
  QColor robot_color_;

  QCheckBox *grid_visible_checkbox_{nullptr};
  QSlider *grid_spacing_slider_{nullptr};
  QSlider *grid_opacity_slider_{nullptr};
  QSlider *laser_size_slider_{nullptr};
  QSlider *laser_opacity_slider_{nullptr};
  QSlider *path_width_slider_{nullptr};
  QSlider *costmap_opacity_slider_{nullptr};
  QSpinBox *grid_spacing_spin_{nullptr};
  QSpinBox *grid_opacity_spin_{nullptr};
  QSpinBox *laser_size_spin_{nullptr};
  QSpinBox *laser_opacity_spin_{nullptr};
  QSpinBox *path_width_spin_{nullptr};
  QSpinBox *costmap_opacity_spin_{nullptr};
  QPushButton *grid_color_button_{nullptr};
  QPushButton *laser_color_button_{nullptr};
  QPushButton *global_path_color_button_{nullptr};
  QPushButton *local_path_color_button_{nullptr};

  QComboBox *channel_type_combo_{nullptr};
  QLineEdit *rosbridge_ip_edit_{nullptr};
  QLineEdit *rosbridge_port_edit_{nullptr};
  QPushButton *reconnect_channel_btn_{nullptr};
  QFrame *connection_status_card_{nullptr};
  QLabel *connection_status_title_{nullptr};
  QLabel *connection_status_label_{nullptr};

  QLabel *title_label_{nullptr};
  QLabel *connection_section_label_{nullptr};
  QLabel *channel_type_label_{nullptr};
  QLabel *rosbridge_section_label_{nullptr};
  QLabel *rosbridge_ip_label_{nullptr};
  QLabel *rosbridge_port_label_{nullptr};
  QPushButton *map_browse_btn_{nullptr};
  QPushButton *image_add_btn_{nullptr};
  QLabel *robot_points_hint_label_{nullptr};
  QLabel *robot_polygon_section_label_{nullptr};
  QLabel *robot_style_section_label_{nullptr};
  QLabel *robot_color_caption_label_{nullptr};
  QLabel *robot_opacity_caption_label_{nullptr};
  QPushButton *robot_add_vertex_btn_{nullptr};
  QPushButton *robot_remove_vertex_btn_{nullptr};

  bool is_loading_config_ = {false};
  std::vector<std::string> channel_list_;
};
