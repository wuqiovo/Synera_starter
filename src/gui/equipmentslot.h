#ifndef EQUIPMENTSLOT_H
#define EQUIPMENTSLOT_H

#include <QGraphicsObject>

class Equipment;

class EquipmentSlotItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit EquipmentSlotItem(int slotIndex, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void setEquipment(Equipment* eq);
    Equipment* equipment() const { return m_eq; }

signals:
    void discardClicked(int slotIndex);
    void dragStarted(int slotIndex, QPointF scenePos);
    void dragMoved(int slotIndex, QPointF scenePos);
    void dragDropped(int slotIndex, QPointF scenePos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    int m_slotIndex;
    Equipment* m_eq;
    bool m_dragging;
    QRectF m_discardRect;
};

#endif // EQUIPMENTSLOT_H
