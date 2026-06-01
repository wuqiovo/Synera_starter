#include "gui/unititem.h"
#include "entity/unit.h"
#include "entity/equipment.h"
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
    , m_eqDragging(false)
    , m_active(false)
    , m_eqRect(-24, 27, 48, 12)
    , m_spriteTried(false)
{
    setAcceptedMouseButtons(Qt::LeftButton);
}

QRectF UnitItem::boundingRect() const
{
    return QRectF(-38, -44, 76, 88);
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
    if (m_unit && m_unit->owner() == Unit::Owner::EnemyCtrl) {
        painter->setBrush(QColor(200, 70, 70));
    } else {
        painter->setBrush(QColor(100, 150, 200));
    }
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
        hpFont.setPointSize(7);
        hpFont.setBold(true);
        painter->setFont(hpFont);
        if (m_unit->owner() == Unit::Owner::EnemyCtrl) {
            painter->setPen(QColor(230, 80, 80));
        } else {
            painter->setPen(QColor(90, 170, 255));
        }
        painter->drawText(QRectF(-32, -40, 64, 10), Qt::AlignCenter, hpText);

        if (m_unit->trait() == Unit::Trait::Mage || m_unit->maxMana() > 0) {
            const QString manaText = QString("%1/%2").arg(m_unit->mana()).arg(m_unit->maxMana());
            QFont manaFont = painter->font();
            manaFont.setPointSize(7);
            painter->setFont(manaFont);
            painter->setPen(QColor(120, 210, 240));
            painter->drawText(QRectF(-32, -31, 64, 10), Qt::AlignCenter, manaText);
        }

        // 在单位蓝条下方显示 Atk
        const QString atkText = QString("Atk: %1").arg(m_unit->atk());
        QFont atkFont = painter->font();
        atkFont.setPointSize(7);
        atkFont.setBold(true);
        painter->setFont(atkFont);
        painter->setPen(QColor(255, 215, 0));
        painter->drawText(QRectF(-32, -22, 64, 10), Qt::AlignCenter, atkText);

        // 在图标下方显示 Lv.x
        const QString levelText = QString("Lv. %1").arg(m_unit->level());
        QFont levelFont = painter->font();
        levelFont.setPointSize(7);
        levelFont.setBold(true);
        painter->setFont(levelFont);
        painter->setPen(QColor(255, 215, 0));
        painter->drawText(QRectF(-32, 16, 64, 10), Qt::AlignCenter, levelText);
        
        // 如果有装备，显示在等级下方
        if (m_unit->owner() == Unit::Owner::PlayerCtrl && m_unit->equipment()) {
            QString eqName;
            switch (m_unit->equipment()->type()) {
                case Equipment::Type::Sword: eqName = QString::fromUtf8("剑"); break;
                case Equipment::Type::Crystal: eqName = QString::fromUtf8("水晶"); break;
                case Equipment::Type::armor: eqName = QString::fromUtf8("盔甲"); break;
                case Equipment::Type::Gloves: eqName = QString::fromUtf8("手套"); break;
                default: eqName = "Eq"; break;
            }
            QFont eqFont = painter->font();
            eqFont.setPointSize(6);
            painter->setFont(eqFont);
            painter->setPen(Qt::white);
            painter->setBrush(QColor(50, 50, 50, 180));
            QRectF bgRect(-24, 27, 48, 12);
            painter->drawRoundedRect(bgRect, 2, 2);
            painter->drawText(bgRect, Qt::AlignCenter, QString::fromUtf8("装备: ") + eqName);
        }
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

    // 如果点击位置在装备标签热区内，且单位有装备且属于玩家，则启动装备拖拽。
    if (m_unit && m_unit->owner() == Unit::Owner::PlayerCtrl
        && m_unit->equipment() && m_eqRect.contains(event->pos())) {
        m_eqDragging = true;
        emit eqDragStarted(unitId(), event->scenePos());
        event->accept();
        return;
    }

    m_dragging = true;
    emit dragStarted(unitId(), m_gridPos, event->scenePos());
    event->accept();
}

void UnitItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_eqDragging) {
        emit eqDragMoved(unitId(), event->scenePos());
        event->accept();
        return;
    }

    if (!m_dragging) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    emit dragMoved(unitId(), m_gridPos, event->scenePos());
    event->accept();
}

void UnitItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_eqDragging && event->button() == Qt::LeftButton) {
        m_eqDragging = false;
        emit eqDragDropped(unitId(), event->scenePos());
        event->accept();
        return;
    }

    if (!m_dragging || event->button() != Qt::LeftButton) {
        QGraphicsObject::mouseReleaseEvent(event);
        return;
    }

    m_dragging = false;
    emit dragDropped(unitId(), m_gridPos, event->scenePos());
    event->accept();
}
