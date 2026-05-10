#ifndef PLAYER_H
#define PLAYER_H

#include <QString>

class Player {
public:
    explicit Player(const QString& name = QString("User"));

    int getHp() const {return hp;}
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

    int getGold() const { return gold; }
    void setGold(int num) { gold = num; }
    void addGold(int num) { gold += num; }
    bool costGold(int num) {
        if (gold < num) {
            return false;
        }
        gold -= num;
        return true;
    }

    int getLevel() const { return level; }
    void setLevel(int num) { level = num; }

    int getPopulationCap() const { return populationCap; }
    void setPopulationCap(int tar) { populationCap = tar;}

    int getCurStage() const { return curStage; }
    void setCurStage(int num) { curStage = num; }
    void nextStage() { curStage += 1; }

    QString getPlayerName() const { return player_name; }
    void setPlayerName(const QString& name) { player_name = name; }


private:
    int hp; // 玩家生命值(默认100)
    int gold; // 玩家金币(默认5)
    int level; // 玩家等级(默认1)
    int populationCap; // 人口上限(默认6)
    int curStage; // 当前关卡(初始1)
    QString player_name; // 玩家姓名
};

#endif // PLAYER_H