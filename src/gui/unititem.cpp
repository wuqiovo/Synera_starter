#include "gui/unititem.h"
#include "entity/unit.h"
#include <QCoreApplication>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QFileInfo>

UnitItem::UnitItem(Unit* unit, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_unit(unit)
    , m_gridPos(-1, -1)
    , m_benchSlot(-1)
    , m_dragging(false)
    , m_active(false)
    , m_spriteTried(false)
{
    setAcceptedMouseButtons(Qt::LeftButton);
}

QRectF UnitItem::boundingRect() const
{
    return QRectF(-45, -62, 90, 112);
}

void UnitItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    if (m_unit && m_unit->isStunned()) {
        QPen haloPen(QColor(230, 70, 70));
        haloPen.setWidthF(2.5);
        painter->setPen(haloPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QRectF(-34, -34, 68, 68));
    } else if (m_unit && m_unit->hasDamageOutputReduction()) {
        QPen haloPen(QColor(80, 200, 120));
        haloPen.setWidthF(2.5);
        painter->setPen(haloPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QRectF(-34, -34, 68, 68));
    } else if (m_active) {
        QPen haloPen(QColor(255, 210, 80));
        haloPen.setWidthF(2.5);
        painter->setPen(haloPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QRectF(-34, -34, 68, 68));
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(20, 20, 20, 110));
    painter->drawEllipse(QRectF(-14, 8, 28, 10));

    QPolygonF badge;
    badge << QPointF(0, -15)
          << QPointF(13, -7)
          << QPointF(13, 7)
          << QPointF(0, 15)
          << QPointF(-13, 7)
          << QPointF(-13, -7);

    painter->setPen(QPen(QColor(18, 18, 18), 1.5));
    painter->setBrush(QColor(100, 150, 200));
    painter->drawPolygon(badge);

    if (m_unit) {
        QString label = QStringLiteral("?");
        switch (m_unit->trait()) {
        case Unit::Trait::Warrior:
            label = QString::fromUtf8("战");
            break;
        case Unit::Trait::Archer:
            label = QString::fromUtf8("弓");
            break;
        case Unit::Trait::Mage:
            label = QString::fromUtf8("法");
            break;
        case Unit::Trait::Boss:
            label = QString::fromUtf8("王");
            break;
        default:
            break;
        }

        painter->setPen(Qt::white);
        QFont font = painter->font();
        font.setPointSize(12);
        font.setBold(true);
        painter->setFont(font);
        painter->drawText(QRectF(-13, -13, 26, 26), Qt::AlignCenter, label);
    }

    if (m_unit) {
        const QString hpText = QString("%1/%2").arg(m_unit->hp()).arg(m_unit->maxHp());
        QFont hpFont = painter->font();
        hpFont.setPointSize(9);
        hpFont.setBold(true);
        painter->setFont(hpFont);
        if (m_unit->owner() == Unit::Owner::EnemyCtrl) {
            painter->setPen(QColor(230, 80, 80));
        } else {
            painter->setPen(QColor(90, 170, 255));
        }
        painter->drawText(QRectF(-32, -58, 64, 12), Qt::AlignCenter, hpText);

        if (m_unit->trait() == Unit::Trait::Mage || m_unit->maxMana() > 0) {
            const QString manaText = QString("%1/%2").arg(m_unit->mana()).arg(m_unit->maxMana());
            QFont manaFont = painter->font();
            manaFont.setPointSize(8);
            painter->setFont(manaFont);
            painter->setPen(QColor(120, 210, 240));
            painter->drawText(QRectF(-32, -46, 64, 12), Qt::AlignCenter, manaText);
        }

        // 在单位蓝条下方显示 Atk
        const QString atkText = QString("Atk: %1").arg(m_unit->atk());
        QFont atkFont = painter->font();
        atkFont.setPointSize(8);
        atkFont.setBold(true);
        painter->setFont(atkFont);
        painter->setPen(QColor(255, 215, 0));
        painter->drawText(QRectF(-32, -34, 64, 12), Qt::AlignCenter, atkText);

        // 在图标下方显示 Lv.x
        const QString levelText = QString("Lv. %1").arg(m_unit->level());
        QFont levelFont = painter->font();
        levelFont.setPointSize(8);
        levelFont.setBold(true);
        painter->setFont(levelFont);
        painter->setPen(QColor(255, 215, 0));
        painter->drawText(QRectF(-32, 24, 64, 12), Qt::AlignCenter, levelText);
    }
}

void UnitItem::ensureSpriteLoaded() const
{
    m_spriteTried = true;
}

QString UnitItem::spriteRelativePathForUnit() const
{
    return QString();
}

int UnitItem::unitId() const
{
    return m_unit ? m_unit->id() : -1;
}

void UnitItem::setGridPos(const QPoint& gridPos)
{
    m_gridPos = gridPos;
}

// 记录单位所在的备战区槽位。
void UnitItem::setBenchSlot(int slot)
{
    m_benchSlot = slot;
}

void UnitItem::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    update();
}

void UnitItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsObject::mousePressEvent(event);
        return;
    }

    m_dragging = true;
    emit dragStarted(unitId(), m_gridPos, event->scenePos());
    event->accept();
}

void UnitItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_dragging) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    emit dragMoved(unitId(), m_gridPos, event->scenePos());
    event->accept();
}

void UnitItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_dragging || event->button() != Qt::LeftButton) {
        QGraphicsObject::mouseReleaseEvent(event);
        return;
    }

    m_dragging = false;
    emit dragDropped(unitId(), m_gridPos, event->scenePos());
    event->accept();
}
