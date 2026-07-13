#include "display_config_widget.h"
#include <QAbstractItemView>
#include <QFontMetrics>
#include <QFrame>
#include <QHeaderView>
#include <QIcon>
#include <QIntValidator>
#include <QListWidgetItem>
#include <utility>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpacerItem>
#include <algorithm>
#include "config/config_manager.h"
#include "display/manager/display_factory.h"
#include "display/manager/display_manager.h"
#include "display/virtual_display.h"
#include "logger/logger.h"
#include "msg/msg_info.h"
#include "widgets/ui_style.h"

namespace {

QString LineEditStyle() {
  return UiStyle::InputStyleSheet();
}

}  // namespace

DisplayConfigWidget::DisplayConfigWidget(QWidget* parent)
    : QWidget(parent), robot_color_(QColor(0, 0, 255)) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMinimumWidth(0);
  ApplyGlobalStyle();
  InitUI();
}

DisplayConfigWidget::~DisplayConfigWidget() {}

void DisplayConfigWidget::ApplyGlobalStyle() {
  setStyleSheet(QStringLiteral(
                     "DisplayConfigWidget { background-color:%1; }"
                     "DisplayConfigWidget QWidget { font-size:%2px; color:%3; }"
                     "DisplayConfigWidget QStackedWidget#settingsPageStack { background-color:%1; }"
                     "DisplayConfigWidget QLabel#pageTitle { font-size:%4px; font-weight:700; color:%3; padding-bottom:5px; }"
                     "DisplayConfigWidget QLabel#pageSubtitle { font-size:%5px; color:%6; padding-bottom:12px; }"
                     "DisplayConfigWidget QListWidget#settingsNav { background-color:%7; border:1px solid %8; border-radius:5px; padding:6px; outline:none; }"
                     "DisplayConfigWidget QListWidget#settingsNav::item { color:%9; font-size:%2px; padding:12px 10px; border-radius:3px; margin:2px 0; border:1px solid transparent; min-height:28px; }"
                     "DisplayConfigWidget QListWidget#settingsNav::item:hover { background-color:%10; border-color:%11; }"
                     "DisplayConfigWidget QListWidget#settingsNav::item:selected { background-color:%12; border-color:%13; color:%14; font-weight:600; }"
                     "DisplayConfigWidget QFrame#settingsCard { background-color:%12; border:1px solid %8; border-radius:5px; }"
                     "DisplayConfigWidget QTableWidget { font-size:%2px; }"
                     "DisplayConfigWidget QScrollArea { border:none; background:transparent; }"
                     "DisplayConfigWidget QToolTip { background:%12; color:%3; border:1px solid %8; padding:6px 8px; border-radius:4px; }")
                     .arg(UiStyle::Palette::Background)
                     .arg(UiStyle::FontBasePx())
                     .arg(UiStyle::Palette::Text)
                     .arg(UiStyle::FontTitlePx())
                     .arg(UiStyle::FontSmallPx())
                     .arg(UiStyle::Palette::TextSecondary)
                     .arg(UiStyle::Palette::PrimaryLight)
                     .arg(UiStyle::Palette::Border)
                     .arg(UiStyle::Palette::TextSecondary)
                     .arg(UiStyle::Palette::SurfaceHover)
                     .arg(UiStyle::Palette::Border)
                     .arg(UiStyle::Palette::Surface)
                     .arg(UiStyle::Palette::BorderHover)
                     .arg(UiStyle::Palette::Primary));
}

QFrame* DisplayConfigWidget::CreateSettingsCard(QWidget* parent) {
  QFrame* card = new QFrame(parent);
  card->setObjectName(QStringLiteral("settingsCard"));
  card->setProperty("uiCard", true);
  card->setStyleSheet(UiStyle::CardStyleSheet());
  return card;
}

QLabel* DisplayConfigWidget::AddSectionHeader(QVBoxLayout* layout, const QString& title) {
  QLabel* lbl = new QLabel(title);
  lbl->setStyleSheet(UiStyle::SectionLabelStyleSheet());
  layout->addWidget(lbl);
  return lbl;
}

void DisplayConfigWidget::InitUI() {
  main_layout_ = new QVBoxLayout(this);
  main_layout_->setContentsMargins(16, 14, 16, 14);
  main_layout_->setSpacing(0);
  setAutoFillBackground(true);
  setAttribute(Qt::WA_StyledBackground, true);
  QPalette themed_palette = palette();
  themed_palette.setColor(QPalette::Window,
                          QColor(UiStyle::Palette::Background));
  themed_palette.setColor(QPalette::WindowText,
                          QColor(UiStyle::Palette::Text));
  themed_palette.setColor(QPalette::Base,
                          QColor(UiStyle::Palette::Surface));
  themed_palette.setColor(QPalette::Text,
                          QColor(UiStyle::Palette::Text));
  setPalette(themed_palette);

  title_label_ = new QLabel(tr("设置"), this);
  title_label_->setStyleSheet(UiStyle::TitleLabelStyleSheet() + QStringLiteral("padding:4px 2px 14px 2px;"));
  main_layout_->addWidget(title_label_);

  QHBoxLayout* body = new QHBoxLayout();
  body->setSpacing(16);
  body->setContentsMargins(0, 0, 0, 0);

  nav_list_ = new QListWidget(this);
  nav_list_->setObjectName(QStringLiteral("settingsNav"));
  const QFontMetrics nav_metrics(font());
  int nav_text_width = 0;
  nav_list_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  nav_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  nav_list_->setFocusPolicy(Qt::StrongFocus);
  const QVector<std::pair<QString, QString>> navItems = {
      {tr("通道"), QStringLiteral(":/icons/tabler/plug-connected.svg")},
      {tr("显示与话题"), QStringLiteral(":/icons/tabler/messages.svg")},
      {tr("摄像头"), QStringLiteral(":/icons/tabler/camera.svg")},
      {tr("机器人外形"), QStringLiteral(":/icons/tabler/polygon.svg")},
  };
  for (const auto& item : navItems) {
    auto* nav_item = new QListWidgetItem(
        UiStyle::TintedIcon(item.second, QSize(24, 24)), item.first, nav_list_);
    nav_text_width = (std::max)(nav_text_width,
                                nav_metrics.horizontalAdvance(item.first));
    nav_item->setSizeHint(QSize(150, 54));
  }
  const int nav_width = (std::min)(190, (std::max)(154, nav_text_width + 66));
  nav_list_->setFixedWidth(nav_width);
  nav_list_->setMinimumHeight(navItems.size() * 62 + 18);

  page_stack_ = new QStackedWidget(this);
  page_stack_->setObjectName(QStringLiteral("settingsPageStack"));
  page_stack_->setAutoFillBackground(true);
  page_stack_->setAttribute(Qt::WA_StyledBackground, true);
  page_stack_->setPalette(themed_palette);
  page_stack_->setMinimumWidth(0);
  page_stack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  page_stack_->addWidget(CreateChannelPage());
  page_stack_->addWidget(CreateLayersPage());
  page_stack_->addWidget(CreateImagePage());
  page_stack_->addWidget(CreateRobotPage());
  for (int index = 0; index < page_stack_->count(); ++index) {
    QWidget* page = page_stack_->widget(index);
    page->setObjectName(QStringLiteral("settingsPage"));
    page->setAutoFillBackground(true);
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setPalette(themed_palette);
    page->setStyleSheet(QStringLiteral(
        "QWidget#settingsPage { background-color:%1; }")
        .arg(UiStyle::Palette::Background));
  }

  connect(nav_list_, &QListWidget::currentRowChanged, page_stack_, &QStackedWidget::setCurrentIndex);
  nav_list_->setCurrentRow(0);

  body->addWidget(nav_list_);
  body->addWidget(page_stack_, 1);
  main_layout_->addLayout(body, 1);
}

