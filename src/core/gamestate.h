#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <QPoint>
#include <QString>
#include <QVector>
#include "entity/unit.h"
#include "entity/equipment.h"

// 单位存档
struct UnitState
{
    // 单位唯一 ID。
    int id = -1;
    // 单位名称。
    QString name;
    // 单位位置（棋盘坐标，备战区为 (-1, -1)）。
    QPoint position;
    // 当前生命值。
    int hp = 0;
    // 最大生命值。
    int maxHp = 0;
    // 攻击力。
    int atk = 0;
    // 攻击距离。
    int range = 0;
    // 最大法力值。
    int maxMana = 0;
    // 当前法力值。
    int mana = 0;
    // 控制归属。
    Unit::Owner owner = Unit::Owner::PlayerCtrl;
    // 当前状态。
    Unit::Status status = Unit::Status::Idle;
    // 羁绊类型。
    Unit::Trait trait = Unit::Trait::None;
    // 星级/等级
    int level = 1;
    // 单位佩戴装备的类型，None 表示无装备。
    Equipment::Type equipmentType = Equipment::Type::None;
};

// 玩家存档
struct PlayerState
{
    // 玩家生命值。
    int hp = 100;
    // 当前金币。
    int gold = 0;
    // 玩家等级。
    int level = 1;
    // 人口上限。
    int populationCap = 0;
    // 当前关卡。
    int stage = 1;
    // 当前回合。
    int round = 1;
    // 玩家名称。
    QString name;
    // 装备栏中每格的装备类型，None 表示空格。
    QVector<Equipment::Type> inventoryTypes;
};

// 全局游戏存档
struct GameState
{
    // 玩家状态。
    PlayerState player;
    // 已战胜的敌方波次。
    int enemyDefeatedWaves = 0;
    // 敌方最大波次。
    int enemyMaxWaves = 5;
    // 是否处于战斗阶段。
    bool inBattle = false;

    // 全部单位状态列表。
    QVector<UnitState> units;
    // 备战区槽位映射：index -> units 下标，-1 表示空槽。
    QVector<int> benchUnitIndices;
    
    // 全局单位 ID 计数器。
    int nextUnitId = 0;
};

#endif // GAMESTATE_H
