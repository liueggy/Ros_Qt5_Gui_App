/*
 * @Author: chengyang chengyangkj@outlook.com
 * @Date: 2022-12-15 09:59:43
 * @LastEditors: chengyangkj chengyangkj@qq.com
 * @LastEditTime: 2023-10-14 09:48:15
 * @FilePath: ////src/PointShape.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
// NOLINTBEGIN
#include "display/point_shape.h"
#include "QDebug"
#include "algorithm.h"
#include "core/framework/framework.h"
#include "msg/msg_info.h"
#include <cmath>
using namespace basic;
// Unused: #define circle_radius 20
namespace {
constexpr double kRobotFootprintWidthScenePx = 6.0;
constexpr double kRobotFootprintHeightScenePx = 4.0;
constexpr double kRobotIconFootprintFitRatio = 0.50;

QRectF RobotIconRect() {
  const double size = (std::min)(kRobotFootprintWidthScenePx,
                               kRobotFootprintHeightScenePx) *
                      kRobotIconFootprintFitRatio;
  return QRectF(-size / 2.0, -size / 2.0, size, size);
}
}  // namespace
namespace Display {
PointShape::PointShape(const ePointType &type, const std::string &display_type,
                       const std::string &display_name, const int &z_value,
                       std::string parent_name)
    : VirtualDisplay(display_type, z_value, parent_name, display_name),
      type_(type) {
  // 机器人可旋转；导航点固定为图钉朝上，避免被姿态或拖拽旋转。
  SetRotateEnable(type_ != kNavGoal);
  SetScaleEnable(false);
  moveBy(0, 0);
  switch (type_) {
    case kRobot: {
      setZValue(10);
      robot_svg_renderer_.load(QString("://images/robot.svg"));
      deg_offset_ = 45;
      SetBoundingRect(RobotIconRect());
    } break;
    case kParticle: {
    } break;
    case kNavGoal: {
      // 导航点需要位于机器人图标之上，避免透明 SVG 中透出绿色机器人。
      setZValue(11);
      robot_svg_renderer_.load(QString("://images/target.svg"));
      deg_offset_ = 0;
      SetBoundingRect(RobotIconRect());
    } break;
  }
}
QVariant PointShape::itemChange(GraphicsItemChange change,
                                const QVariant &value) {
  switch (change) {
    case ItemPositionHasChanged:
      curr_scene_pose_ = RobotPose(scenePos().x(), scenePos().y(),
                                   normalize(robot_pose_.theta + rotate_value_));
      emit signalPoseUpdate(curr_scene_pose_);
      break;
    // case ItemTransformHasChanged:
    //   emit signalPoseUpdate(
    //       Eigen::Vector3f(scenePos().x(), scenePos().y(), rotate_value_));
    //   break;
    // case ItemRotationHasChanged:
    //   emit signalPoseUpdate(
    //       Eigen::Vector3f(scenePos().x(), scenePos().y(), rotate_value_));
    //   break;
    default:
      break;
  };
  return QGraphicsItem::itemChange(change, value);
}
bool PointShape::UpdateData(const RobotPose &pose) {
  robot_pose_ = pose;
  rotate_value_ = 0;
  SetPoseInParent(pose);
  update();
  return true;
}
bool PointShape::SetDisplayConfig(const std::string &config_name,
                                  const std::any &config_data) {
  if (config_name == "Enable") {
    GetAnyData(bool, config_data, enable_);
  } else {
    return false;
  }
  return true;
}
void PointShape::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget) {
  painter->setRenderHints(QPainter::Antialiasing |
                          QPainter::SmoothPixmapTransform);
  switch (type_) {
    case kRobot:
      drawRobot(painter);
      break;
    case kParticle:
      drawParticle(painter);
      break;
    case kNavGoal:
      drawNavGoal(painter);
      break;
  }
  static double last_rotate_value = rotate_value_;

  if (fabs(rotate_value_ - last_rotate_value) > deg2rad(0.1)) {
    curr_scene_pose_ = RobotPose(scenePos().x(), scenePos().y(),
                                 normalize(robot_pose_.theta + rotate_value_));
    emit signalPoseUpdate(
        RobotPose(scenePos().x(), scenePos().y(),
                  normalize(robot_pose_.theta + rotate_value_)));
  }
}
void PointShape::setEnable(const bool &enable) {}
void PointShape::drawRobot(QPainter *painter) {
  painter->setRenderHint(QPainter::Antialiasing, true);  // 设置反锯齿 反走样
  painter->save();
  painter->rotate(-rad2deg(robot_pose_.theta) - rad2deg(rotate_value_) + deg_offset_);
  const QRectF targetRect = RobotIconRect();
  // 将SVG图形渲染到QPainter
  robot_svg_renderer_.render(painter, targetRect);
  painter->restore();
}
void PointShape::drawNavGoal(QPainter *painter) {
  painter->setRenderHint(QPainter::Antialiasing, true);  // 设置反锯齿 反走样
  
  // 设置目标图标透明度
  painter->save();
  
  // 绘制不旋转的目标SVG图标
  const QRectF targetRect = RobotIconRect();
  // 地图视图可能整体旋转（默认约 90°），对导航图标做反向补偿，
  // 使图钉始终保持屏幕朝上，而点位坐标仍跟随地图移动。
  const QTransform view_transform = painter->worldTransform();
  const double view_rotation =
      rad2deg(std::atan2(view_transform.m12(), view_transform.m11()));
  painter->rotate(-view_rotation);
  // 导航点可能与机器人当前位置重叠，先绘制浅色隔离底，避免两个图标视觉粘连。
  painter->setPen(Qt::NoPen);
  painter->setBrush(QColor(244, 245, 247, 230));
  painter->drawEllipse(QPointF(0, 0), targetRect.width() * 0.9,
                       targetRect.height() * 0.9);
  robot_svg_renderer_.render(painter, targetRect);
  
  painter->restore();
  
}
// void PointShape::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
//   QMenu menu;
//   QAction *removeAction = menu.addAction("Navigation");
//   QAction *selectedAction = menu.addAction("Delete");
//   menu.exec(event->screenPos());
//   connect(removeAction, SIGNAL(triggered()), this, SLOT(slotRemoveItem()));
// }
void PointShape::drawParticle(QPainter *painter) {}
// NOLINTEND
}  // namespace Display