QWidget* DisplayConfigWidget::CreateChannelPage() {
  QWidget* page = new QWidget;
  QVBoxLayout* root = new QVBoxLayout(page);
  root->setContentsMargins(8, 4, 8, 8);
  root->setSpacing(10);

  QLabel* page_title = new QLabel(tr("连接小车"));
  page_title->setObjectName(QStringLiteral("pageTitle"));
  root->addWidget(page_title);

  QFrame* card = CreateSettingsCard(page);
  QVBoxLayout* card_layout = new QVBoxLayout(card);
  card_layout->setContentsMargins(16, 16, 16, 16);
  card_layout->setSpacing(12);

  auto* card_title = new QLabel(tr("ROSBridge 连接"), card);
  card_title->setStyleSheet(UiStyle::CaptionLabelStyleSheet());
  card_title->setVisible(false);

  QHBoxLayout* type_layout = new QHBoxLayout();
  channel_type_label_ = new QLabel(tr("方式"));
  channel_type_label_->setFixedWidth(56);
  channel_type_label_->setStyleSheet(UiStyle::FieldLabelStyleSheet());
  channel_type_combo_ = new QComboBox(card);
  channel_type_combo_->setMinimumHeight(36);
  channel_type_combo_->setStyleSheet(UiStyle::InputStyleSheet());
  connect(channel_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
    if (is_loading_config_) {
      return;
    }
    std::string channel_type = channel_type_combo_->itemData(index).toString().toStdString();
    Config::ConfigManager::Instance()->UpdateRootConfig(
        [&channel_type](auto& config) { config.channel_config.channel_type = channel_type; });
    bool show_rosbridge = (channel_type == "rosbridge");
    rosbridge_ip_edit_->setEnabled(show_rosbridge);
    rosbridge_port_edit_->setEnabled(show_rosbridge);
  });
  type_layout->addWidget(channel_type_label_);
  type_layout->addWidget(channel_type_combo_, 1);
  card_layout->addLayout(type_layout);

  rosbridge_section_label_ = new QLabel(tr("目标地址"));
  rosbridge_section_label_->setStyleSheet(UiStyle::CaptionLabelStyleSheet());
  rosbridge_section_label_->setVisible(false);

  QHBoxLayout* ip_layout = new QHBoxLayout();
  rosbridge_ip_label_ = new QLabel(tr("地址"));
  rosbridge_ip_label_->setFixedWidth(56);
  rosbridge_ip_label_->setStyleSheet(UiStyle::FieldLabelStyleSheet());
  rosbridge_ip_edit_ = new QLineEdit(card);
  rosbridge_ip_edit_->setPlaceholderText(QStringLiteral("192.168.31.50"));
  rosbridge_ip_edit_->setStyleSheet(UiStyle::InputStyleSheet());
  connect(rosbridge_ip_edit_, &QLineEdit::editingFinished, [this]() {
    if (is_loading_config_) {
      return;
    }
    QString new_ip = rosbridge_ip_edit_->text();
    Config::ConfigManager::Instance()->UpdateRootConfig([&new_ip](auto& config) {
      config.channel_config.rosbridge_config.ip = new_ip.toStdString();
    });
  });
  ip_layout->addWidget(rosbridge_ip_label_);
  ip_layout->addWidget(rosbridge_ip_edit_, 1);
  card_layout->addLayout(ip_layout);

  QHBoxLayout* port_layout = new QHBoxLayout();
  rosbridge_port_label_ = new QLabel(tr("端口"));
  rosbridge_port_label_->setFixedWidth(56);
  rosbridge_port_label_->setStyleSheet(UiStyle::FieldLabelStyleSheet());
  rosbridge_port_edit_ = new QLineEdit(card);
  rosbridge_port_edit_->setValidator(new QIntValidator(1, 65535, rosbridge_port_edit_));
  rosbridge_port_edit_->setPlaceholderText(QStringLiteral("9090"));
  rosbridge_port_edit_->setStyleSheet(UiStyle::InputStyleSheet());
  connect(rosbridge_port_edit_, &QLineEdit::editingFinished, [this]() {
    if (is_loading_config_) {
      return;
    }
    if (!rosbridge_port_edit_->hasAcceptableInput()) {
      const auto config = Config::ConfigManager::Instance()->GetRootConfigSnapshot();
      const QString saved_port = QString::fromStdString(
          config.channel_config.rosbridge_config.port.empty()
              ? std::string("9090")
              : config.channel_config.rosbridge_config.port);
      rosbridge_port_edit_->setText(saved_port);
      rosbridge_port_edit_->setToolTip(tr("端口必须是 1 到 65535 之间的整数"));
      return;
    }
    rosbridge_port_edit_->setToolTip(QString());
    QString new_port = rosbridge_port_edit_->text();
    Config::ConfigManager::Instance()->UpdateRootConfig([&new_port](auto& config) {
      config.channel_config.rosbridge_config.port = new_port.toStdString();
    });
  });
  port_layout->addWidget(rosbridge_port_label_);
  port_layout->addWidget(rosbridge_port_edit_, 1);
  card_layout->addLayout(port_layout);

  root->addWidget(card);

  connection_status_card_ = new QFrame(page);
  connection_status_card_->setObjectName(QStringLiteral("connectionStatusCard"));
  QHBoxLayout* connection_action_layout = new QHBoxLayout(connection_status_card_);
  connection_action_layout->setContentsMargins(14, 10, 14, 10);
  connection_action_layout->setSpacing(12);
  auto* connection_copy = new QVBoxLayout();
  connection_copy->setSpacing(2);
  connection_status_title_ = new QLabel(tr("等待连接"), connection_status_card_);
  connection_status_title_->setStyleSheet(UiStyle::CaptionLabelStyleSheet());
  connection_status_label_ = new QLabel(tr("检测中"), connection_status_card_);
  connection_status_label_->setStyleSheet(UiStyle::MutedLabelStyleSheet());
  connection_status_label_->setWordWrap(true);
  connection_copy->addWidget(connection_status_title_);
  connection_copy->addWidget(connection_status_label_);

  reconnect_channel_btn_ = new QPushButton(tr("连接"), connection_status_card_);
  reconnect_channel_btn_->setCursor(Qt::PointingHandCursor);
  reconnect_channel_btn_->setMinimumWidth(106);
  reconnect_channel_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  reconnect_channel_btn_->setStyleSheet(UiStyle::MainButtonStyleSheet());
  connect(reconnect_channel_btn_, &QPushButton::clicked, [this]() {
    if (reconnect_channel_btn_->property("connected").toBool()) {
      emit DisconnectRequested();
    } else {
      emit ConnectRequested();
    }
  });
  connection_action_layout->addLayout(connection_copy, 1);
  connection_action_layout->addWidget(reconnect_channel_btn_, 0, Qt::AlignVCenter);
  root->addWidget(connection_status_card_);
  root->addStretch(1);
  return page;
}

