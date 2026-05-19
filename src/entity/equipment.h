#ifndef EQUIMENT_H
#define EQUIMENT_H

class Equipment
{
public:
    enum class Type {
        None,
        Sword,  // 剑 +3atk
        Crystal,  // 水晶 -20mana
        armor,  // 盔甲 +30hp
        Gloves  // 手套 行动速度加倍
    };

    explicit Equipment(Type type = Type::None);
    ~Equipment() = default;

    Type type() const { return m_type; }
    void setType(Type type);

    int bonusHp() const { return m_bonusHp; }
    int bonusAtk() const { return m_bonusAtk; }
    int bonusRange() const { return m_bonusRange; }
    int bonusMaxMana() const { return m_bonusMaxMana; }

private:
    Type m_type;
    int m_bonusHp;
    int m_bonusAtk;
    int m_bonusRange;
    int m_bonusMaxMana;
    void applyStats();
};

#endif // EQUIMENT_H