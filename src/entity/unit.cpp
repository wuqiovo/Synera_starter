#include "unit.h"
#include "core/game.h"
#include "core/board.h"
#include "equipment.h"
#include <QList>
#include <QHash>
#include <QQueue>
#include <QSet>

int Unit::s_nextId = 0;

Unit::Unit(const QString& name, Trait trait, int hp, int atk)
    : m_id(s_nextId++)
    , m_name(name)
    , m_position(0, 0)
    , m_hp(100)      // 默认 HP（可按需调整）
    , m_maxHp(100)   // 默认最大 HP
    , m_bonusMaxHp(0)
    , m_atk(10)      // 默认 ATK
    , m_bonusAtk(0)
    , m_range(1)     // 默认攻击距离
    , m_bonusRange(0)
    , m_maxMana(50)   // 默认最大法力
    , m_mana(50)      // 默认当前法力
    , m_level(1)     // 默认等级
    , m_trait(trait)
    , m_status(Status::Idle)
    , m_owner(Owner::PlayerCtrl)
    , m_target(nullptr)
    , m_stunTurns(0)
    , m_damageOutputReductionTurns(0)
    , m_damageOutputReductionRatio(0.0f)
    , m_equipment(nullptr)
{
    if (hp >= 0) {
        m_hp = hp;
        m_maxHp = hp;
    }
    if (atk >= 0) {
        m_atk = atk;
    }
}

int Unit::maxHp() const { 
    return m_maxHp + m_bonusMaxHp + (m_equipment ? m_equipment->bonusHp() : 0); 
}

int Unit::atk() const { 
    return m_atk + m_bonusAtk + (m_equipment ? m_equipment->bonusAtk() : 0); 
}

int Unit::range() const { 
    return m_range + m_bonusRange + (m_equipment ? m_equipment->bonusRange() : 0); 
}

int Unit::maxMana() const { 
    int m = m_maxMana + (m_equipment ? m_equipment->bonusMaxMana() : 0);
    return m > 0 ? m : 1; 
}

void Unit::act(Game* game)
{
    if (!prepareForAct(game)) {
        return;
    }

    if (!game || !game->isUnitOnBoard(this)) {
        return;
    }
    
    switch (m_status) {
    case Status::Idle:
        // 在空闲状态下寻找目标
        // 不break，继续执行后续状态逻辑
        normalIdleBehavior(game);
    case Status::Moving:
        // 移动中可能更新位置、检查路径等
        if (!m_target) {
            setStatus(Status::Idle);
            break;
        }
        normalMoveBehavior(game);
        break;
    case Status::Attacking:
        // 攻击中可能执行攻击逻辑、计算伤害等
        if (m_target) {
            attackTarget(m_target);
            resolveAttack(game);
        } else {
            setStatus(Status::Idle);    
        }        
        break;
    case Status::Casting:
        // 技能
        skill(game);
        resolveAttack(game);
        setMana(0);
        break;
    case Status::Dead:
        // 已死亡状态下可能播放死亡动画、移除单位等
        if (game) {
            game->requestRemoveUnit(this);
        }
        break;
    }
}

void Unit::upgrade()
{
    // 默认升级行为：属性提升 1.6 倍，下取整
    ++m_level;
    m_maxHp = static_cast<int>(m_maxHp * 1.6);
    m_atk   = static_cast<int>(m_atk   * 1.6);
    m_hp = maxHp(); // 升级后恢复 HP（含装备加成）
    // 升级后确保 mana 不超过 maxMana（装备可能降低 maxMana）。
    if (m_mana > maxMana()) {
        m_mana = maxMana();
    }
}

void Unit::skill(Game* game)
{
    // 默认技能行为（可被子类重写）
}

Unit* Unit::findTarget(Game* game) const
{
    if (!game) {
        return nullptr;
    }
    // 简单的目标选择逻辑：寻找最近的敌方单位（曼哈顿距离）
    Unit* closestTarget = nullptr;
    int closestDistance = std::numeric_limits<int>::max();

    const QList<Unit*>& units = game->units();
    for (Unit* unit : units) {
        if (!unit || unit->owner() == this->owner() || unit->status() == Status::Dead) {
            continue;
        }
        if (!game->isUnitOnBoard(unit)) {
            continue;
        }
        int distance = std::abs(unit->position().x() - this->position().x()) +
                        std::abs(unit->position().y() - this->position().y());
        if (distance < closestDistance) {
            closestDistance = distance;
            closestTarget = unit;
        }
    }
    return closestTarget;
}

