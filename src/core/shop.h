#ifndef SHOP_H
#define SHOP_H

#include <QVector>
#include "entity\unit.h"

class Game;

struct UnitInfo {
    QString name;
    Unit::Trait trait;
    int hp;
    int atk;
};

class Shop
{
public:
    Shop(int size = 5);
    ~Shop() = default;

    void createUnit(int index);
    void generate();
    void refresh(Game* game);

    void buyUnit(int index, Game* game);
    void upgradeCapacity(Game* game);
    void sellUnit(Unit* unit, Game* game);

    const QVector<UnitInfo*>& products() const { return m_products; }

private:
    const QVector<QString> unitNames = {
        "Warrior", "Mage", "Archer"
    };
    const QVector<Unit::Trait> traits = {
        Unit::Trait::Warrior, 
        Unit::Trait::Mage, 
        Unit::Trait::Archer, 
    };
    const QVector<int> hpValues = {
        150, 80, 100
    };
    const QVector<int> atkValues = {
        10, 15, 10
    };

    QVector<UnitInfo*> m_products;
};

#endif // SHOP_H