void DisplayConfigWidget::SetConnectionState(bool connected, bool connecting,
                                             const QString& message) {
  if (!reconnect_channel_btn_ || !connection_status_label_ ||
      !connection_status_title_ || !connection_status_card_) {
    return;
  }
  reconnect_channel_btn_->setProperty("connected", connected);
  reconnect_channel_btn_->setEnabled(!connecting);
  reconnect_channel_btn_->setText(connecting ? tr("正在检测…")
                                             : (connected ? tr("断开连接") : tr("连接")));
  reconnect_channel_btn_->setStyleSheet(
      connected ? UiStyle::DangerButtonStyleSheet() : UiStyle::MainButtonStyleSheet());

  const QString color = connecting ? UiStyle::Palette::Warning
                                   : (connected ? UiStyle::Palette::Success
                                                : UiStyle::Palette::Danger);
  const QString background = connecting ? UiStyle::Palette::WarningBg
                                        : (connected ? UiStyle::Palette::SuccessBg
                                                     : UiStyle::Palette::DangerBg);
  const QString border = connecting ? UiStyle::Palette::WarningBorder
                                    : (connected ? UiStyle::Palette::SuccessBorder
                                                 : UiStyle::Palette::DangerBorder);
  connection_status_title_->setText(connecting ? tr("正在连接")
                                               : (connected ? tr("小车已连接")
                                                            : tr("小车未连接")));
  Q_UNUSED(message);
  const QString compact_message = connecting ? tr("检测中")
                                             : (connected ? tr("在线")
                                                          : tr("启动后重试"));
  connection_status_label_->setText(compact_message);
  connection_status_label_->setStyleSheet(
      QStringLiteral("QLabel { color:%1; background:transparent; border:none; "
                     "font-size:%2px; font-weight:400; padding:0; }")
          .arg(color)
          .arg(UiStyle::FontSmallPx()));
  connection_status_title_->setStyleSheet(
      QStringLiteral("QLabel { color:%1; background:transparent; border:none; "
                     "font-size:%2px; font-weight:700; padding:0; }")
          .arg(color)
          .arg(UiStyle::FontMiniPx()));
  connection_status_card_->setStyleSheet(
      QStringLiteral("QFrame#connectionStatusCard { background:%1; border:1px solid %2; "
                     "border-radius:10px; }")
          .arg(background, border));
  emit ConnectionStateChanged(connected, connecting, message);
}