namespace {
struct AxialCoord {
    int q;
    int r;
};

// 将偶数行右移布局的偏移坐标转换为轴向坐标。
AxialCoord toAxialCoord(const QPoint& pos)
{
    const int q = pos.x() - (pos.y() + (pos.y() & 1)) / 2;
    const int r = pos.y();
    return { q, r };
}

// 将轴向坐标转换为偶数行右移布局的偏移坐标。
QPoint toOffsetCoord(const AxialCoord& coord)
{
    const int x = coord.q + (coord.r + (coord.r & 1)) / 2;
    return QPoint(x, coord.r);
}

// 计算两个格子在六边形网格中的距离。
int hexDistance(const QPoint& a, const QPoint& b)
{
    const AxialCoord aAx = toAxialCoord(a);
    const AxialCoord bAx = toAxialCoord(b);
    const int s1 = -aAx.q - aAx.r;
    const int s2 = -bAx.q - bAx.r;
    return (std::abs(aAx.q - bAx.q) + std::abs(aAx.r - bAx.r) + std::abs(s1 - s2)) / 2;
}

// 获取六边形网格中一个格子的六个相邻格。
QVector<QPoint> hexNeighbors(const QPoint& pos)
{
    static const int kDirQ[6] = { 1, 1, 0, -1, -1, 0 };
    static const int kDirR[6] = { 0, -1, -1, 0, 1, 1 };
    QVector<QPoint> result;
    result.reserve(6);

    const AxialCoord axial = toAxialCoord(pos);
    for (int i = 0; i < 6; ++i) {
        const AxialCoord next{ axial.q + kDirQ[i], axial.r + kDirR[i] };
        result.append(toOffsetCoord(next));
    }
    return result;
}
}

int Unit::distanceTo(const Unit* other) const
{
    if (!other) {
        return -1;
    }

    return hexDistance(position(), other->position());
}

void Unit::moveTowardsTarget(Game* game, const Unit* target)
{

    if (!game || !target) {
        return;
    }

    // 定义起点与终点用于路径规划。
    const QPoint start = position();
    const QPoint goal = target->position();
    if (start == goal) {
        return;
    }

    // 棋盘边界检查，供 BFS 扩展使用。
    auto isValid = [](const QPoint& pos) {
        return pos.x() >= 0 && pos.x() < Board::COLS && pos.y() >= 0 && pos.y() < Board::ROWS;
    };

    // 收集已占用格子，作为障碍处理。
    QSet<QPoint> blocked;
    const QList<Unit*>& units = game->units();
    for (Unit* unit : units) {
        if (!unit || unit == this || unit->status() == Status::Dead) {
            continue;
        }
        if (game->isUnitOnBoard(unit)) {
            blocked.insert(unit->position());
        }
    }

    // 可通行检查：在边界内且未被占用。
    auto isPassable = [&](const QPoint& pos) {
        return isValid(pos) && !blocked.contains(pos);
    };

    // BFS 寻找一条到达“可攻击范围内格子”的最短路径。
    QQueue<QPoint> frontier;
    QSet<QPoint> visited;
    QHash<QPoint, QPoint> cameFrom;
    QHash<QPoint, int> stepsFromStart;

    // 以起点初始化 BFS 队列。
    frontier.enqueue(start);
    visited.insert(start);
    cameFrom.insert(start, start);
    stepsFromStart.insert(start, 0);

    // 记录可达的“最接近目标”的备选格子。
    QPoint bestCell = start;
    int bestDistance = hexDistance(start, goal);
    int bestSteps = 0;
    bool foundGoal = false;
    QPoint foundCell = start;

    // 标准 BFS 循环：按层扩展前沿。
    while (!frontier.isEmpty()) {
        const QPoint current = frontier.dequeue();
        const int distToTarget = hexDistance(current, goal);
        // 若无法进入攻击范围，则使用更近的格子作为备选。
        const int curSteps = stepsFromStart.value(current, 0);
        // 若距离更近则更新；若距离相同且当前是非起点，则允许替换避免原地停滞。
        if (distToTarget < bestDistance
            || (distToTarget == bestDistance && bestCell == start && current != start)
            || (distToTarget == bestDistance && curSteps < bestSteps)) {
            bestDistance = distToTarget;
            bestSteps = curSteps;
            bestCell = current;
        }

        // 一旦当前格子已在攻击范围内，提前停止。
        if (distToTarget <= range()) {
            foundGoal = true;
            foundCell = current;
            break;
        }

        // 扩展当前格子的六个相邻格。
        const QVector<QPoint> neighbors = hexNeighbors(current);
        for (const QPoint& next : neighbors) {
            // 跳过已访问或不可通行的格子。
            if (visited.contains(next) || !isPassable(next)) {
                continue;
            }
            visited.insert(next);
            cameFrom.insert(next, current);
            stepsFromStart.insert(next, stepsFromStart.value(current, 0) + 1);
            frontier.enqueue(next);
        }
    }

    // 选择可达目的地：优先进入攻击范围，否则选最近备选格。
    const QPoint destination = foundGoal ? foundCell : bestCell;
    if (destination == start) {
        return;
    }

    // 从目的地回溯一步，决定本回合的移动步。
    QPoint step = destination;
    while (cameFrom.contains(step) && cameFrom.value(step) != start) {
        step = cameFrom.value(step);
    }
    // 移动前的最终安全检查。
    if (step == start || !isPassable(step)) {
        return;
    }

    // 在棋盘上执行移动。
    game->moveUnitDuringBattle(this, step);
}

