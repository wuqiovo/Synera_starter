#ifndef BOARD_H
#define BOARD_H

#include <QHash>
#include <QPoint>
#include <QVector>
#include "entity/unit.h"

class Board
{
public:
    static constexpr int ROWS = 8;
    static constexpr int COLS = 8;

    Board();
    ~Board() = default;

    bool addUnit(Unit* unit, const QPoint& pos);
    bool removeUnit(Unit* unit);
    bool moveUnit(Unit* unit, const QPoint& targetPos);

    Unit* getUnitAt(const QPoint& pos) const;
    bool hasUnitAt(const QPoint& pos) const;

    bool isValidPosition(const QPoint& pos) const;
    bool isPlayerHalf(const QPoint& pos) const;

    void clear();

private:
    int indexOf(const QPoint& pos) const;

    // 棋盘格子容器，按行优先存储每个格子上的单位指针。
    QVector<Unit*> m_cells;

    // 单位到棋盘坐标的反向索引，用于快速定位单位当前位置。
    QHash<Unit*, QPoint> m_unitToPosition;
};

#endif // BOARD_H