QWidget* DisplayConfigWidget::CreateLayersPage() {
  QWidget* page = new QWidget;
  QVBoxLayout* root = new QVBoxLayout(page);
  root->setContentsMargins(8, 4, 8, 8);
  root->setSpacing(10);

  QLabel* page_title = new QLabel(tr("显示与话题"));
  page_title->setObjectName(QStringLiteral("pageTitle"));
  root->addWidget(page_title);

  QScrollArea* scroll = new QScrollArea(page);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  QWidget* scroll_body = new QWidget;
  scroll_body->setMinimumWidth(0);
  QVBoxLayout* sl = new QVBoxLayout(scroll_body);
  sl->setContentsMargins(0, 0, 8, 0);
  sl->setSpacing(4);

  const struct {
    const char* section_en;
    std::vector<std::pair<std::string, const char*>> rows;
  } groups[] = {
      {"地图与定位",
       {{DISPLAY_MAP, "占据栅格地图"},
        {DISPLAY_ROBOT, "里程计 / 机器人"}}},
      {"感知", {{DISPLAY_LASER, "激光扫描"}}},
      {"路径规划",
       {{DISPLAY_GLOBAL_PATH, "全局路径"}, {DISPLAY_LOCAL_PATH, "局部路径"}}},
      {"代价地图",
       {{DISPLAY_GLOBAL_COST_MAP, "全局代价地图"},
        {DISPLAY_LOCAL_COST_MAP, "局部代价地图"}}},
      {"交互",
       {{DISPLAY_ROBOT_FOOTPRINT, "机器人足迹"}, {DISPLAY_GOAL, "导航目标"}}},
  };

  for (const auto& grp : groups) {
    AddSectionHeader(sl, tr(grp.section_en));
    QFrame* card = CreateSettingsCard(scroll_body);
    QVBoxLayout* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(0, 0, 0, 0);
    card_layout->setSpacing(0);

    for (size_t i = 0; i < grp.rows.size(); ++i) {
      const auto& entry = grp.rows[i];
      const std::string& display_name = entry.first;

      QWidget* row = new QWidget(card);
      row->setStyleSheet(QStringLiteral("QWidget { background:transparent; }"));  // intentional
      QHBoxLayout* h = new QHBoxLayout(row);
      h->setContentsMargins(14, 12, 14, 12);
      h->setSpacing(12);

      QLabel* name = new QLabel(tr(entry.second));
      name->setWordWrap(true);
      name->setMinimumWidth(128);
      name->setMaximumWidth(240);
      name->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      name->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
      name->setStyleSheet(UiStyle::TopStatusLabelStyleSheet());

      QLineEdit* topic_edit = new QLineEdit(row);
      topic_edit->setPlaceholderText(QStringLiteral("/topic/name"));
      topic_edit->setMinimumWidth(140);
      topic_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      topic_edit->setStyleSheet(UiStyle::InputStyleSheet());
      display_topic_edits_[display_name] = topic_edit;
      connect(topic_edit, &QLineEdit::editingFinished, [this, display_name, topic_edit]() {
        OnDisplayTopicChanged(display_name, topic_edit->text());
      });

      QCheckBox* vis_cb = new QCheckBox(row);
      vis_cb->setChecked(true);
      vis_cb->setCursor(Qt::PointingHandCursor);
      vis_cb->setText(QString());
      vis_cb->setStyleSheet(QStringLiteral(
          "QCheckBox { spacing: 0; padding: 0; }"
          "QCheckBox::indicator { width: 20px; height: 20px; }"));
      display_toggle_buttons_[display_name] = vis_cb;
      connect(vis_cb, &QCheckBox::toggled, [this, display_name](bool checked) {
        OnToggleDisplay(display_name, checked);
      });

      h->addWidget(name, 0);
      h->addWidget(topic_edit, 1);
      h->addWidget(vis_cb, 0, Qt::AlignCenter);
      card_layout->addWidget(row);

      if (i + 1 < grp.rows.size()) {
        QFrame* sep = new QFrame(card);
        sep->setFixedHeight(1);
        sep->setStyleSheet(UiStyle::SeparatorStyleSheet());
        card_layout->addWidget(sep);
      }
    }
    sl->addWidget(card);
  }

  sl->addStretch();
  scroll->setWidget(scroll_body);
  root->addWidget(scroll, 1);
  return page;
}

QWidget* DisplayConfigWidget::CreateImagePage() {
  QWidget* page = new QWidget;
  QVBoxLayout* root = new QVBoxLayout(page);
  root->setContentsMargins(8, 4, 8, 8);
  root->setSpacing(10);

  QLabel* page_title = new QLabel(tr("摄像头"));
  page_title->setObjectName(QStringLiteral("pageTitle"));
  root->addWidget(page_title);

  QFrame* card = CreateSettingsCard(page);
  QVBoxLayout* card_layout = new QVBoxLayout(card);
  card_layout->setContentsMargins(10, 10, 10, 10);
  card_layout->setSpacing(8);

  image_table_ = new QTableWidget(0, 4, card);
  image_table_->setHorizontalHeaderLabels(
      QStringList() << tr("位置") << tr("话题") << tr("启用") << QString());
  image_table_->setMinimumHeight(96);
  image_table_->setMaximumHeight(182);
  image_table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  image_table_->horizontalHeader()->setMinimumSectionSize(64);
  image_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  image_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  image_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
  image_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
  image_table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  image_table_->setColumnWidth(0, 72);
  image_table_->setColumnWidth(2, 52);
  image_table_->setColumnWidth(3, 58);
  image_table_->verticalHeader()->setVisible(false);
  image_table_->verticalHeader()->setDefaultSectionSize(42);
  image_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  image_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  image_table_->setShowGrid(false);
  image_table_->setAlternatingRowColors(false);
  image_table_->setWordWrap(false);
  image_table_->setTextElideMode(Qt::ElideRight);
  image_table_->setStyleSheet(UiStyle::TableStyleSheet());

  connect(image_table_, &QTableWidget::cellChanged, this, [this](int row, int) {
    OnImageConfigChanged(row);
  });

  image_add_btn_ = new QPushButton(tr("添加摄像头"), card);
  image_add_btn_->setCursor(Qt::PointingHandCursor);
  image_add_btn_->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  connect(image_add_btn_, &QPushButton::clicked, this, &DisplayConfigWidget::OnAddImageConfig);

  card_layout->addWidget(image_table_);
  card_layout->addWidget(image_add_btn_, 0, Qt::AlignLeft);
  root->addWidget(card, 0, Qt::AlignTop);
  root->addStretch(1);
  return page;
}

