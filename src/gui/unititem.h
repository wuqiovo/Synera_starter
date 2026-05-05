#ifndef GUI_ITEMS_UNITITEM_H
#define GUI_ITEMS_UNITITEM_H

#include <QGraphicsObject>
#include <QPoint>
#include <QPixmap>

class Unit;

class UnitItem : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit UnitItem(Unit* unit, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    Unit* unit() const { return m_unit; }
    int unitId() const;

    void setGridPos(const QPoint& gridPos);
    QPoint gridPos() const { return m_gridPos; }
    void setBenchSlot(int slot);
    int benchSlot() const { return m_benchSlot; }
    bool isOnBench() const { return m_benchSlot >= 0; }
    void setActive(bool active);
    bool isActive() const { return m_active; }

signals:
    void dragStarted(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void dragMoved(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void dragDropped(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    void ensureSpriteLoaded() const;
    QString spriteRelativePathForUnit() const;

    Unit* m_unit;
    QPoint m_gridPos;
    // 所在备战区槽位，-1 表示未在备战区。
    int m_benchSlot;
    bool m_dragging;
    bool m_active;
    // 缓存单位图像，避免重复加载。mutable 以允许在 const 方法中加载。
    mutable QPixmap m_sprite;
    mutable bool m_spriteTried;
};

#endif // GUI_ITEMS_UNITITEM_H
