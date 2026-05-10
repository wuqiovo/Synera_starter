#include "player.h"

// 玩家对象初始化，暂时固定设定
Player::Player(const QString& name)
    : player_name(name)
    , hp(100)
    , gold(5)
    , level(1)
    , populationCap(6)
    , curStage(1)
{}