QWidget* DisplayConfigWidget::CreateRobotPage() {
  QWidget* page = new QWidget;
  QVBoxLayout* root = new QVBoxLayout(page);
  root->setContentsMargins(8, 4, 8, 8);
  root->setSpacing(10);

  QLabel* page_title = new QLabel(tr("机器人外形"));
  page_title->setObjectName(QStringLiteral("pageTitle"));
  root->addWidget(page_title);

  QScrollArea* scroll = new QScrollArea(page);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  QWidget* scroll_content = new QWidget;
  QVBoxLayout* outer = new QVBoxLayout(scroll_content);
  outer->setContentsMargins(0, 0, 8, 0);
  outer->setSpacing(12);

  robot_polygon_section_label_ = AddSectionHeader(outer, tr("多边形顶点"));
  QFrame* points_card = CreateSettingsCard(scroll_content);
  QVBoxLayout* points_layout = new QVBoxLayout(points_card);
  points_layout->setContentsMargins(14, 14, 14, 14);
  points_layout->setSpacing(10);

  robot_points_hint_label_ = new QLabel(
      tr("以机器人基座为原点的顶点坐标（米）。至少填写 3 个有效顶点后，地图立即显示自定义外形；否则显示小车实时轮廓。"));
  robot_points_hint_label_->setWordWrap(true);
  robot_points_hint_label_->setStyleSheet(UiStyle::MutedLabelStyleSheet());
  points_layout->addWidget(robot_points_hint_label_);

  robot_points_table_ = new QTableWidget(0, 2, points_card);
  robot_points_table_->setHorizontalHeaderLabels(QStringList() << tr("X") << tr("Y"));
  robot_points_table_->horizontalHeader()->setStretchLastSection(true);
  robot_points_table_->verticalHeader()->setVisible(false);
  robot_points_table_->verticalHeader()->setDefaultSectionSize(40);
  robot_points_table_->setMinimumHeight(220);
  robot_points_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  robot_points_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  robot_points_table_->setShowGrid(false);
  robot_points_table_->setAlternatingRowColors(true);
  robot_points_table_->setStyleSheet(UiStyle::TableStyleSheet());

  connect(robot_points_table_, &QTableWidget::cellChanged, this, &DisplayConfigWidget::OnRobotShapePointChanged);

  QHBoxLayout* points_btn_layout = new QHBoxLayout();
  robot_add_vertex_btn_ = new QPushButton(tr("添加顶点"), points_card);
  robot_add_vertex_btn_->setCursor(Qt::PointingHandCursor);
  robot_add_vertex_btn_->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  connect(robot_add_vertex_btn_, &QPushButton::clicked, [this]() {
    int row = robot_points_table_->rowCount();
    robot_points_table_->insertRow(row);
    robot_points_table_->setItem(row, 0, new QTableWidgetItem(QStringLiteral("0.0")));
    robot_points_table_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("0.0")));
    OnRobotShapePointChanged();
  });

  robot_remove_vertex_btn_ = new QPushButton(tr("移除选中"), points_card);
  robot_remove_vertex_btn_->setCursor(Qt::PointingHandCursor);
  robot_remove_vertex_btn_->setStyleSheet(UiStyle::DangerButtonStyleSheet());
  connect(robot_remove_vertex_btn_, &QPushButton::clicked, [this]() {
    int row = robot_points_table_->currentRow();
    if (row >= 0) {
      robot_points_table_->removeRow(row);
      OnRobotShapePointChanged();
    }
  });

  points_btn_layout->addWidget(robot_add_vertex_btn_);
  points_btn_layout->addWidget(robot_remove_vertex_btn_);
  points_btn_layout->addStretch();

  points_layout->addWidget(robot_points_table_);
  points_layout->addLayout(points_btn_layout);
  outer->addWidget(points_card);

  robot_style_section_label_ = AddSectionHeader(outer, tr("样式"));
  QFrame* style_card = CreateSettingsCard(scroll_content);
  QVBoxLayout* style_layout = new QVBoxLayout(style_card);
  style_layout->setContentsMargins(14, 14, 14, 14);
  style_layout->setSpacing(14);

  robot_is_ellipse_checkbox_ = new QCheckBox(tr("按顶点范围绘制椭圆"), style_card);
  robot_is_ellipse_checkbox_->setStyleSheet(UiStyle::CheckBoxStyleSheet());
  robot_is_ellipse_checkbox_->setToolTip(
      tr("仅影响 Qt 地图显示，不会修改小车板端的导航足迹。"));
  connect(robot_is_ellipse_checkbox_, &QCheckBox::toggled, this, &DisplayConfigWidget::OnRobotShapeIsEllipseChanged);

  QHBoxLayout* color_layout = new QHBoxLayout();
  robot_color_caption_label_ = new QLabel(tr("颜色"));
  robot_color_caption_label_->setFixedWidth(72);
  robot_color_caption_label_->setStyleSheet(UiStyle::TopStatusLabelStyleSheet());
  robot_color_button_ = new QPushButton(tr("选择颜色"), style_card);
  robot_color_button_->setMinimumWidth(120);
  robot_color_button_->setCursor(Qt::PointingHandCursor);
  robot_color_button_->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  connect(robot_color_button_, &QPushButton::clicked, this, &DisplayConfigWidget::OnRobotShapeColorChanged);
  color_layout->addWidget(robot_color_caption_label_);
  color_layout->addWidget(robot_color_button_);
  color_layout->addStretch();

  QHBoxLayout* opacity_layout = new QHBoxLayout();
  robot_opacity_caption_label_ = new QLabel(tr("不透明度"));
  robot_opacity_caption_label_->setFixedWidth(72);
  robot_opacity_caption_label_->setStyleSheet(UiStyle::TopStatusLabelStyleSheet());
  robot_opacity_slider_ = new QSlider(Qt::Horizontal, style_card);
  robot_opacity_slider_->setRange(0, 100);
  robot_opacity_slider_->setValue(50);
  robot_opacity_slider_->setStyleSheet(
      QStringLiteral("QSlider::groove:horizontal { height:6px; background:%1; border-radius:3px; }"
                     "QSlider::handle:horizontal { background:%1; width:18px; margin:-6px 0; border-radius:9px; }"));
  robot_opacity_label_ = new QLabel(QStringLiteral("50%"), style_card);
  robot_opacity_label_->setFixedWidth(44);
  robot_opacity_label_->setStyleSheet(UiStyle::MutedLabelStyleSheet());
  connect(robot_opacity_slider_, &QSlider::valueChanged, [this](int value) {
    robot_opacity_label_->setText(QString::number(value) + QStringLiteral("%"));
    OnRobotShapeOpacityChanged(value);
  });
  opacity_layout->addWidget(robot_opacity_caption_label_);
  opacity_layout->addWidget(robot_opacity_slider_, 1);
  opacity_layout->addWidget(robot_opacity_label_);

  style_layout->addWidget(robot_is_ellipse_checkbox_);
  style_layout->addLayout(color_layout);
  style_layout->addLayout(opacity_layout);

  outer->addWidget(style_card);
  outer->addStretch();

  scroll->setWidget(scroll_content);
  root->addWidget(scroll, 1);
  return page;
}