void Unit::attackTarget(Unit* target)
{
    if (!target) {
        return;
    }
    // 简单的攻击逻辑：对目标造成伤害
    int damage = adjustDamageOutput(atk());
    target->takeDamage(damage);
    m_mana += 10; // 攻击后回复法力
    if (m_mana > maxMana()) {
        m_mana = maxMana();
    }
}

bool Unit::prepareForAct(Game* game)
{
    // 预处理逻辑：在每轮行动前执行的准备工作
    if (m_damageOutputReductionTurns > 0) {
        --m_damageOutputReductionTurns;
        if (m_damageOutputReductionTurns == 0) {
            m_damageOutputReductionRatio = 0.0f;
        }
    }

    if (m_vunerableTurns > 0) {
        --m_vunerableTurns;
        if (m_vunerableTurns == 0) {
            m_vunerableDamageIncreaseRatio = 0.0f;
        }
    }

    if (m_status != Status::Dead && m_status != Status::Attacking && m_status != Status::Casting) {
        m_target = findTarget(game);
    }

    if (m_stunTurns > 0) {
        // 原来会将状态设置为Idle，但这会导致单位在被击晕时丢失当前状态
        // 改为保持原状态但跳过行动
        --m_stunTurns;
        return false;
    }

    if (m_status == Status::Casting && m_mana < maxMana()) {
        setStatus(Status::Idle);
    }
    
    // 在非死亡状态下检查目标状态，必要时重置目标和状态
    if (m_status != Status::Dead) {
        // 单位存活时优先寻找目标

        if (m_target && m_target->status() == Status::Dead) {
            m_target = nullptr;
            setStatus(Status::Idle);
        }

        if (!m_target && m_status != Status::Idle) {
            setStatus(Status::Idle);
        }

        if (m_target && distanceTo(m_target) > range()) {
            setStatus(Status::Moving);
        }

        return true;
    }

    return false;
}

void Unit::normalIdleBehavior(Game* game)
{
    if (!game || !game->isUnitOnBoard(this)) {
        return;
    }
    // 默认空闲行为：寻找目标并切换状态
    m_mana += 10; // 空闲状态回复法力
    if (m_mana > maxMana()) {
        m_mana = maxMana();
    }
    
    if (m_target && distanceTo(m_target) > range()) {
        setStatus(Status::Moving);
    }
    else if (m_target && m_mana >= maxMana()) {
        setStatus(Status::Casting);
    }
    else if (m_target) {
        setStatus(Status::Attacking);
    }
    else {
        // 无目标则保持空闲状态(结束当前行动)
        return;
    }
}

void Unit::normalMoveBehavior(Game* game)
{
    // 默认移动行为：向目标移动    
    if (distanceTo(m_target) > range()) {
        moveTowardsTarget(game, m_target);
    } 
    else if (m_target && m_mana >= maxMana()) {
        setStatus(Status::Casting);
    }
    else {
        setStatus(Status::Attacking);
    }
}

void Unit::resolveAttack(Game* game)
{
    if (m_target && m_target->status() == Status::Dead) {
        if (game) {
            game->requestRemoveUnit(m_target);
        }
        m_target = nullptr;
        setStatus(Status::Idle);
    }
    else if (m_target && m_mana >= maxMana()) {
        setStatus(Status::Casting);
    }
}

void Unit::setDamageOutputReduction(int turns, float ratio)
{
    m_damageOutputReductionTurns = turns > 0 ? turns : 0;
    m_damageOutputReductionRatio = ratio > 0.0f ? ratio : 0.0f;
}

int Unit::adjustDamageOutput(int damage) const
{
    if (m_damageOutputReductionRatio > 0.0f) {
        return static_cast<int>(damage * (1.0f - m_damageOutputReductionRatio));
    }
    return damage;
}

