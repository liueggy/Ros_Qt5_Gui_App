/*
 * Copyright (c) 2011, Dirk Thomas, TU Darmstadt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the TU Darmstadt nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "widgets/ratio_layouted_frame.h"

#include <assert.h>
#include <QMouseEvent>



RatioLayoutedFrame::RatioLayoutedFrame(QWidget* parent, Qt::WindowFlags flags)
    : QFrame(parent, flags), outer_layout_(NULL), aspect_ratio_(4, 3), smoothImage_(false) {
  connect(this, SIGNAL(delayed_update()), this, SLOT(update()), Qt::QueuedConnection);
}

RatioLayoutedFrame::~RatioLayoutedFrame() {
}

const QImage& RatioLayoutedFrame::getImage() const {
  return qimage_;
}

QImage RatioLayoutedFrame::getImageCopy() const {
  QImage img;
  qimage_mutex_.lock();
  img = qimage_.copy();
  qimage_mutex_.unlock();
  return img;
}

void RatioLayoutedFrame::setImage(const QImage& image)  //, QMutex* image_mutex)
{
  {
    QMutexLocker lock(&qimage_mutex_);
    qimage_ = image.copy();
    setAspectRatio(qimage_.width(), qimage_.height());
  }
  update();
}

void RatioLayoutedFrame::resizeToFitAspectRatio() {
  QRect rect = contentsRect();

  // reduce longer edge to aspect ration
  double width;
  double height;

  if (outer_layout_) {
    width = outer_layout_->contentsRect().width();
    height = outer_layout_->contentsRect().height();
  } else {
    // if outer layout isn't available, this will use the old
    // width and height, but this can shrink the display image if the
    // aspect ratio changes.
    width = rect.width();
    height = rect.height();
  }

  double layout_ar = width / height;
  const double image_ar = double(aspect_ratio_.width()) / double(aspect_ratio_.height());
  if (layout_ar > image_ar) {
    // too large width
    width = height * image_ar;
  } else {
    // too large height
    height = width / image_ar;
  }
  rect.setWidth(int(width + 0.5));
  rect.setHeight(int(height + 0.5));

  // resize taking the border line into account
  int border = lineWidth();
  resize(rect.width() + 2 * border, rect.height() + 2 * border);
}

void RatioLayoutedFrame::setOuterLayout(QHBoxLayout* outer_layout) {
  outer_layout_ = outer_layout;
}

void RatioLayoutedFrame::setInnerFrameMinimumSize(const QSize& size) {
  int border = lineWidth();
  QSize new_size = size;
  new_size += QSize(2 * border, 2 * border);
  setMinimumSize(new_size);
  emit delayed_update();
}

void RatioLayoutedFrame::setInnerFrameMaximumSize(const QSize& size) {
  int border = lineWidth();
  QSize new_size = size;
  new_size += QSize(2 * border, 2 * border);
  setMaximumSize(new_size);
  emit delayed_update();
}

void RatioLayoutedFrame::setInnerFrameFixedSize(const QSize& size) {
  setInnerFrameMinimumSize(size);
  setInnerFrameMaximumSize(size);
}

void RatioLayoutedFrame::setAspectRatio(unsigned short width, unsigned short height) {
  int divisor = greatestCommonDivisor(width, height);
  if (divisor != 0) {
    aspect_ratio_.setWidth(width / divisor);
    aspect_ratio_.setHeight(height / divisor);
  }
}

void RatioLayoutedFrame::paintEvent(QPaintEvent* event) {
  QPainter painter(this);
  QImage image;
  {
    QMutexLocker lock(&qimage_mutex_);
    image = qimage_;
  }
  if (!image.isNull()) {
    const QRect target = getAspectRatioCorrectPaintArea();
    if (!smoothImage_) {
      painter.drawImage(target, image);
    } else {
      if (target.size() == image.size()) {
        painter.drawImage(target, image);
      } else {
        painter.drawImage(target, image.scaled(target.size(), Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
      }
    }
  } else {
    // default image with gradient
    QLinearGradient gradient(0, 0, frameRect().width(), frameRect().height());
    gradient.setColorAt(0, Qt::white);
    gradient.setColorAt(1, Qt::black);
    painter.setBrush(gradient);
    painter.drawRect(0, 0, frameRect().width() + 1, frameRect().height() + 1);
  }
}

QRect RatioLayoutedFrame::getAspectRatioCorrectPaintArea() {
  QRect target = contentsRect();
  if (aspect_ratio_.width() <= 0 || aspect_ratio_.height() <= 0 ||
      target.width() <= 0 || target.height() <= 0) {
    return target;
  }
  const double image_ratio = static_cast<double>(aspect_ratio_.width()) /
                             aspect_ratio_.height();
  const double target_ratio = static_cast<double>(target.width()) /
                              target.height();
  if (target_ratio > image_ratio) {
    const int width = static_cast<int>(target.height() * image_ratio + 0.5);
    target.setLeft(target.left() + (target.width() - width) / 2);
    target.setWidth(width);
  } else {
    const int height = static_cast<int>(target.width() / image_ratio + 0.5);
    target.setTop(target.top() + (target.height() - height) / 2);
    target.setHeight(height);
  }
  return target;
}

int RatioLayoutedFrame::greatestCommonDivisor(int a, int b) {
  if (b == 0) {
    return a;
  }
  return greatestCommonDivisor(b, a % b);
}

void RatioLayoutedFrame::mousePressEvent(QMouseEvent* mouseEvent) {
  if (mouseEvent->button() == Qt::LeftButton) {
    emit mouseLeft(mouseEvent->x(), mouseEvent->y());
  }
  QFrame::mousePressEvent(mouseEvent);
}

void RatioLayoutedFrame::onSmoothImageChanged(bool checked) {
  smoothImage_ = checked;
}