void DisplayConfigWidget::SetChannelList(const std::vector<std::string>& channel_list) {
  channel_list_ = channel_list;

  channel_type_combo_->blockSignals(true);
  channel_type_combo_->clear();

  channel_type_combo_->addItem(tr("自动"), QStringLiteral("auto"));
  for (const auto& channel_type : channel_list_) {
    QString q = QString::fromStdString(channel_type);
    channel_type_combo_->addItem(q, q);
  }

  const auto config = Config::ConfigManager::Instance()->GetRootConfigSnapshot();
  std::string channel_type =
      config.channel_config.channel_type.empty() ? "rosbridge" : config.channel_config.channel_type;
  int index = channel_type_combo_->findData(QString::fromStdString(channel_type));
  if (index >= 0) {
    channel_type_combo_->setCurrentIndex(index);
  } else {
    channel_type_combo_->setCurrentIndex(0);
  }

  channel_type_combo_->blockSignals(false);
}

void DisplayConfigWidget::SetDisplayManager(Display::DisplayManager* manager) {
  display_manager_ = manager;
  if (display_manager_) {
    LoadConfig();
  }
}

void DisplayConfigWidget::OnToggleDisplay(const std::string& display_name, bool visible) {
  UpdateDisplayVisibility(display_name, visible);
  auto topic_it = display_topic_edits_.find(display_name);
  std::string topic =
      (topic_it != display_topic_edits_.end()) ? topic_it->second->text().toStdString() : std::string();
  Config::ConfigManager::Instance()->UpdateRootConfig([&](auto& config) {
    auto it = std::find_if(config.display_config.begin(), config.display_config.end(),
                           [&display_name](const auto& item) { return item.display_name == display_name; });
    if (it != config.display_config.end()) it->visible = visible;
    else config.display_config.push_back(Config::DisplayConfig(display_name, topic, visible));
  });
}

void DisplayConfigWidget::OnDisplayTopicChanged(const std::string& display_name, const QString& topic) {
  auto toggle_it = display_toggle_buttons_.find(display_name);
  const bool visible = (toggle_it != display_toggle_buttons_.end()) ? toggle_it->second->isChecked() : true;
  const std::string topic_name = topic.toStdString();
  Config::ConfigManager::Instance()->UpdateRootConfig([&](auto& config) {
    auto it = std::find_if(config.display_config.begin(), config.display_config.end(),
                           [&display_name](const auto& item) { return item.display_name == display_name; });
    if (it != config.display_config.end()) it->topic = topic_name;
    else config.display_config.push_back(Config::DisplayConfig(display_name, topic_name, visible));
  });
}

void DisplayConfigWidget::UpdateImageTableHeight() {
  if (!image_table_) {
    return;
  }

  int visible_rows = image_table_->rowCount();
  if (visible_rows < 1) {
    visible_rows = 1;
  }
  if (visible_rows > 3) {
    visible_rows = 3;
  }

  int header_height = image_table_->horizontalHeader() ? image_table_->horizontalHeader()->height() : 34;
  if (header_height < 30) {
    header_height = 34;
  }
  int row_height = image_table_->verticalHeader()->defaultSectionSize();
  if (row_height < 32) {
    row_height = 42;
  }
  const int frame_height = image_table_->frameWidth() * 2;
  image_table_->setFixedHeight(header_height + visible_rows * row_height + frame_height + 4);
}

void DisplayConfigWidget::OnAddImageConfig() {
  int row = image_table_->rowCount();
  image_table_->insertRow(row);

  QTableWidgetItem* location_item = new QTableWidgetItem(QString());
  location_item->setToolTip(QStringLiteral("停靠标识，如 front、rear"));
  QTableWidgetItem* topic_item = new QTableWidgetItem(QString());
  topic_item->setToolTip(QStringLiteral("图像话题，如 /camera/front/image_raw"));
  QTableWidgetItem* enable_item = new QTableWidgetItem(QString());
  enable_item->setFlags(enable_item->flags() & ~Qt::ItemIsEditable);

  QCheckBox* enable_checkbox = new QCheckBox();
  enable_checkbox->setChecked(true);
  enable_checkbox->setStyleSheet(QStringLiteral(
      "QCheckBox { margin-left:15px; }"
      "QCheckBox::indicator { width:18px; height:18px; }"));
  connect(enable_checkbox, &QCheckBox::toggled, [this, row](bool) {
    OnImageConfigChanged(row);
  });

  QPushButton* remove_btn = new QPushButton(tr("移除"));
  remove_btn->setCursor(Qt::PointingHandCursor);
  remove_btn->setFixedWidth(58);
  remove_btn->setStyleSheet(UiStyle::LinkButtonStyleSheet(UiStyle::Palette::Danger));
  connect(remove_btn, &QPushButton::clicked, [this, row]() { OnRemoveImageConfig(row); });

  image_table_->setItem(row, 0, location_item);
  image_table_->setItem(row, 1, topic_item);
  image_table_->setItem(row, 2, enable_item);
  image_table_->setCellWidget(row, 2, enable_checkbox);
  image_table_->setCellWidget(row, 3, remove_btn);

  UpdateImageTableHeight();
  OnImageConfigChanged(row);
}

