#include "bench.h"
#include "entity/unit.h"

// 初始化固定槽位容量（默认 8 格）。
Bench::Bench(int capacity)
    : m_slots(capacity > 0 ? capacity : 8, nullptr)
{}

// 将单位放入指定备战区槽位。
bool Bench::addUnit(Unit* unit, int slot)
{
    if (!unit || !isValidSlot(slot) || m_slots[slot] != nullptr || m_unitToSlot.contains(unit)) {
        return false;
    }

    m_slots[slot] = unit;
    m_unitToSlot[unit] = slot;
    unit->setPosition(QPoint(-1, -1));
    return true;
}

// 将单位从备战区槽位移除。
bool Bench::removeUnit(Unit* unit)
{
    if (!unit || !m_unitToSlot.contains(unit)) {
        return false;
    }

    const int slot = m_unitToSlot.value(unit);
    if (isValidSlot(slot)) {
        m_slots[slot] = nullptr;
    }
    m_unitToSlot.remove(unit);
    return true;
}

// 支持备战区内部槽位重排（含与目标槽位交换）。
bool Bench::moveWithin(int fromSlot, int toSlot)
{
    if (!isValidSlot(fromSlot) || !isValidSlot(toSlot) || fromSlot == toSlot) {
        return false;
    }

    Unit* fromUnit = m_slots[fromSlot];
    Unit* toUnit = m_slots[toSlot];
    if (!fromUnit) {
        return false;
    }

    m_slots[fromSlot] = toUnit;
    m_slots[toSlot] = fromUnit;

    m_unitToSlot[fromUnit] = toSlot;
    if (toUnit) {
        m_unitToSlot[toUnit] = fromSlot;
    }
    return true;
}

// 备战区展示与查询：按槽位读取单位指针。
Unit* Bench::unitAt(int slot) const
{
    if (!isValidSlot(slot)) {
        return nullptr;
    }
    return m_slots[slot];
}

// 备战区展示与查询：判断指定槽位是否已有单位。
bool Bench::hasUnitAt(int slot) const
{
    return unitAt(slot) != nullptr;
}

// 备战区与棋盘之间的数据同步：判断单位当前是否位于备战区。
bool Bench::contains(Unit* unit) const
{
    return unit && m_unitToSlot.contains(unit);
}

// 返回指定单位在备战区内的槽位索引。
int Bench::slotOf(Unit* unit) const
{
    if (!unit || !m_unitToSlot.contains(unit)) {
        return -1;
    }
    return m_unitToSlot.value(unit);
}

// 返回备战区总槽位数。
int Bench::capacity() const
{
    return m_slots.size();
}

// 备战区落位逻辑：查找第一个空闲槽位用于自动放置。
int Bench::firstEmptySlot() const
{
    for (int i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i]) {
            return i;
        }
    }
    return -1;
}

// 备战区状态重置：清空所有槽位与单位到槽位映射。
void Bench::clear()
{
    m_slots.fill(nullptr);
    m_unitToSlot.clear();
}

// 备战区占用规则：校验槽位索引是否合法。
bool Bench::isValidSlot(int slot) const
{
    return slot >= 0 && slot < m_slots.size();
}
