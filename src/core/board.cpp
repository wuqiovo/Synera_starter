#include "board.h"

// 创建
Board::Board()
    : m_cells(ROWS * COLS, nullptr)
{}

// 试图将一个单位添加到格子中，如果无法添加，静默失败
// 补充检测当前单位是否已经部署在棋盘上
bool Board::addUnit(Unit* unit, const QPoint& pos) {
    const int idx = indexOf(pos);
    if (!unit || idx < 0 || m_cells[idx] 
        || m_unitToPosition.contains(unit)) {
        return false;
    }

    m_cells[idx] = unit;
    m_unitToPosition[unit] = pos;
    unit->setPosition(pos);
    return true;
}

// 将单位（如果确实在棋盘上）从棋盘上移出，并不delete
bool Board::removeUnit(Unit* unit) {
    if (!unit || !m_unitToPosition.contains(unit)) {
        return false;
    }

    const int idx = indexOf(m_unitToPosition.value(unit));
    if (idx >= 0) {
        m_cells[idx] = nullptr;
    }
    m_unitToPosition.remove(unit);
    unit->setPosition(QPoint(-1, -1)); // 避免外部误判
    return true;
}

// 新增移动单位函数
bool Board::moveUnit(Unit* unit, const QPoint& targetPos){
    const int tar_idx = indexOf(targetPos);
    if (!unit || !m_unitToPosition.contains(unit)
        || m_cells[tar_idx] != nullptr || tar_idx < 0) 
    {
        return false;
    }
    QPoint prevPos = m_unitToPosition[unit];
    const int prev_idx = indexOf(prevPos);
    m_cells[prev_idx] = nullptr;
    m_cells[tar_idx] = unit;
    m_unitToPosition[unit] = targetPos;
    unit->setPosition(targetPos);
    return true;
}

// 如果有，返回Unit指针；如果没有，返回nullptr
Unit* Board::getUnitAt(const QPoint& pos) const {
    const int idx = indexOf(pos);
    return idx < 0 ? nullptr : m_cells[idx];
}

// 复用上述函数判断某个坐标是否有单位
bool Board::hasUnitAt(const QPoint& pos) const {
    return getUnitAt(pos) != nullptr;
}

// 判断坐标是否合法
bool Board::isValidPosition(const QPoint& pos) const {
    return pos.x() >= 0 && pos.x() < COLS && pos.y() >= 0 && pos.y() < ROWS;
}

// 棋盘分上下半区，玩家在下半区，敌人在上半区
bool Board::isPlayerHalf(const QPoint& pos) const {
    return pos.y() >= ROWS / 2;
}

// 清空棋盘
void Board::clear() {
    std::fill(m_cells.begin(), m_cells.end(), nullptr);
    m_unitToPosition.clear();
}

// 将QPoint转换为QVector中对应的下标
int Board::indexOf(const QPoint& pos) const {
    if (!isValidPosition(pos)) {
        return -1;
    }
    return pos.y() * COLS + pos.x();
}