void DisplayConfigWidget::OnRemoveImageConfig(int row) {
  image_table_->removeRow(row);
  UpdateImageTableHeight();

  Config::ConfigManager::Instance()->UpdateRootConfig([row](auto& config) {
    if (row >= 0 && row < static_cast<int>(config.images.size())) {
      config.images.erase(config.images.begin() + row);
    }
  });

  for (int i = row; i < image_table_->rowCount(); i++) {
    QPushButton* btn = qobject_cast<QPushButton*>(image_table_->cellWidget(i, 3));
    if (btn) {
      btn->disconnect();
      connect(btn, &QPushButton::clicked, [this, i]() { OnRemoveImageConfig(i); });
    }
    QCheckBox* checkbox = qobject_cast<QCheckBox*>(image_table_->cellWidget(i, 2));
    if (checkbox) {
      checkbox->disconnect();
      connect(checkbox, &QCheckBox::toggled, [this, i](bool) {
        OnImageConfigChanged(i);
      });
    }
  }
}

void DisplayConfigWidget::OnImageConfigChanged(int row) {
  if (row < 0 || row >= image_table_->rowCount()) {
    return;
  }

  QTableWidgetItem* location_item = image_table_->item(row, 0);
  QTableWidgetItem* topic_item = image_table_->item(row, 1);
  QCheckBox* enable_checkbox = qobject_cast<QCheckBox*>(image_table_->cellWidget(row, 2));

  if (!location_item || !topic_item || !enable_checkbox) {
    return;
  }

  Config::ImageDisplayConfig image_config;
  image_config.location = location_item->text().toStdString();
  image_config.topic = topic_item->text().toStdString();
  image_config.enable = enable_checkbox->isChecked();

  Config::ConfigManager::Instance()->UpdateRootConfig([row, &image_config](auto& config) {
    if (row < static_cast<int>(config.images.size())) config.images[row] = image_config;
    else config.images.push_back(image_config);
  });

  location_item->setToolTip(location_item->text());
  topic_item->setToolTip(topic_item->text());

}

void DisplayConfigWidget::OnRobotShapePointChanged() {
  std::vector<Config::Point> points;

  for (int row = 0; row < robot_points_table_->rowCount(); row++) {
    QTableWidgetItem* x_item = robot_points_table_->item(row, 0);
    QTableWidgetItem* y_item = robot_points_table_->item(row, 1);

    if (x_item && y_item) {
      bool x_ok = false;
      bool y_ok = false;
      double x = x_item->text().toDouble(&x_ok);
      double y = y_item->text().toDouble(&y_ok);

      if (x_ok && y_ok) {
        points.push_back({x, y});
      }
    }
  }

  Config::ConfigManager::Instance()->UpdateRootConfig(
      [&points](auto& config) { config.robot_shape_config.shaped_points = points; });
  ApplyRobotAppearance();
}

void DisplayConfigWidget::OnRobotShapeIsEllipseChanged(bool checked) {
  Config::ConfigManager::Instance()->UpdateRootConfig(
      [checked](auto& config) { config.robot_shape_config.is_ellipse = checked; });
  ApplyRobotAppearance();
}

void DisplayConfigWidget::OnRobotShapeColorChanged() {
  QColor color = QColorDialog::getColor(robot_color_, this, tr("选择颜色"));
  if (color.isValid()) {
    robot_color_ = color;
    QString color_style = QStringLiteral("background-color: %1;").arg(color.name());
    robot_color_button_->setStyleSheet(
        QStringLiteral("QPushButton { border:1px solid %1; border-radius:8px; padding:8px 12px; }"
                       "QPushButton:hover { border-color:%1; }") +
        color_style);

    QString color_str = QStringLiteral("0x%1").arg(color.rgb(), 8, 16, QChar('0')).toUpper();
    const std::string value = color_str.toStdString();
    Config::ConfigManager::Instance()->UpdateRootConfig(
        [&value](auto& config) { config.robot_shape_config.color = value; });
    ApplyRobotAppearance();
  }
}

void DisplayConfigWidget::OnRobotShapeOpacityChanged(int value) {
  Config::ConfigManager::Instance()->UpdateRootConfig([value](auto& config) {
    config.robot_shape_config.opacity = value / 100.0f;
  });
  ApplyRobotAppearance();
}

void DisplayConfigWidget::ApplyRobotAppearance() {
  if (display_manager_) {
    display_manager_->SetRobotAppearanceConfig(
        Config::ConfigManager::Instance()->GetRootConfigSnapshot().robot_shape_config);
  }
}

void DisplayConfigWidget::UpdateDisplayVisibility(const std::string& display_name, bool visible) {
  auto display = Display::FactoryDisplay::Instance()->GetDisplay(display_name);
  if (display) {
    display->setVisible(visible);
    LOG_INFO("Display " << display_name << " visibility set to " << (visible ? "visible" : "hidden"));
  }
}

void DisplayConfigWidget::AutoSaveConfig() {
  Config::ConfigManager::Instance()->StoreConfig();
}

