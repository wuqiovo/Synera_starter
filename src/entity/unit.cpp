#include "unit.h"
#include "core/game.h"
#include <QList>

int Unit::s_nextId = 0;

Unit::Unit(const QString& name, Trait trait, int hp, int atk)
    : m_id(s_nextId++)
    , m_name(name)
    , m_position(0, 0)
    , m_hp(100)      // 默认 HP（可按需调整）
    , m_maxHp(100)   // 默认最大 HP
    , m_atk(10)      // 默认 ATK
    , m_range(1)     // 默认攻击距离
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
{
    if (hp >= 0) {
        m_hp = hp;
        m_maxHp = hp;
    }
    if (atk >= 0) {
        m_atk = atk;
    }
}

void Unit::act(Game* game)
{
    // 默认行为：根据状态执行相应的逻辑
}

void Unit::upgrade()
{
    // 默认升级行为
    ++m_level;
    m_maxHp *= 1.5;
    m_atk *= 1.5;
    m_hp = m_maxHp; // 升级后恢复 HP
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

int Unit::distanceTo(const Unit* other) const
{
    if (!other) {
        return -1;
    }

    int q1 = this->position().x() - 
            (this->position().y() - (this->position().y() & 1)) / 2;
    int r1 = this->position().y();
    int s1 = -q1 - r1;

    int q2 = other->position().x() - 
            (other->position().y() - (other->position().y() & 1)) / 2;
    int r2 = other->position().y();
    int s2 = -q2 - r2;
    
    return (std::abs(q2 - q1) + std::abs(r2 - r1) + std::abs(s2 - s1)) / 2;
}

void Unit::moveTowardsTarget(Game* game, const Unit* target)
{
    if (!target) {
        return;
    }

    // 简单的移动逻辑：向目标单位的方向移动一步
    int dx = target->position().x() - this->position().x();
    int dy = target->position().y() - this->position().y();

    QPoint nextPos = position();
    if (std::abs(dx) > std::abs(dy)) {
        nextPos = QPoint(position().x() + (dx > 0 ? 1 : -1), position().y());
    } else if (dy != 0) {
        nextPos = QPoint(position().x(), position().y() + (dy > 0 ? 1 : -1));
    }

    if (game) {
        game->moveUnitDuringBattle(this, nextPos);
    } else {
        setPosition(nextPos);
    }
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
    if (m_mana > m_maxMana) {
        m_mana = m_maxMana;
    }
}

bool Unit::prepareForAct(Game* game)
{
    // 预处理逻辑：在每轮行动前执行的准备工作
    if (m_hp <= 0 && m_status != Status::Dead) {
        setStatus(Status::Dead);
    }

    if (m_damageOutputReductionTurns > 0) {
        --m_damageOutputReductionTurns;
        if (m_damageOutputReductionTurns == 0) {
            m_damageOutputReductionRatio = 0.0f;
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

    if (m_status == Status::Casting && m_mana < m_maxMana) {
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
    if (m_mana > m_maxMana) {
        m_mana = m_maxMana;
    }
    /* 改为在pre阶段寻找目标
       每次行动之前自动寻找目标
    if (!m_target || m_target->status() == Status::Dead) {
        m_target = findTarget(game);
    }
    */
    
    if (m_target && distanceTo(m_target) > range()) {
        setStatus(Status::Moving);
    }
    else if (m_target && m_mana == m_maxMana) {
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
    else if (m_target && m_mana == m_maxMana) {
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
    else if (m_target && m_mana == m_maxMana) {
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
    setHp(m_hp - damage);
    if (m_hp <= 0) {
        setStatus(Status::Dead);
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
    setHp(1000); // 调试高Hp
}

void Warrior::act(Game* game)
{
    // 战士特有的行为可以在这里实现
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
        // 留给战士特殊技能
        skill(game);
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
    setHp(1000); // 调试高Hp
}

void Mage::act(Game* game)
{
    // 法师特有的行为可以在这里实现
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
        // 有待补充：攻击动画、技能效果等
        if (m_target) {
            attackTarget(m_target);
            resolveAttack(game);
        } else {
            setStatus(Status::Idle);    
        }
        break;
    case Status::Casting:
        // 满蓝时施放特殊技能
        skill(game);
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

void Mage::skill(Game* game)
{
    // 法师的特殊技能实现：目标周围一格内所有敌人
    if (!game || !m_target) {
        return;
    }
    if (!game->isUnitOnBoard(m_target) || m_target->status() == Status::Dead) {
        return;
    }
    const QList<Unit*>& units = game->units();
    const int damage = adjustDamageOutput(20);
    for (Unit* unit : units) {
        if (!unit || unit->owner() == owner() || unit->status() == Status::Dead) {
            continue;
        }
        if (!game->isUnitOnBoard(unit)) {
            continue;
        }
        const int distance = std::abs(unit->position().x() - m_target->position().x()) +
                             std::abs(unit->position().y() - m_target->position().y());
        if (distance <= 1) {
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
    setHp(1000); // 调试高Hp
}

void Archer::act(Game* game)
{
    // 弓箭手特有的行为可以在这里实现
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
            break;
        }        
        break;
    case Status::Casting:
        // 满蓝时施放特殊技能
        skill(game);
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