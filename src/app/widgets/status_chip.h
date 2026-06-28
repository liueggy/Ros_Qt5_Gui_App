#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QWidget>

class StatusChip : public QWidget {
 public:
  enum class State { Unknown, Online, Warning, Offline };

  explicit StatusChip(const QString &name, QWidget *parent = nullptr)
      : QWidget(parent), name_(name) {
    setObjectName("statusChip");
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(9, 4, 10, 4);
    layout->setSpacing(6);

    indicator_ = new QLabel(this);
    indicator_->setFixedSize(7, 7);
    indicator_->setObjectName("statusIndicator");

    text_ = new QLabel(this);
    text_->setObjectName("statusText");

    layout->addWidget(indicator_);
    layout->addWidget(text_);
    SetState(State::Unknown, "等待");
  }

  void SetState(State state, const QString &detail) {
    const char *state_name = "unknown";
    switch (state) {
      case State::Online:
        state_name = "online";
        break;
      case State::Warning:
        state_name = "warning";
        break;
      case State::Offline:
        state_name = "offline";
        break;
      case State::Unknown:
        break;
    }
    setProperty("statusState", state_name);
    text_->setText(name_ + "  " + detail);
    setToolTip(name_ + "：" + detail);
    style()->unpolish(this);
    style()->polish(this);
    indicator_->style()->unpolish(indicator_);
    indicator_->style()->polish(indicator_);
  }

 private:
  QString name_;
  QLabel *indicator_{nullptr};
  QLabel *text_{nullptr};
};
