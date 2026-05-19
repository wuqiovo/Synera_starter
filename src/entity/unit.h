#ifndef UNIT_H
#define UNIT_H

#include <QPoint>
#include <QString>

class Game;

class Unit
{
public:
    enum class Owner {
        PlayerCtrl,
        EnemyCtrl
    };

    // 羁绊列表
    enum class Trait {
        None,
        Warrior, // 战士
        Mage,    // 法师
        Archer,  // 弓箭手
        Boss,    // Boss
        // 后续可扩展更多羁绊类型
    };

    // 单位状态集合
    enum class Status {
        Idle,   // 空闲
        Moving, // 移动中
        Attacking, // 攻击中
        Casting, // 施法中
        Dead // 已死亡
    };

    explicit Unit(const QString& name = QString("Unit")
        , Trait trait = Trait::None
        , int hp = -1
        , int atk = -1);
    virtual ~Unit() = default;

    static int getNextId() { return s_nextId; }
    static void setNextId(int id) { s_nextId = id; }

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    QString name() const { return m_name; }
    QPoint position() const { return m_position; }

    void setName(const QString& name) { m_name = name; }
    void setPosition(const QPoint& pos) { m_position = pos; }

    int hp() const { return m_hp; }
    void setHp(int hp) { m_hp = hp > 0 ? hp : 0; }
    int maxHp() const;
    void setMaxHp(int maxHp) { m_maxHp = maxHp; }

    int atk() const;
    void setAtk(int atk) { m_atk = atk; }

    int range() const;
    void setRange(int range) { m_range = range; }

    void setBonusMaxHp(int bonus) { m_bonusMaxHp = bonus; }
    void setBonusAtk(int bonus) { m_bonusAtk = bonus; }
    void setBonusRange(int bonus) { m_bonusRange = bonus; }
    int bonusMaxHp() const { return m_bonusMaxHp; }
    int bonusAtk() const { return m_bonusAtk; }
    int bonusRange() const { return m_bonusRange; }

    int maxMana() const;
    void setMaxMana(int maxMana) { m_maxMana = maxMana; }

    int mana() const { return m_mana; }
    void setMana(int mana) { m_mana = mana > 0 ? mana : 0; }

    int level() const { return m_level; }
    void setLevel(int level) { m_level = level; }

    Owner owner() const { return m_owner; }
    void setOwner(Owner o) { m_owner = o; }

    Status status() const { return m_status; }
    void setStatus(Status status) { m_status = status; }

    Trait trait() const { return m_trait; }
    void setTrait(Trait trait) { m_trait = trait; }

    Unit* target() const { return m_target; }
    void setTarget(Unit* target) { m_target = target; }

    class Equipment* equipment() const { return m_equipment; }
    void setEquipment(class Equipment* eq);

    virtual void act(Game* game);
    virtual void upgrade();
    virtual void skill(Game* game);
    Unit* findTarget(Game* game) const;
    int distanceTo(const Unit* other) const;
    void moveTowardsTarget(Game* game, const Unit* target);
    void attackTarget(Unit* target);
    bool prepareForAct(Game* game);
    virtual void normalIdleBehavior(Game* game);
    virtual void normalMoveBehavior(Game* game);
    void resolveAttack(Game* game);

    void setStunTurns(int turns) { m_stunTurns = turns; }
    bool isStunned() const { return m_stunTurns > 0; }
    void setDamageOutputReduction(int turns, float ratio);
    bool hasDamageOutputReduction() const { return m_damageOutputReductionTurns > 0; }
    void setVunerableTurns(int turns) { m_vunerableTurns = turns > 0 ? turns : 0; }
    bool isVunerable() const { return m_vunerableTurns > 0; }
    void setVunerableDamageIncreaseRatio(float ratio) { m_vunerableDamageIncreaseRatio = ratio > 0.0f ? ratio : 0.0f; }
    void takeDamage(int damage);

protected:
    Unit* m_target;
    Status m_status;
    int adjustDamageOutput(int damage) const;

private:
    static int s_nextId;
    
    int m_id;
    QString m_name;
    QPoint m_position;
    int m_hp;
    int m_maxHp;
    int m_bonusMaxHp;
    int m_atk;
    int m_bonusAtk;
    int m_range;
    int m_bonusRange;
    int m_maxMana;
    int m_mana;
    int m_level;
    Owner m_owner;
    Trait m_trait; // 羁绊类型
    int m_stunTurns;
    int m_damageOutputReductionTurns;
    float m_damageOutputReductionRatio;
    int m_vunerableTurns;
    float m_vunerableDamageIncreaseRatio;
    
    class Equipment* m_equipment;
};

class Warrior : public Unit
{
public:
    explicit Warrior(const QString& name = QString::fromUtf8("战士")
        , int hp = 150, int atk = 15);
    void skill(Game* game) override;
};

class Mage : public Unit
{
public:    
    explicit Mage(const QString& name = QString::fromUtf8("法师")
        , int hp = 80, int atk = 23);
    void skill(Game* game) override;
};

class Archer : public Unit
{
public:
    explicit Archer(const QString& name = QString::fromUtf8("弓手")
        , int hp = 100, int atk = 15);
    void skill(Game* game) override;
};

class Boss : public Unit
{
public:
    explicit Boss(const QString& name = QString("Boss")
        , int hp = 200, int atk = 30);
    void skill(Game* game) override;
};

#endif // UNIT_H
