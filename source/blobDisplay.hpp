#pragma once

#include <QtQuick/QtQuick>

#include <atomic>
#include <unordered_map>
#include <array>
#include <cmath>

/// chameleon provides Qt components for event stream display.
namespace chameleon {

    /// BlobDisplay displays gaussian blobs as ellipses.
    class BlobDisplay : public QQuickPaintedItem {
        Q_OBJECT
        Q_PROPERTY(QSize canvasSize READ canvasSize WRITE setCanvasSize)
        Q_PROPERTY(QColor strokeColor READ strokeColor WRITE setStrokeColor)
        Q_PROPERTY(qreal strokeThickness READ strokeThickness WRITE setStrokeThickness)
        Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor)
        Q_PROPERTY(qreal confidence READ confidence WRITE setConfidence)
        public:
            BlobDisplay(QQuickItem* parent = nullptr) :
                QQuickPaintedItem(parent),
                _brush(Qt::transparent, Qt::SolidPattern),
                _confidence(1.96)
            {
                _pen.setColor(Qt::black);
                _pen.setWidthF(1);
                _accessingBlobs.clear(std::memory_order_release);
            }
            BlobDisplay(const BlobDisplay&) = delete;
            BlobDisplay(BlobDisplay&&) = default;
            BlobDisplay& operator=(const BlobDisplay&) = delete;
            BlobDisplay& operator=(BlobDisplay&&) = default;
            virtual ~BlobDisplay() {}

            /// setCanvasSize defines the display coordinates.
            virtual void setCanvasSize(QSize canvasSize) {
                _canvasSize = canvasSize;
            }

            /// canvasSize returns the currently used canvasSize.
            virtual QSize canvasSize() const {
                return _canvasSize;
            }

            /// setStrokeColor defines the stroke color for the blobs.
            virtual void setStrokeColor(QColor color) {
                _pen.setColor(color);
            }

            /// strokeColor returns the currently used stroke color.
            virtual QColor strokeColor() const {
                return _pen.color();
            }

            /// setStrokeThickness defines the stroke thickness for the blobs.
            virtual void setStrokeThickness(qreal thickness) {
                _pen.setWidthF(thickness);
            }

            /// strokeThickness returns the currently used stroke thickness.
            virtual qreal strokeThickness() const {
                return _pen.widthF();
            }

            /// setFillColor defines the fill color for the blobs.
            virtual void setFillColor(QColor color) {
                _brush.setColor(color);
            }

            /// fillColor returns the currently used fill color.
            virtual QColor fillColor() const {
                return _brush.color();
            }

            /// setConfidence defines the confidence level for gaussian representation.
            virtual void setConfidence(qreal confidence) {
                _confidence = confidence;
            }

            /// confidence returns the currently used confidence level.
            virtual qreal confidence() const {
                return _confidence;
            }

            /// promoteBlob adds a blob to the display or shows a hidden blob.
            template <typename Blob>
            void promoteBlob(std::size_t id, Blob blob) {
                while (_accessingBlobs.test_and_set(std::memory_order_acquire)) {}
                const auto idAndBlobAndIsVisibleCandidate = _blobAndIsVisibleById.find(id);
                if (idAndBlobAndIsVisibleCandidate == _blobAndIsVisibleById.end()) {
                    _blobAndIsVisibleById.insert(std::make_pair(
                        id,
                        std::make_pair(
                            ProtectedBlob{blob.x, blob.y, blob.squaredSigmaX, blob.sigmaXY, blob.squaredSigmaY},
                            true
                        )
                    ));
                } else {
                    idAndBlobAndIsVisibleCandidate->second.first.x = blob.x;
                    idAndBlobAndIsVisibleCandidate->second.first.y = blob.y;
                    idAndBlobAndIsVisibleCandidate->second.first.squaredSigmaX = blob.squaredSigmaX;
                    idAndBlobAndIsVisibleCandidate->second.first.sigmaXY = blob.sigmaXY;
                    idAndBlobAndIsVisibleCandidate->second.first.squaredSigmaY = blob.squaredSigmaY;
                    idAndBlobAndIsVisibleCandidate->second.second = true;
                }
                _accessingBlobs.clear(std::memory_order_release);
            }

