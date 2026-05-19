#include "equipmentslot.h"
#include "entity/equipment.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QApplication>

EquipmentSlotItem::EquipmentSlotItem(int slotIndex, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_slotIndex(slotIndex)
    , m_eq(nullptr)
    , m_dragging(false)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    m_discardRect = QRectF(65, 5, 20, 20); // 丢弃按钮矩形
}

QRectF EquipmentSlotItem::boundingRect() const
{
    return QRectF(0, 0, 90, 110);
}

void EquipmentSlotItem::setEquipment(Equipment* eq)
{
    m_eq = eq;
    update();
}

void EquipmentSlotItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing);

    // 背景
    painter->setBrush(QColor(60, 60, 80, 200));
    painter->setPen(QPen(QColor(90, 90, 122), 2));
    painter->drawRoundedRect(0, 0, 90, 110, 5, 5);

    if (m_eq) {
        // 绘制装备名称和效果
        QString name;
        QString effect;
        switch (m_eq->type()) {
        case Equipment::Type::Sword:
            name = QString::fromUtf8("剑");
            effect = "ATK +3";
            break;
        case Equipment::Type::Crystal:
            name = QString::fromUtf8("水晶");
            effect = "Mana -20";
            break;
        case Equipment::Type::armor:
            name = QString::fromUtf8("盔甲");
            effect = "HP +30";
            break;
        case Equipment::Type::Gloves:
            name = QString::fromUtf8("手套");
            effect = QString::fromUtf8("双倍行动");
            break;
        default:
            name = "Unknown";
            effect = "";
            break;
        }

        painter->setPen(Qt::white);
        QFont f = painter->font();
        f.setPointSize(12);
        f.setBold(true);
        painter->setFont(f);
        painter->drawText(QRectF(0, 25, 90, 25), Qt::AlignCenter, name);

        f.setPointSize(9);
        f.setBold(false);
        painter->setFont(f);
        painter->setPen(QColor(200, 200, 200));
        painter->drawText(QRectF(0, 55, 90, 45), Qt::AlignCenter | Qt::TextWordWrap, effect);

        // 绘制丢弃按钮
        painter->setBrush(QColor(180, 50, 50));
        painter->setPen(QPen(QColor(200, 100, 100), 1));
        painter->drawRoundedRect(m_discardRect, 3, 3);
        painter->setPen(Qt::white);
        f.setPointSize(10);
        f.setBold(true);
        painter->setFont(f);
        painter->drawText(m_discardRect, Qt::AlignCenter, "X");
    }
}

void EquipmentSlotItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_eq) {
        if (m_discardRect.contains(event->pos())) {
            emit discardClicked(m_slotIndex);
            event->accept();
            return;
        }
        m_dragging = true;
        emit dragStarted(m_slotIndex, event->scenePos());
        event->accept();
    } else {
        QGraphicsObject::mousePressEvent(event);
    }
}

void EquipmentSlotItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_dragging) {
        emit dragMoved(m_slotIndex, event->scenePos());
        event->accept();
    } else {
        QGraphicsObject::mouseMoveEvent(event);
    }
}

void EquipmentSlotItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        emit dragDropped(m_slotIndex, event->scenePos());
        event->accept();
    } else {
        QGraphicsObject::mouseReleaseEvent(event);
    }
}