void DisplayConfigWidget::LoadConfig() {
  Config::ConfigManager::Instance()->UpdateRootConfig([](auto& config) {
    auto front = std::find_if(config.images.begin(), config.images.end(),
                              [](const auto& image) { return image.location == "front"; });
    if (front == config.images.end()) {
      config.images.push_back({"front", "/camera/front/image/compressed", true});
    } else {
      if (front->topic.empty()) front->topic = "/camera/front/image/compressed";
      front->enable = true;
    }
  });
  const auto config = Config::ConfigManager::Instance()->GetRootConfigSnapshot();

  for (const auto& display_config : config.display_config) {
    auto toggle_it = display_toggle_buttons_.find(display_config.display_name);
    if (toggle_it != display_toggle_buttons_.end()) {
      toggle_it->second->blockSignals(true);
      toggle_it->second->setChecked(display_config.visible);
      toggle_it->second->blockSignals(false);
      UpdateDisplayVisibility(display_config.display_name, display_config.visible);
    }

    auto topic_it = display_topic_edits_.find(display_config.display_name);
    if (topic_it != display_topic_edits_.end()) {
      topic_it->second->blockSignals(true);
      topic_it->second->setText(QString::fromStdString(display_config.topic));
      topic_it->second->blockSignals(false);
    }
  }

  image_table_->blockSignals(true);
  image_table_->setRowCount(0);
  for (size_t i = 0; i < config.images.size(); i++) {
    const auto& image_config = config.images[i];
    int row = image_table_->rowCount();
    image_table_->insertRow(row);

    QTableWidgetItem* location_item = new QTableWidgetItem(QString::fromStdString(image_config.location));
    location_item->setToolTip(QStringLiteral("位置标识"));
    QTableWidgetItem* topic_item = new QTableWidgetItem(QString::fromStdString(image_config.topic));
    topic_item->setToolTip(QStringLiteral("图像话题"));
    QTableWidgetItem* enable_item = new QTableWidgetItem(QString());
    enable_item->setFlags(enable_item->flags() & ~Qt::ItemIsEditable);

    QCheckBox* enable_checkbox = new QCheckBox();
    enable_checkbox->setChecked(image_config.enable);
    enable_checkbox->setStyleSheet(QStringLiteral(
        "QCheckBox { margin-left:15px; }"
        "QCheckBox::indicator { width:18px; height:18px; }"));
    connect(enable_checkbox, &QCheckBox::toggled, [this, row](bool) {
      OnImageConfigChanged(row);
    });

    QPushButton* remove_btn = new QPushButton(tr("移除"));
    remove_btn->setCursor(Qt::PointingHandCursor);
    remove_btn->setFixedWidth(58);
    remove_btn->setStyleSheet(UiStyle::LinkButtonStyleSheet(UiStyle::Palette::Danger));
    connect(remove_btn, &QPushButton::clicked, [this, row]() { OnRemoveImageConfig(row); });

    image_table_->setItem(row, 0, location_item);
    image_table_->setItem(row, 1, topic_item);
    image_table_->setItem(row, 2, enable_item);
    image_table_->setCellWidget(row, 2, enable_checkbox);
    image_table_->setCellWidget(row, 3, remove_btn);

    location_item->setToolTip(location_item->text());
    topic_item->setToolTip(topic_item->text());
  }
  image_table_->blockSignals(false);
  UpdateImageTableHeight();

  robot_points_table_->blockSignals(true);
  robot_points_table_->setRowCount(0);
  for (const auto& point : config.robot_shape_config.shaped_points) {
    int row = robot_points_table_->rowCount();
    robot_points_table_->insertRow(row);
    robot_points_table_->setItem(row, 0, new QTableWidgetItem(QString::number(point.x)));
    robot_points_table_->setItem(row, 1, new QTableWidgetItem(QString::number(point.y)));
  }
  robot_points_table_->blockSignals(false);

  robot_is_ellipse_checkbox_->blockSignals(true);
  robot_is_ellipse_checkbox_->setChecked(config.robot_shape_config.is_ellipse);
  robot_is_ellipse_checkbox_->blockSignals(false);

  QString color_str = QString::fromStdString(config.robot_shape_config.color);
  if (color_str.startsWith(QStringLiteral("0x"))) {
    bool ok = false;
    uint rgb = color_str.mid(2).toUInt(&ok, 16);
    if (ok) {
      robot_color_ = QColor::fromRgb(rgb);
      QString color_style = QStringLiteral("background-color: %1;").arg(robot_color_.name());
      robot_color_button_->setStyleSheet(
          QStringLiteral("QPushButton { border:1px solid %1; border-radius:8px; padding:8px 12px; }"
                         "QPushButton:hover { border-color:%1; }") +
          color_style);
    }
  }

  robot_opacity_slider_->blockSignals(true);
  robot_opacity_slider_->setValue(static_cast<int>(config.robot_shape_config.opacity * 100));
  robot_opacity_label_->setText(QString::number(robot_opacity_slider_->value()) + QStringLiteral("%"));
  robot_opacity_slider_->blockSignals(false);
  ApplyRobotAppearance();

  is_loading_config_ = true;

  std::string channel_type =
      config.channel_config.channel_type.empty() ? "rosbridge" : config.channel_config.channel_type;
  channel_type_combo_->blockSignals(true);
  int index = channel_type_combo_->findData(QString::fromStdString(channel_type));
  if (index >= 0) {
    channel_type_combo_->setCurrentIndex(index);
  } else {
    channel_type_combo_->setCurrentIndex(0);
  }
  channel_type_combo_->blockSignals(false);

  std::string rosbridge_ip =
      config.channel_config.rosbridge_config.ip.empty() ? "192.168.31.50" : config.channel_config.rosbridge_config.ip;
  rosbridge_ip_edit_->blockSignals(true);
  rosbridge_ip_edit_->setText(QString::fromStdString(rosbridge_ip));
  rosbridge_ip_edit_->blockSignals(false);

  std::string rosbridge_port =
      config.channel_config.rosbridge_config.port.empty() ? "9090" : config.channel_config.rosbridge_config.port;
  rosbridge_port_edit_->blockSignals(true);
  rosbridge_port_edit_->setText(QString::fromStdString(rosbridge_port));
  rosbridge_port_edit_->blockSignals(false);

  bool show_rosbridge = (channel_type == "rosbridge");
  rosbridge_ip_edit_->setEnabled(show_rosbridge);
  rosbridge_port_edit_->setEnabled(show_rosbridge);

  is_loading_config_ = false;
}

void DisplayConfigWidget::SaveConfig() {
  AutoSaveConfig();
}