            /// updateBlob changes a visible blob.
            template <typename Blob>
            void updateBlob(std::size_t id, Blob blob) {
                while (_accessingBlobs.test_and_set(std::memory_order_acquire)) {}
                const auto idAndBlobAndIsVisible = _blobAndIsVisibleById.find(id);
                idAndBlobAndIsVisible->second.first.x = blob.x;
                idAndBlobAndIsVisible->second.first.y = blob.y;
                idAndBlobAndIsVisible->second.first.squaredSigmaX = blob.squaredSigmaX;
                idAndBlobAndIsVisible->second.first.sigmaXY = blob.sigmaXY;
                idAndBlobAndIsVisible->second.first.squaredSigmaY = blob.squaredSigmaY;
                _accessingBlobs.clear(std::memory_order_release);
            }

            /// demoteBlob hides a blob while keeping its data.
            template <typename Blob>
            void demoteBlob(std::size_t id, Blob blob) {
                while (_accessingBlobs.test_and_set(std::memory_order_acquire)) {}
                const auto idAndBlobAndIsVisible = _blobAndIsVisibleById.find(id);
                idAndBlobAndIsVisible->second.first.x = blob.x;
                idAndBlobAndIsVisible->second.first.y = blob.y;
                idAndBlobAndIsVisible->second.first.squaredSigmaX = blob.squaredSigmaX;
                idAndBlobAndIsVisible->second.first.sigmaXY = blob.sigmaXY;
                idAndBlobAndIsVisible->second.first.squaredSigmaY = blob.squaredSigmaY;
                idAndBlobAndIsVisible->second.second = false;
                _accessingBlobs.clear(std::memory_order_release);
            }

            /// deleteBlob removes a blob from the display.
            template <typename Blob>
            void deleteBlob(std::size_t id, Blob) {
                while (_accessingBlobs.test_and_set(std::memory_order_acquire)) {}
                _blobAndIsVisibleById.erase(id);
                _accessingBlobs.clear(std::memory_order_release);
            }

            /// paint is called by the render thread when drawing is required.
            virtual void paint(QPainter* painter) {
                painter->setPen(_pen);
                painter->setBrush(_brush);
                painter->setRenderHint(QPainter::Antialiasing);

                while (_accessingBlobs.test_and_set(std::memory_order_acquire)) {}
                for (auto idAndBlobAndIsVisible : _blobAndIsVisibleById) {
                    if (idAndBlobAndIsVisible.second.second) {
                        painter->resetTransform();
                        painter->setWindow(0, 0, _canvasSize.width(), _canvasSize.height());
                        painter->translate(idAndBlobAndIsVisible.second.first.x, _canvasSize.height() - 1 - idAndBlobAndIsVisible.second.first.y);
                        const auto ellipse = ellipseFromBlob(idAndBlobAndIsVisible.second.first, _confidence);
                        painter->rotate(-std::get<2>(ellipse) / M_PI * 180);
                        painter->drawEllipse(QPointF(), std::get<0>(ellipse), std::get<1>(ellipse));
                    }
                }
                _accessingBlobs.clear(std::memory_order_release);
            }

        protected:

            /// ProtectedBlob represents a tracked gaussian blob.
            struct ProtectedBlob {
                double x;
                double y;
                double squaredSigmaX;
                double sigmaXY;
                double squaredSigmaY;
            };

            /// ellipseFromBlob calculates an ellipse parameters from a blob.
            /// The returned array contains a, b and angle (in that order), where:
            ///     - a is the major radius
            ///     - b is the minor radius
            ///     - angle is the angle between the horizontal axis and the major axis
            std::array<double, 3> ellipseFromBlob(ProtectedBlob blob, double confidence) {
                const auto deltaSquareRoot = std::sqrt(std::pow(blob.squaredSigmaX - blob.squaredSigmaY, 2) + 4 * std::pow(blob.sigmaXY, 2)) / 2;
                const auto firstOrderCoefficient = (blob.squaredSigmaX + blob.squaredSigmaY) / 2;
                return std::array<double, 3>{{
                    confidence * std::sqrt(firstOrderCoefficient + deltaSquareRoot),
                    confidence * std::sqrt(firstOrderCoefficient - deltaSquareRoot),
                    blob.squaredSigmaY == blob.squaredSigmaX ? M_PI / 4
                    : (
                        std::atan(2 * blob.sigmaXY / (blob.squaredSigmaX - blob.squaredSigmaY))
                        + (blob.squaredSigmaY > blob.squaredSigmaX ? M_PI : 0)
                    ) / 2,
                }};
            }

            QSize _canvasSize;
            QPen _pen;
            QBrush _brush;
            qreal _confidence;
            std::unordered_map<std::size_t, std::pair<ProtectedBlob, bool>> _blobAndIsVisibleById;
            std::atomic_flag _accessingBlobs;
    };
}
