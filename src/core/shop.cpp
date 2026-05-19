#include "shop.h"
#include "game.h"
#include "entity/equipment.h"
#include <QRandomGenerator>

Shop::Shop(int size)
    : m_products(size > 0 ? size : 5, nullptr)
{
    generate();
}

void Shop::createUnit(int index)
{
    int traitIndex = QRandomGenerator::global()->bounded((int)traits.size());
    UnitInfo* info = new UnitInfo{
        unitNames[traitIndex],
        traits[traitIndex],
        hpValues[traitIndex],
        atkValues[traitIndex]
    };
    if (m_products[index]) {
        delete m_products[index];
    }
    m_products[index] = info;
}

void Shop::generate()
{
    for (int i = 0; i < m_products.size(); ++i) {
        createUnit(i);
    }
}

void Shop::refresh(Game* game)
{
    game->player()->costGold(2); // 刷新需要花费2金币
    generate();
}

void Shop::buyUnit(int index, Game* game)
{
    if (index < 0 || index >= m_products.size()) {
        return; // 无效索引
    }
    UnitInfo* info = m_products[index];
    if (!info) {
        return; // 没有商品
    }
    if (game->player()->Gold() < 3) {
        return; // 金钱不足
    }

    game->player()->costGold(3); // 购买需要花费3金币
    
    Unit* newUnit = nullptr;
    if (info->trait == Unit::Trait::Warrior) {
        newUnit = new Warrior(info->name, info->hp, info->atk);
    } else if (info->trait == Unit::Trait::Mage) {
        newUnit = new Mage(info->name, info->hp, info->atk);
    } else if (info->trait == Unit::Trait::Archer) {
        newUnit = new Archer(info->name, info->hp, info->atk);
    } else {
        newUnit = new Unit(info->name, info->trait, info->hp, info->atk);
    }
    
    newUnit->setOwner(Unit::Owner::PlayerCtrl);

    // 试图将单位放入备战区，若备战区已满则购买失败
    bool placed = false;
    for (int i = game->bench()->capacity() - 1; i >= 0; --i) {
        if (!game->bench()->hasUnitAt(i)) {
            if (game->bench()->addUnit(newUnit, i)) {
                // Must add to game's unit list to be managed and rendered!
                game->addUnitFromShop(newUnit);
                delete info;
                m_products[index] = nullptr;
                placed = true;
            }
            break;
        }
    }

    if (!placed) {
        game->player()->addGold(3); // 退还金钱
        delete newUnit; // 购买失败，删除创建的单位
    } else {
        // 判定是否存在相同职业的三个1星单位
        QList<Unit*> matchingUnits;
        for (Unit* u : game->units()) {
            if (u->owner() == Unit::Owner::PlayerCtrl && 
                u->trait() == newUnit->trait() && 
                u->level() == 1) {
                matchingUnits.append(u);
            }
        }
        
        if (matchingUnits.size() >= 3) {
            Unit::Trait t = matchingUnits[0]->trait();
            QString tname = matchingUnits[0]->name();
            // 记录纯基础属性（不含羁绊加成和装备加成），避免升星后羁绊被重复叠加。
            int baseHp = matchingUnits[0]->maxHp() - matchingUnits[0]->bonusMaxHp()
                         - (matchingUnits[0]->equipment() ? matchingUnits[0]->equipment()->bonusHp() : 0);
            int baseAtk = matchingUnits[0]->atk() - matchingUnits[0]->bonusAtk()
                          - (matchingUnits[0]->equipment() ? matchingUnits[0]->equipment()->bonusAtk() : 0);

            // 保留首个遇到装备的单位的装备，其余装备释放避免泄露。
            Equipment* preservedEq = nullptr;
            for (int i = 0; i < 3; ++i) {
                Equipment* eq = matchingUnits[i]->equipment();
                if (!eq) continue;
                if (!preservedEq) {
                    preservedEq = eq;
                } else {
                    delete eq;
                }
                // 解除原单位对装备的引用，避免后续误用或重复释放。
                matchingUnits[i]->setEquipment(nullptr);
            }

            for (int i = 0; i < 3; ++i) {
                game->removeUnitNow(matchingUnits[i]);
            }
            
            Unit* star2 = nullptr;
            if (t == Unit::Trait::Warrior) {
                star2 = new Warrior(tname, baseHp, baseAtk);
            } else if (t == Unit::Trait::Mage) {
                star2 = new Mage(tname, baseHp, baseAtk);
            } else if (t == Unit::Trait::Archer) {
                star2 = new Archer(tname, baseHp, baseAtk);
            } else {
                star2 = new Unit(tname, t, baseHp, baseAtk);
            }
            star2->setOwner(Unit::Owner::PlayerCtrl);
            star2->upgrade(); // 变成2星并增加属性
            if (preservedEq) {
                star2->setEquipment(preservedEq);
            }
            
            for (int i = game->bench()->capacity() - 1; i >= 0; --i) {
                if (!game->bench()->hasUnitAt(i)) {
                    if (game->bench()->addUnit(star2, i)) {
                        game->addUnitFromShop(star2);
                        break;
                    }
                }
            }
        }
    }
}

void Shop::upgradeCapacity(Game* game)
{
    if (!game || game->player()->Gold() < 5) {
        return; // 金钱不足
    }
    
    game->player()->costGold(5); // 升级需要花费5金币
    game->player()->addPopulationCap(2); // 每次升级增加2点人口上限
}

void Shop::sellUnit(Unit* unit, Game* game)
{
    if (!unit || !game) {
        return; // 无效单位或游戏对象
    }

    // 卖出单位获得1金币
    game->player()->addGold(1);

    // Call game's request Remove Unit to properly clean up from board, bench, and scene without crashing
    game->requestRemoveUnit(unit);
}