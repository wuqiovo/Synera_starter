#ifndef BENCH_H
#define BENCH_H

#include <QHash>
#include <QVector>

class Unit;

class Bench
{
public:
    explicit Bench(int capacity = 8);
    ~Bench() = default;

    bool addUnit(Unit* unit, int slot);
    bool removeUnit(Unit* unit);
    bool moveWithin(int fromSlot, int toSlot);

    Unit* unitAt(int slot) const;
    bool hasUnitAt(int slot) const;
    bool contains(Unit* unit) const;
    int slotOf(Unit* unit) const;

    int capacity() const;
    int firstEmptySlot() const;

    void clear();

private:
    bool isValidSlot(int slot) const;

    // 备战区槽位容器（按索引存放 Unit*，nullptr 表示空槽）。
    QVector<Unit*> m_slots;
    // 单位到槽位的反向索引，用于快速定位某个单位所在的槽位索引。
    QHash<Unit*, int> m_unitToSlot;
};

#endif // BENCH_H
