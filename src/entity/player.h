#ifndef PLAYER_H
#define PLAYER_H

#include <QString>
#include <QList>
#include "equipment.h"

class Equipment;

class Player {
public:
    explicit Player(const QString& name = QString("User"));

    int Hp() const {return hp;}
    void setHp(int tar) { hp = tar; }
    void healHp(int num) { hp += num; }
    bool reduceHp(int damage) {
        if (hp <= damage) {
            hp = 0;
            return false;
        }
        hp -= damage;
        return true;
    }

    int Gold() const { return gold; }
    void setGold(int num) { gold = num; }
    void addGold(int num) { gold += num; }
    bool costGold(int num) {
        if (gold < num) {
            return false;
        }
        gold -= num;
        return true;
    }

    int Level() const { return level; }
    void setLevel(int num) { level = num; }

    int PopulationCap() const { return populationCap; }
    void addPopulationCap(int num) { populationCap += num; }
    void setPopulationCap(int tar) { populationCap = tar;}

    int CurStage() const { return curStage; }
    void setCurStage(int num) { curStage = num; }
    void nextStage() { curStage += 1; }

    QString getPlayerName() const { return player_name; }
    void setPlayerName(const QString& name) { player_name = name; }

    Equipment* inventory(int index) const { return (index >= 0 && index < 5) ? m_inventory[index] : nullptr; }
    void setInventory(int index, Equipment* eq) { if (index >= 0 && index < 5) m_inventory[index] = eq; }
    bool addInventory(Equipment* eq) {
        for(int i = 0; i < 5; ++i) {
            if (!m_inventory[i]) {
                m_inventory[i] = eq;
                return true;
            }
        }
        delete eq; // 超过容量被丢弃
        return false;
    }
    void removeInventory(int index) {
        if (index >= 0 && index < 5) {
            delete m_inventory[index];
            m_inventory[index] = nullptr;
        }
    }

private:
    int hp; // 玩家生命值(默认100)
    int gold; // 玩家金币(默认0)
    int level; // 玩家等级(默认1)
    int populationCap; // 人口上限(默认6)
    int curStage; // 当前关卡(初始1)
    QString player_name; // 玩家姓名
    Equipment* m_inventory[5];
};

#endif // PLAYER_H