void Unit::takeDamage(int damage)
{
    if (m_vunerableDamageIncreaseRatio > 0.0f) {
        damage = static_cast<int>(damage * (1.0f + m_vunerableDamageIncreaseRatio));
    }
    setHp(m_hp - damage);
}

void Unit::setEquipment(Equipment* eq)
{
    // 卸下旧装备时，移除对应的 hp 加成（但不低于基础 m_maxHp），
    // 并将 mana 限制在卸下后的 maxMana 范围内。
    if (m_equipment) {
        int oldBonusHp = m_equipment->bonusHp();
        if (oldBonusHp > 0) {
            m_hp = qMax(m_hp - oldBonusHp, m_maxHp);
        }
    }

    m_equipment = eq;

    // 装上新装备时，同步增加当前 hp，
    // 并将 mana 限制在新的 maxMana 范围内。
    if (m_equipment) {
        int newBonusHp = m_equipment->bonusHp();
        if (newBonusHp > 0) {
            m_hp += newBonusHp;
        }
        // 水晶等装备会降低 maxMana，需要确保 mana 不超过新的上限。
        int newMaxMana = maxMana();
        if (m_mana > newMaxMana) {
            m_mana = newMaxMana;
        }
    }
}

Warrior::Warrior(const QString& name, int hp, int atk)
    : Unit(name, Trait::Warrior, hp, atk)
{
    // 战士特有的属性调整
    setRange(1);     // 战士近战攻击

    if (hp >= 0) {
        setHp(hp);
        setMaxHp(hp);
    }
    if (atk >= 0) {
        setAtk(atk);
    }
    // setHp(1000); // 调试高Hp
}

void Warrior::skill(Game* game)
{
    // 战士的特殊技能实现：单体击晕1回合
    if (!m_target || !game) {
        return;
    }
    if (!game->isUnitOnBoard(m_target)) {
        return;
    }
    m_target->setStunTurns(1);
}

Mage::Mage(const QString& name, int hp, int atk)
    : Unit(name, Trait::Mage, hp, atk)
{
    // 法师特有的属性调整
    setRange(3);     // 法师远程攻击
    setMaxMana(80); 
    setMana(80);    

    if (hp >= 0) {
        setHp(hp);
        setMaxHp(hp);
    }
    if (atk >= 0) {
        setAtk(atk);
    }
    // setHp(1000); // 调试高Hp
}

void Mage::skill(Game* game)
{
    // 法师的特殊技能实现：目标周围一格内所有敌人（达到3法师羁绊则为两格）
    if (!game || !m_target) {
        return;
    }
    if (!game->isUnitOnBoard(m_target) || m_target->status() == Status::Dead) {
        return;
    }
    
    int mageCount = game->traitCount(owner(), Trait::Mage);
    int skillDist = (mageCount >= 3) ? 2 : 1;
    
    const QList<Unit*>& units = game->units();
    const int damage = adjustDamageOutput(20);
    for (Unit* unit : units) {
        if (!unit || unit->owner() == owner() || unit->status() == Status::Dead) {
            continue;
        }
        if (!game->isUnitOnBoard(unit)) {
            continue;
        }
        const int distance = m_target->distanceTo(unit);
        if (distance <= skillDist) {
            unit->takeDamage(damage);
        }
    }
}

Archer::Archer(const QString& name, int hp, int atk)
    : Unit(name, Trait::Archer, hp, atk)
{
    // 弓箭手特有的属性调整
    setRange(2);     // 弓箭手中远程攻击

    if (hp >= 0) {
        setHp(hp);
        setMaxHp(hp);
    }
    if (atk >= 0) {
        setAtk(atk);
    }
    // setHp(1000); // 调试高Hp
}

void Archer::skill(Game* game)
{
    // 弓箭手的特殊技能实现：目标输出伤害降低
    if (!m_target || !game) {
        return;
    }
    if (!game->isUnitOnBoard(m_target)) {
        return;
    }
    m_target->setDamageOutputReduction(2, 0.2f);
}

Boss::Boss(const QString& name, int hp, int atk)
    : Unit(name, Trait::Boss, hp, atk)
{
    setRange(1);     // Boss近战攻击

    if (hp >= 0) {
        setHp(hp);
        setMaxHp(hp);
    }
    if (atk >= 0) {
        setAtk(atk);
    }
}

void Boss::skill(Game* game)
{
    // 为自己永久增加5点攻击力，并使目标陷入易伤状态（受伤增加30%）持续两回合
    if (!m_target || !game) {
        return;
    }
    if (!game->isUnitOnBoard(m_target)) {
        return;
    }

    setAtk(atk() + 10);
    m_target->setVunerableTurns(2);
    m_target->setVunerableDamageIncreaseRatio(0.3f);
}