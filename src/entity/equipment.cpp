#include "equipment.h"

Equipment::Equipment(Type type)
    : m_type(type)
    , m_bonusHp(0)
    , m_bonusAtk(0)
    , m_bonusRange(0)
    , m_bonusMaxMana(0)
{
    applyStats();
}

void Equipment::setType(Type type)
{
    m_type = type;
    applyStats();
}

void Equipment::applyStats()
{
    m_bonusHp = 0;
    m_bonusAtk = 0;
    m_bonusRange = 0;
    m_bonusMaxMana = 0;

    switch (m_type) {
    case Type::Sword:
        m_bonusAtk = 3;
        break;
    case Type::Crystal:
        m_bonusMaxMana = -20;
        break;
    case Type::armor:
        m_bonusHp = 30;
        break;
    case Type::Gloves:
        // 手套加速行动，在单位逻辑中处理
        break;
    case Type::None:
    default:
        break;
    }
}

