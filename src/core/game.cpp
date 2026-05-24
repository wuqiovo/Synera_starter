#include "game.h"
#include "entity/unit.h"
#include "gui/griditem.h"
#include "gui/unititem.h"
#include "gui/equipmentslot.h"
#include "core/gamestate.h"
#include "entity/equipment.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QPen>
#include <QtMath>
#include <QRandomGenerator>
#include <algorithm>

namespace {
constexpr qreal kZGrid = 0.0;
constexpr qreal kZUnit = 1.0;
constexpr qreal kZDraggingUnit = 2.0;
}

// 实现玩家实体与敌方轮次生成中的玩家侧游戏对象初始化入口。
Game::Game(QObject* parent)
    : QObject(parent)
    , m_scene(new QGraphicsScene(this))
    , m_bench(8)
    , m_dragActive(false)
    , m_activeUnitId(-1)
    , m_sourceGrid(-1, -1)
    , m_sourceFromBench(false)
    , m_sourceBenchSlot(-1)
    , m_stage(0)
    , m_round(1)
    , m_winStreak(0)
    , m_loseStreak(0)
    , m_enemyDefeatedWaves(0)
    , m_enemyMaxWaves(5)
    , m_enemyUnitCap(30)
    , m_enemyUnitCount(0)
    , m_inBattle(false)
    , m_battleTimer(new QTimer(this))
    , m_battleTickMs(300)
    , m_activeActionUnitId(-1)
    , m_battleTurnIndex(0)
    , m_dragEqActive(false)
    , m_activeEqSlotIndex(-1)
    , m_rows(Board::ROWS)
    , m_cols(Board::COLS)
    , m_radius(46.0)
    , m_rowSpacing(69.0)
    , m_benchSlotColor(QColor(50, 50, 70))
    , m_benchSlotHighlightColor(QColor(100, 150, 120))
    , m_benchTop(0.0)
    , m_separatorY(0.0)
{
    m_battleTimer->setInterval(m_battleTickMs);
    connect(m_battleTimer, &QTimer::timeout, this, &Game::battleTick);
}

// 对应阶段一基础系统的资源管理要求，负责安全释放单位等运行期资源。
Game::~Game()
{
    clearAllUnits();
}

// 对应阶段一「实现备战区与棋盘之间的数据同步」的初始落位与状态重置。
// 略微修改以适配新增的备战区部分
void Game::reset()
{
    stopBattleLoop();
    setActiveUnitItem(-1);
    m_board.clear();
    m_bench.clear();
    m_dragActive = false;
    m_activeUnitId = -1;
    m_sourceGrid = QPoint(-1, -1);
    m_sourceFromBench = false;
    m_sourceBenchSlot = -1;
    m_round = 0;
    m_winStreak = 0;
    m_loseStreak = 0;
    m_enemyDefeatedWaves = 0;
    m_enemyMaxWaves = 5;
    m_inBattle = false;
    updateShopUI();
    m_player.setHp(100);
    m_player.setGold(0);
    m_player.setLevel(1);
    m_player.setPopulationCap(6);
    m_player.setCurStage(m_stage);

    // 清空装备栏
    for (int i = 0; i < 5; ++i) {
        m_player.removeInventory(i);
    }

    // 新 stage 初始化：清空所有历史单位后重新生成初始玩家单位。
    clearAllUnits();
    createStarterUnitsIfNeeded();
    m_shop.generate();
    buildScene();
    refreshTraitCounts();

    const QPoint playerInitialPositions[] = {
        QPoint(0, 7),
        QPoint(1, 7),
        QPoint(2, 7)
    };

    int playerBoardCount = 0;

    for (Unit* unit : m_units) {
        if (!unit) {
            continue;
        }

        if (playerBoardCount < 3) {
            m_board.addUnit(unit, playerInitialPositions[playerBoardCount]);
            ++playerBoardCount;
            continue;
        }

        const int slot = m_bench.firstEmptySlot();
        if (slot >= 0) {
            m_bench.addUnit(unit, slot);
        }
    }

    startNextRound();

    emit playerInfoChanged();
    emit enemyInfoChanged();
    emit battleStateChanged(m_inBattle);
}

// 推进到下一关卡：初始化玩家单位和信息。
void Game::startNextStage()
{
    ++m_stage;
    m_player.setCurStage(m_stage);
    reset();
}

// 推进到同一关卡内的下一回合：在部署阶段生成新敌方单位。
void Game::startNextRound()
{
    ++m_round;
    clearEnemyUnits();
    restorePlayerUnits();
    spawnEnemiesForCurrentRound();
    syncFromBoard();
    refreshTraitCounts();
    emit stageRoundChanged();
    m_battleTurnIndex = 0;
}

// 结束部署：进入战斗阶段。
void Game::endDeployment()
{
    if (m_inBattle) {
        return;
    }

    // 记录战斗前玩家棋盘上的单位状态
    m_preBattlePlayerUnits.clear();
    for (Unit* unit : m_units) {
        if (unit && unit->owner() == Unit::Owner::PlayerCtrl && isUnitOnBoard(unit)) {
            UnitState u;
            u.id = unit->id();
            u.name = unit->name();
            u.position = unit->position();
            // 保存纯基础值，不含羁绊加成和装备加成，恢复时由 setEquipment 自动处理装备 hp。
            int eqBonusHp = unit->equipment() ? unit->equipment()->bonusHp() : 0;
            u.hp = unit->hp() - eqBonusHp;
            u.maxHp = unit->maxHp() - unit->bonusMaxHp() - eqBonusHp;
            u.atk = unit->atk() - unit->bonusAtk() - (unit->equipment() ? unit->equipment()->bonusAtk() : 0);
            u.range = unit->range() - unit->bonusRange() - (unit->equipment() ? unit->equipment()->bonusRange() : 0);
            u.maxMana = unit->maxMana() - (unit->equipment() ? unit->equipment()->bonusMaxMana() : 0);
            u.mana = unit->mana();
            u.owner = unit->owner();
            u.status = unit->status();
            u.trait = unit->trait();
            u.level = unit->level();
            u.equipmentType = unit->equipment() ? unit->equipment()->type() : Equipment::Type::None;
            m_preBattlePlayerUnits.push_back(u);
        }
    }

    m_inBattle = true;
    updateShopUI();
    emit battleStateChanged(m_inBattle);
    startBattleLoop();
}

// 调试用：手动结束战斗并结算回合。
void Game::debugEndBattle()
{
    if (!m_inBattle) {
        return;
    }

    stopBattleLoop();
    setActiveUnitItem(-1);

    resolveRoundFromCurrentBoard();

    if (!m_inBattle) {
        return;
    }

    m_inBattle = false;
    updateShopUI();
    emit battleStateChanged(m_inBattle);

    if (m_enemyDefeatedWaves < m_enemyMaxWaves && m_player.Hp() > 0) {
        startNextRound();
    }
}

// 用于调试：重置全部状态并回到初始关卡。
void Game::debugResetAll()
{
    m_stage = 0;
    startNextStage();
}

void Game::clearAllUnits()
{
    qDeleteAll(m_units);
    m_units.clear();
    m_unitsPendingRemoval.clear();
    m_enemyUnitCount = 0;
    m_battleTurnIndex = 0;
    m_preBattlePlayerUnits.clear(); // 清理残留的存档信息
    setActiveUnitItem(-1);
}

GameState Game::captureState() const
{
    GameState state;
    state.player.hp = m_player.Hp();
    state.player.gold = m_player.Gold();
    state.player.level = m_player.Level();
    state.player.populationCap = m_player.PopulationCap();
    state.player.stage = m_stage;
    state.player.round = m_round;
    state.player.winStreak = m_winStreak;
    state.player.loseStreak = m_loseStreak;
    state.player.name = m_player.getPlayerName();

    state.enemyDefeatedWaves = m_enemyDefeatedWaves;
    state.enemyMaxWaves = m_enemyMaxWaves;
    state.inBattle = m_inBattle;

    // 预分配空间以提升性能，避免多次扩容。
    state.units.reserve(m_units.size());
    for (Unit* unit : m_units) {
        if (!unit) {
            continue;
        }
        UnitState u;
        u.id = unit->id();
        u.name = unit->name();
        u.position = unit->position();
        // 存档中保存纯基础值，不含羁绊加成和装备加成，恢复时由 setEquipment 自动处理装备 hp。
        int eqBonusHp = unit->equipment() ? unit->equipment()->bonusHp() : 0;
        u.hp = unit->hp() - eqBonusHp;
        u.maxHp = unit->maxHp() - unit->bonusMaxHp() - eqBonusHp;
        u.atk = unit->atk() - unit->bonusAtk() - (unit->equipment() ? unit->equipment()->bonusAtk() : 0);
        u.range = unit->range() - unit->bonusRange() - (unit->equipment() ? unit->equipment()->bonusRange() : 0);
        u.maxMana = unit->maxMana() - (unit->equipment() ? unit->equipment()->bonusMaxMana() : 0);
        u.mana = unit->mana();
        u.owner = unit->owner();
        u.status = unit->status();
        u.trait = unit->trait();
        u.level = unit->level();
        u.equipmentType = unit->equipment() ? unit->equipment()->type() : Equipment::Type::None;
        state.units.push_back(u);
    }

    // 保存玩家装备栏状态。
    state.player.inventoryTypes.resize(5);
    for (int i = 0; i < 5; ++i) {
        Equipment* eq = m_player.inventory(i);
        state.player.inventoryTypes[i] = eq ? eq->type() : Equipment::Type::None;
    }

    state.benchUnitIndices.resize(m_bench.capacity());
    state.benchUnitIndices.fill(-1);
    for (int slot = 0; slot < m_bench.capacity(); ++slot) {
        Unit* unit = m_bench.unitAt(slot);
        if (!unit) {
            continue;
        }
        int index = -1;
        for (int i = 0; i < m_units.size(); ++i) {
            if (m_units[i] == unit) {
                index = i;
                break;
            }
        }
        state.benchUnitIndices[slot] = index;
    }

    state.nextUnitId = Unit::getNextId();

    return state;
}

void Game::loadFromState(const GameState& state)
{
    m_board.clear();
    m_bench.clear();
    clearAllUnits();

    m_stage = state.player.stage;
    m_round = state.player.round;
    m_winStreak = state.player.winStreak;
    m_loseStreak = state.player.loseStreak;
    m_enemyDefeatedWaves = state.enemyDefeatedWaves;
    m_enemyMaxWaves = state.enemyMaxWaves;
    m_inBattle = state.inBattle;
    updateShopUI();

    m_player.setHp(state.player.hp);
    m_player.setGold(state.player.gold);
    m_player.setLevel(state.player.level);
    m_player.setPopulationCap(state.player.populationCap);
    m_player.setCurStage(state.player.stage);
    m_player.setPlayerName(state.player.name);

    // 清理现有装备栏并从存档恢复（避免旧装备泄露）。
    for (int i = 0; i < 5; ++i) {
        m_player.removeInventory(i);
    }
    for (int i = 0; i < 5; ++i) {
        Equipment::Type t = (i < state.player.inventoryTypes.size())
            ? state.player.inventoryTypes[i]
            : Equipment::Type::None;
        if (t != Equipment::Type::None) {
            m_player.setInventory(i, new Equipment(t));
        } else {
            m_player.setInventory(i, nullptr);
        }
    }

    m_units.reserve(state.units.size());
    for (const UnitState& u : state.units) {
        Unit* unit = nullptr;
        switch (u.trait) {
        case Unit::Trait::Warrior:
            unit = new Warrior(u.name);
            break;
        case Unit::Trait::Mage:
            unit = new Mage(u.name);
            break;
        case Unit::Trait::Archer:
            unit = new Archer(u.name);
            break;
        case Unit::Trait::Boss:
            unit = new Boss(u.name);
            break;
        default:
            unit = new Unit(u.name, Unit::Trait::None);
            break;
        }
        unit->setHp(u.hp);
        unit->setMaxHp(u.maxHp);
        unit->setAtk(u.atk);
        unit->setRange(u.range);
        unit->setMaxMana(u.maxMana);
        unit->setMana(u.mana);
        unit->setOwner(u.owner);
        unit->setStatus(u.status);
        unit->setTrait(u.trait);
        unit->setLevel(u.level);
        unit->setPosition(u.position);
        if (u.equipmentType != Equipment::Type::None) {
            unit->setEquipment(new Equipment(u.equipmentType));
        }
        if (u.id != -1) {
            unit->setId(u.id);
        }
        m_units.append(unit);
        if (u.owner == Unit::Owner::EnemyCtrl) {
            ++m_enemyUnitCount;
        }
    }

    for (int i = 0; i < m_units.size(); ++i) {
        Unit* unit = m_units[i];
        if (!unit) {
            continue;
        }
        const QPoint pos = unit->position();
        if (m_board.isValidPosition(pos)) {
            m_board.addUnit(unit, pos);
        }
    }

    for (int slot = 0; slot < state.benchUnitIndices.size(); ++slot) {
        const int index = state.benchUnitIndices[slot];
        if (index < 0 || index >= m_units.size()) {
            continue;
        }
        Unit* unit = m_units[index];
        if (unit) {
            m_board.removeUnit(unit);
            m_bench.addUnit(unit, slot);
        }
    }

    if (state.nextUnitId > 0) {
        Unit::setNextId(state.nextUnitId);
    } else {
        // 兼容没有 nextUnitId 的旧存档：通过 maxId 确保不会重叠
        int maxId = Unit::getNextId() - 1;
        for (Unit* u : m_units) {
            if (u && u->id() > maxId) {
                maxId = u->id();
            }
        }
        Unit::setNextId(maxId + 1);
    }

    buildScene();
    syncFromBoard();
    refreshTraitCounts();

    emit playerInfoChanged();
    emit enemyInfoChanged();
    emit stageRoundChanged();
    emit battleStateChanged(m_inBattle);
}

// 获取指定阵营的羁绊单位数量。
int Game::traitCount(Unit::Owner owner, Unit::Trait trait) const
{
    // 从缓存读取指定羁绊的单位数量。
    const int index = traitIndex(trait);
    if (index < 0 || index >= kTraitCount) {
        return 0;
    }

    if (owner == Unit::Owner::EnemyCtrl) {
        return m_enemyTraitCounts[static_cast<size_t>(index)];
    }

    return m_playerTraitCounts[static_cast<size_t>(index)];
}

// 根据当前单位列表刷新羁绊计数缓存， 并应用羁绊加成效果。
// 目前有战士的两种羁绊，法师一种，弓手一种。
void Game::refreshTraitCounts()
{
    // 全量重算当前所有单位的羁绊计数。
    m_playerTraitCounts.fill(0);
    m_enemyTraitCounts.fill(0);

    for (Unit* unit : m_units) {
        if (!unit || !isUnitOnBoard(unit) || unit->status() == Unit::Status::Dead) {
            continue;
        }

        const int index = traitIndex(traitForUnit(unit));
        if (index < 0 || index >= kTraitCount) {
            continue;
        }

        int count = (unit->level() == 2) ? 2 : 1;

        if (unit->owner() == Unit::Owner::EnemyCtrl) {
            m_enemyTraitCounts[static_cast<size_t>(index)] += count;
        } else {
            m_playerTraitCounts[static_cast<size_t>(index)] += count;
        }
    }

    // 应用羁绊加成：仅当目标值与当前值不同时才更新，避免重复叠加。
    // 备战区单位不参与计算，既不增加也不减少其加成。
    for (Unit* unit : m_units) {
        if (!unit) continue;

        if (!isUnitOnBoard(unit)) continue;

        auto ownerCounts = (unit->owner() == Unit::Owner::EnemyCtrl) ? m_enemyTraitCounts : m_playerTraitCounts;

        if (unit->trait() == Unit::Trait::Warrior) {
            const int warriorCount = ownerCounts[traitIndex(Unit::Trait::Warrior)];
            const int targetHpBonus  = (warriorCount >= 2) ? 30 : 0;
            const int targetAtkBonus = (warriorCount >= 4) ? 5  : 0;
            if (unit->bonusMaxHp() != targetHpBonus) unit->setBonusMaxHp(targetHpBonus);
            if (unit->bonusAtk()   != targetAtkBonus) unit->setBonusAtk(targetAtkBonus);
            if (unit->bonusRange() != 0) unit->setBonusRange(0);
        }
        else if (unit->trait() == Unit::Trait::Archer) {
            const int archerCount = ownerCounts[traitIndex(Unit::Trait::Archer)];
            const int targetRangeBonus = (archerCount >= 3) ? 1 : 0;
            if (unit->bonusRange() != targetRangeBonus) unit->setBonusRange(targetRangeBonus);
            if (unit->bonusMaxHp() != 0) unit->setBonusMaxHp(0);
            if (unit->bonusAtk()   != 0) unit->setBonusAtk(0);
        }
        else {
            // 法师等其他职业：羁绊加成在其 skill 中处理，此处清除可能残留的基础属性加成。
            if (unit->bonusMaxHp() != 0) unit->setBonusMaxHp(0);
            if (unit->bonusAtk()   != 0) unit->setBonusAtk(0);
            if (unit->bonusRange() != 0) unit->setBonusRange(0);
        }

        // 保证血量不溢出
        if (!m_inBattle) {
            unit->setHp(unit->maxHp());
        } else if (unit->hp() > unit->maxHp()) {
            unit->setHp(unit->maxHp());
        }
    }

    emit playerInfoChanged();
    emit enemyInfoChanged();
    
    // 触发图元重绘，以更新羁绊变化后的血条等信息
    for (UnitItem* item : m_unitItems) {
        if (item) {
            item->update();
        }
    }

    if (m_scene) {
        m_scene->update();
    }
}

bool Game::saveToFile(const QString& filePath) const
{
    if (m_inBattle) {
        return false; // 战斗中不允许存档
    }

    GameState state = captureState();

    QJsonObject root;
    QJsonObject player;
    player["hp"] = state.player.hp;
    player["gold"] = state.player.gold;
    player["level"] = state.player.level;
    player["populationCap"] = state.player.populationCap;
    player["stage"] = state.player.stage;
    player["round"] = state.player.round;
    player["winStreak"] = state.player.winStreak;
    player["loseStreak"] = state.player.loseStreak;
    player["name"] = state.player.name;
    QJsonArray inventory;
    for (int i = 0; i < state.player.inventoryTypes.size(); ++i) {
        inventory.append(static_cast<int>(state.player.inventoryTypes[i]));
    }
    player["inventory"] = inventory;
    root["player"] = player;

    root["enemyDefeatedWaves"] = state.enemyDefeatedWaves;
    root["enemyMaxWaves"] = state.enemyMaxWaves;
    root["inBattle"] = state.inBattle;
    root["nextUnitId"] = state.nextUnitId;

    QJsonArray units;
    for (const UnitState& u : state.units) {
        QJsonObject obj;
        obj["id"] = u.id;
        obj["name"] = u.name;
        obj["x"] = u.position.x();
        obj["y"] = u.position.y();
        obj["hp"] = u.hp;
        obj["maxHp"] = u.maxHp;
        obj["atk"] = u.atk;
        obj["range"] = u.range;
        obj["maxMana"] = u.maxMana;
        obj["mana"] = u.mana;
        obj["owner"] = static_cast<int>(u.owner);
        obj["status"] = static_cast<int>(u.status);
        obj["trait"] = static_cast<int>(u.trait);
        obj["level"] = u.level;
        obj["equipment"] = static_cast<int>(u.equipmentType);
        units.append(obj);
    }
    root["units"] = units;

    QJsonArray bench;
    for (int index : state.benchUnitIndices) {
        bench.append(index);
    }
    root["bench"] = bench;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool Game::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    GameState state;

    const QJsonObject player = root.value("player").toObject();
    state.player.hp = player.value("hp").toInt(100);
    state.player.gold = player.value("gold").toInt(0);
    state.player.level = player.value("level").toInt(1);
    state.player.populationCap = player.value("populationCap").toInt(0);
    state.player.stage = player.value("stage").toInt(1);
    state.player.round = player.value("round").toInt(1);
    state.player.winStreak = player.value("winStreak").toInt(0);
    state.player.loseStreak = player.value("loseStreak").toInt(0);
    state.player.name = player.value("name").toString();

    // 读取玩家装备栏（旧存档可能没有此字段，全部默认为 None）。
    state.player.inventoryTypes.resize(5);
    state.player.inventoryTypes.fill(Equipment::Type::None);
    if (player.contains("inventory")) {
        const QJsonArray invArr = player.value("inventory").toArray();
        const int count = qMin(invArr.size(), 5);
        for (int i = 0; i < count; ++i) {
            state.player.inventoryTypes[i] = static_cast<Equipment::Type>(invArr.at(i).toInt(0));
        }
    }

    state.enemyDefeatedWaves = root.value("enemyDefeatedWaves").toInt(0); // 旧存档可能会丢失此项，没关系
    state.enemyMaxWaves = root.value("enemyMaxWaves").toInt(5);
    state.inBattle = root.value("inBattle").toBool(false);
    
    if (root.contains("nextUnitId")) {
        state.nextUnitId = root.value("nextUnitId").toInt();
    } else {
        state.nextUnitId = 0;
    }

    const QJsonArray units = root.value("units").toArray();
    for (const QJsonValue& value : units) {
        const QJsonObject obj = value.toObject();
        UnitState u;
        u.id = obj.value("id").toInt(-1);
        u.name = obj.value("name").toString();
        u.position = QPoint(obj.value("x").toInt(-1), obj.value("y").toInt(-1));
        u.hp = obj.value("hp").toInt(0);
        u.maxHp = obj.value("maxHp").toInt(0);
        u.atk = obj.value("atk").toInt(0);
        u.range = obj.value("range").toInt(0);
        u.maxMana = obj.value("maxMana").toInt(0);
        u.mana = obj.value("mana").toInt(0);
        u.owner = static_cast<Unit::Owner>(obj.value("owner").toInt(0));
        u.status = static_cast<Unit::Status>(obj.value("status").toInt(0));
        u.trait = static_cast<Unit::Trait>(obj.value("trait").toInt(0));
        u.level = obj.value("level").toInt(1);
        u.equipmentType = static_cast<Equipment::Type>(obj.value("equipment").toInt(0));
        state.units.push_back(u);
    }

    const QJsonArray bench = root.value("bench").toArray();
    state.benchUnitIndices.resize(bench.size());
    for (int i = 0; i < bench.size(); ++i) {
        state.benchUnitIndices[i] = bench.at(i).toInt(-1);
    }

    loadFromState(state);
    return true;
}

//「实现拖拽摆放」的起始阶段：记录拖拽单位与来源格。
void Game::handleDragStarted(int unitId, const QPoint& sourceGrid, const QPointF&)
{
    Q_UNUSED(sourceGrid);

    Unit* unit = findUnitById(unitId);
    if (!unit || unit->owner() != Unit::Owner::PlayerCtrl) {
        return;
    }

    m_dragActive = true;
    m_activeUnitId = unitId;
    m_sourceFromBench = m_bench.contains(unit);
    if (m_sourceFromBench) {
        m_sourceBenchSlot = m_bench.slotOf(unit);
        m_sourceGrid = QPoint(-1, -1);
    } else {
        m_sourceBenchSlot = -1;
        m_sourceGrid = unit->position();
    }

    UnitItem* item = findUnitItem(unitId);
    if (item) {
        item->setZValue(kZDraggingUnit);
    }
}

//「实现拖拽摆放与非法放置处理」中的拖拽过程反馈（高亮合法/非法目标）。
void Game::handleDragMoved(int unitId, const QPoint&, const QPointF& scenePos)
{
    if (!m_dragActive) {
        return;
    }

    if (unitId != m_activeUnitId) {
        return;
    }

    clearGridHighlights();
    clearBenchHighlights();

    Unit* unit = findUnitById(unitId);
    if (!unit) {
        return;
    }

    if (scenePos.y() >= m_separatorY) {
        const int benchSlot = hitTestBenchSlot(scenePos);
        if (benchSlot >= 0 && benchSlot < m_bench.capacity()) {
            if (canDropOnBench(unit, benchSlot, m_sourceFromBench)) {
                m_benchItems[benchSlot]->setBrush(m_benchSlotHighlightColor);
            }
        }
        
        if (!m_inBattle && m_shopSellRect.contains(scenePos)) {
            if (m_shopSellWidget) {
                m_shopSellWidget->setStyleSheet("#sellBlock { background: rgba(180, 80, 60, 200); border: 2px solid #ff7a5a; border-radius: 5px; }");
            }
        } else {
            if (m_shopSellWidget) {
                m_shopSellWidget->setStyleSheet("#sellBlock { background: rgba(80, 80, 60, 200); border: 2px solid #7a7a5a; border-radius: 5px; }");
            }
        }
        
        return;
    } else {
        // 重置拖拽出商店区域时的高亮状态
        if (m_shopSellWidget) {
            m_shopSellWidget->setStyleSheet("#sellBlock { background: rgba(80, 80, 60, 200); border: 2px solid #7a7a5a; border-radius: 5px; }");
        }
    }

    const QPoint target = worldToGrid(scenePos);
    GridItem* targetItem = findGridItem(target);
    if (targetItem) {
        targetItem->setHoverActive(true);
        if (canDropOnBoard(unit, target)) {
            targetItem->setDropActive(true);
        }
    }
}

//「实现拖拽摆放与非法放置处理」中的落子判定与最终提交。
void Game::handleDropCommand(int unitId, const QPoint& sourceGrid, const QPointF& scenePos)
{
    Q_UNUSED(sourceGrid);

    if (!m_dragActive) {
        return;
    }

    if (unitId != m_activeUnitId) {
        return;
    }

    const QPoint target = worldToGrid(scenePos);
    const int benchSlot = hitTestBenchSlot(scenePos);

    clearGridHighlights();
    clearBenchHighlights();
    
    // 鼠标释放时恢复商店出售区高亮外观
    if (m_shopSellWidget) {
        m_shopSellWidget->setStyleSheet("#sellBlock { background: rgba(80, 80, 60, 200); border: 2px solid #7a7a5a; border-radius: 5px; }");
    }

    Unit* unit = findUnitById(m_activeUnitId);
    
    // 检查拖拽目的地，优先处理商店售卖
    if (unit && !m_inBattle && m_shopSellRect.contains(scenePos)) {
        if (unit->owner() == Unit::Owner::PlayerCtrl) {
            m_shop.sellUnit(unit, this);
            flushUnitRemovals();
            emit playerInfoChanged();
        }
    } else if (unit && benchSlot >= 0 && canDropOnBench(unit, benchSlot, m_sourceFromBench)) {
        if (m_sourceFromBench) {
            if (benchSlot != m_sourceBenchSlot) {
                moveUnitWithinBench(m_sourceBenchSlot, benchSlot);
            }
        } else {
            transferUnitFromBoardToBench(m_activeUnitId, benchSlot);
        }
    } else if (unit && canDropOnBoard(unit, target)) {
        if (m_sourceFromBench) {
            transferUnitFromBenchToBoard(m_activeUnitId, target);
        } else {
            applyDrop(m_activeUnitId, target);
        }
    }

    UnitItem* item = findUnitItem(m_activeUnitId);
    if (item) {
        item->setZValue(kZUnit);
    }

    m_dragActive = false;
    m_activeUnitId = -1;
    m_sourceGrid = QPoint(-1, -1);
    m_sourceFromBench = false;
    m_sourceBenchSlot = -1;

    syncFromBoard();
}

void Game::requestRemoveUnit(Unit* unit)
{
    if (!unit) {
        return;
    }
    if (!m_unitsPendingRemoval.contains(unit)) {
        m_unitsPendingRemoval.append(unit);
    }
}

bool Game::moveUnitDuringBattle(Unit* unit, const QPoint& target)
{
    if (!unit) {
        return false;
    }

    if (!m_board.isValidPosition(target)) {
        return false;
    }

    return m_board.moveUnit(unit, target);
}

bool Game::isUnitOnBoard(const Unit* unit) const
{
    if (!unit) {
        return false;
    }
    const QPoint pos = unit->position();
    return m_board.isValidPosition(pos) && m_board.getUnitAt(pos) == unit;
}

//「实现 Unit 基类」与基础演示需求：创建初始可上阵单位。
void Game::createStarterUnitsIfNeeded()
{
    if (!m_units.isEmpty()) {
        return;
    }

    Unit* player1 = new Warrior();
    Unit* player2 = new Archer();
    Unit* player3 = new Mage();

    m_units.append(player1);
    m_units.append(player2);
    m_units.append(player3);
}

void Game::clearEnemyUnits()
{
    if (m_round == 1) {
        return;
    }

    for (Unit* unit : m_units) {
        if (unit && unit->owner() == Unit::Owner::EnemyCtrl) {
            requestRemoveUnit(unit);
        }
    }

    flushUnitRemovals();

    emit enemyInfoChanged();
}

void Game::restorePlayerUnits()
{
    // 如果没有记录任何状态，不需要恢复，可能是游戏刚初始化完
    if (m_preBattlePlayerUnits.isEmpty()) {
        return;
    }

    // 只移除留在棋盘上的所有玩家单位
    for (Unit* unit : m_units) {
        if (unit && unit->owner() == Unit::Owner::PlayerCtrl && isUnitOnBoard(unit)) {
            requestRemoveUnit(unit);
        }
    }
    flushUnitRemovals();

    auto attachUnitItemFor = [this](Unit* unit) {
        UnitItem* unitItem = new UnitItem(unit);
        unitItem->setZValue(kZUnit);
        m_scene->addItem(unitItem);
        m_unitItems.push_back(unitItem);
        m_unitItemById[unit->id()] = unitItem;

        connect(unitItem, &UnitItem::dragStarted,
                this, &Game::handleDragStarted);
        connect(unitItem, &UnitItem::dragMoved,
                this, &Game::handleDragMoved);
        connect(unitItem, &UnitItem::dragDropped,
                this, &Game::handleDropCommand);
        connect(unitItem, &UnitItem::eqDragStarted,
                this, &Game::handleUnitEqDragStarted);
        connect(unitItem, &UnitItem::eqDragMoved,
                this, &Game::handleUnitEqDragMoved);
        connect(unitItem, &UnitItem::eqDragDropped,
                this, &Game::handleUnitEqDragDropped);
    };

    // 重建所有原来在棋盘上的玩家单位
    for (const UnitState& u : m_preBattlePlayerUnits) {
        Unit* unit = nullptr;
        switch (u.trait) {
        case Unit::Trait::Warrior:
            unit = new Warrior(u.name);
            break;
        case Unit::Trait::Mage:
            unit = new Mage(u.name);
            break;
        case Unit::Trait::Archer:
            unit = new Archer(u.name);
            break;
        case Unit::Trait::Boss:
            unit = new Boss(u.name);
            break;
        default:
            unit = new Unit(u.name, Unit::Trait::None);
            break;
        }

        // 恢复属性和等级
        unit->setHp(u.maxHp);      // 恢复为满血
        unit->setMaxHp(u.maxHp);
        unit->setAtk(u.atk);
        unit->setRange(u.range);
        unit->setMaxMana(u.maxMana);
        unit->setMana(u.maxMana);  // 恢复为满蓝
        unit->setOwner(Unit::Owner::PlayerCtrl);
        unit->setStatus(Unit::Status::Idle);
        unit->setTrait(u.trait);
        unit->setLevel(u.level);

        // 恢复装备。
        if (u.equipmentType != Equipment::Type::None) {
            unit->setEquipment(new Equipment(u.equipmentType));
        }

        m_units.append(unit);
        
        // 把它放回棋盘
        if (m_board.isValidPosition(u.position)) {
            m_board.addUnit(unit, u.position);
        }

        attachUnitItemFor(unit);
    }

    emit playerInfoChanged();
}

// 敌方轮次生成：第一回合生成2个，之后每回合增加2个（最多8个），并根据阶段属性增幅，第5波追加1个最终Boss。
void Game::spawnEnemiesForCurrentRound()
{
    const int availableSlots = m_enemyUnitCap - m_enemyUnitCount;
    if (availableSlots <= 0) {
        return;
    }

    auto attachUnitItemFor = [this](Unit* unit) {
        UnitItem* unitItem = new UnitItem(unit);
        unitItem->setZValue(kZUnit);
        m_scene->addItem(unitItem);
        m_unitItems.push_back(unitItem);
        m_unitItemById[unit->id()] = unitItem;

        connect(unitItem, &UnitItem::dragStarted,
                this, &Game::handleDragStarted);
        connect(unitItem, &UnitItem::dragMoved,
                this, &Game::handleDragMoved);
        connect(unitItem, &UnitItem::dragDropped,
                this, &Game::handleDropCommand);
        connect(unitItem, &UnitItem::eqDragStarted,
                this, &Game::handleUnitEqDragStarted);
        connect(unitItem, &UnitItem::eqDragMoved,
                this, &Game::handleUnitEqDragMoved);
        connect(unitItem, &UnitItem::eqDragDropped,
                this, &Game::handleUnitEqDragDropped);
    };

    // 常规敌人数量：第一回合2个，之后每回合+2，最多8个
    const int normalCount = qMin(8, m_round * 2);
    const bool spawnBoss = (m_enemyDefeatedWaves >= 4);
    const int totalEnemies = normalCount + (spawnBoss ? 1 : 0);

    // 属性增幅：每增加一关增加10%
    const double multiplier = 1.0 + 0.1 * qMax(0, m_stage - 1);

    const QPoint normalPositions[] = {
        QPoint(2, 0), QPoint(3, 0), QPoint(4, 0), QPoint(1, 0), QPoint(5, 0),
        QPoint(0, 0), QPoint(2, 1), QPoint(4, 1)
    };
    const QPoint bossPositions[] = {
        QPoint(3, 2), QPoint(2, 2), QPoint(4, 2), QPoint(3, 1), QPoint(1, 1)
    };

    int spawned = 0;
    for (int i = 0; i < totalEnemies && spawned < availableSlots; ++i) {
        Unit* newEnemy = nullptr;
        const QPoint* prefArr = nullptr;
        int prefCount = 0;

        bool isBoss = spawnBoss && (i == normalCount);

        if (isBoss) {
            // Boss
            newEnemy = new Boss(QString::fromUtf8("最终Boss"), 200 * multiplier, 30 * multiplier);

            prefArr = bossPositions;
            prefCount = 5;
        } else {
            // 生成顺序按战士→弓手→法师循环
            const int typeIdx = i % 3;
            if (typeIdx == 0) {
                newEnemy = new Warrior(QString::fromUtf8("敌方战士"), 150 * multiplier, 15 * multiplier);
            } else if (typeIdx == 1) {
                newEnemy = new Archer(QString::fromUtf8("敌方弓手"), 100 * multiplier, 15 * multiplier);
            } else {
                newEnemy = new Mage(QString::fromUtf8("敌方法师"), 80 * multiplier, 23 * multiplier);
            }

            if (i < 8) {
                prefArr = &normalPositions[i];
                prefCount = 1;
            }
        }

        if (!newEnemy) {
            continue;
        }
        newEnemy->setOwner(Unit::Owner::EnemyCtrl);

        bool placed = false;

        // 1. 尝试首选预设位置
        for (int p = 0; p < prefCount; ++p) {
            if (m_board.addUnit(newEnemy, prefArr[p])) {
                placed = true;
                break;
            }
        }

        // 2. 预设位置均被占用时，回退到敌方半场（上半区0~R/2）任意空格
        if (!placed) {
            for (int row = 0; row < Board::ROWS / 2 && !placed; ++row) {
                for (int col = 0; col < Board::COLS; ++col) {
                    if (m_board.addUnit(newEnemy, QPoint(col, row))) {
                        placed = true;
                        break;
                    }
                }
            }
        }

        // 3. 最终落子确认：入场或丢弃回收内存
        if (placed) {
            m_units.append(newEnemy);
            attachUnitItemFor(newEnemy);
            ++m_enemyUnitCount;
            ++spawned;
        } else {
            delete newEnemy;
        }
    }
}

// 对应阶段一「统一 Unit 类型，仅用 owner 区分敌我」的单位检索基础能力。
Unit* Game::findUnitById(int unitId) const
{
    for (Unit* unit : m_units) {
        if (unit && unit->id() == unitId) {
            return unit;
        }
    }
    return nullptr;
}

// 对应阶段一「实现棋盘、半场划分与地块占用规则」所需的格子对象定位。
GridItem* Game::findGridItem(const QPoint& gridPos) const
{
    for (GridItem* item : m_gridItems) {
        if (item && item->gridPos() == gridPos) {
            return item;
        }
    }
    return nullptr;
}

// 对应阶段一「GUI 展示单位信息」的单位图元定位能力。
UnitItem* Game::findUnitItem(int unitId) const
{
    auto it = m_unitItemById.find(unitId);
    if (it == m_unitItemById.end()) {
        return nullptr;
    }
    return it->second;
}

// 对应阶段一「实现拖拽摆放与非法放置处理」中的视觉提示清理。
void Game::clearGridHighlights()
{
    for (GridItem* item : m_gridItems) {
        if (!item) {
            continue;
        }
        item->setHoverActive(false);
        item->setDropActive(false);
    }
}

// 重置备战区高亮为默认颜色。
void Game::clearBenchHighlights()
{
    for (QGraphicsRectItem* item : m_benchItems) {
        if (!item) {
            continue;
        }
        item->setBrush(m_benchSlotColor);
    }
}

// 判断单位是否可落在玩家半场的目标格。
bool Game::canDropOnBoard(Unit* unit, const QPoint& target) const
{
    if (!unit) {
        return false;
    }
    if (!m_board.isValidPosition(target) || !m_board.isPlayerHalf(target)) {
        return false;
    }

    const bool alreadyOnBoard = isUnitOnBoard(unit);
    if (!alreadyOnBoard && countAliveUnits(Unit::Owner::PlayerCtrl) >= m_player.PopulationCap()
        && !m_board.getUnitAt(target))
    {
        return false; // 人口已满且单位不在棋盘上时，不允许放置新单位到空格
    }

    Unit* targetUnit = m_board.getUnitAt(target);
    if (targetUnit && targetUnit->owner() != Unit::Owner::PlayerCtrl) {
        return false;
    }
    return true;
}

// 判断单位是否可落在指定备战区槽位。
bool Game::canDropOnBench(Unit* unit, int slot, bool fromBench) const
{
    if (!unit || slot < 0 || slot >= m_bench.capacity()) {
        return false;
    }

    return true;
}

// 对应阶段一「半场划分与地块占用规则」以及「非法放置处理」的统一合法性校验。
bool Game::canApplyDrop(int unitId, const QPoint& source, const QPoint& target) const
{
    Unit* unit = findUnitById(unitId);
    if (!unit) {
        return false;
    }

    if (unit->owner() != Unit::Owner::PlayerCtrl) {
        return false;
    }

    if (!m_board.isValidPosition(source)) {
        return false;
    }

    if (!m_board.isPlayerHalf(source)) {
        return false;
    }

    if (m_board.getUnitAt(source) != unit) {
        return false;
    }

    return canDropOnBoard(unit, target) && source != target;
}

// 对应阶段一「备战区与棋盘之间的数据同步」与拖拽结果写回棋盘数据。
bool Game::applyDrop(int unitId, const QPoint& target)
{
    Unit* unit = findUnitById(unitId);
    if (!unit) {
        return false;
    }

    Unit* targetUnit = m_board.getUnitAt(target);
    if (targetUnit && targetUnit != unit) {
        const QPoint sourcePos = unit->position();
        m_board.removeUnit(targetUnit);
        m_board.removeUnit(unit);
        m_board.addUnit(targetUnit, sourcePos);
        m_board.addUnit(unit, target);
        return true;
    }

    return m_board.moveUnit(unit, target);
}

// 新增备战区相关内容
// 对应阶段一「备战区与棋盘之间的数据同步」：将棋盘单位放入备战区指定槽位。
bool Game::transferUnitFromBoardToBench(int unitId, int benchSlot)
{
    Unit* unit = findUnitById(unitId);
    if (!unit) {
        return false;
    }

    Unit* targetUnit = m_bench.unitAt(benchSlot);
    const QPoint boardPos = unit->position();

    if (targetUnit) {
        m_bench.removeUnit(targetUnit);
        m_board.removeUnit(unit);
        m_board.addUnit(targetUnit, boardPos);
        m_bench.addUnit(unit, benchSlot);
        return true;
    }

    if (!m_board.removeUnit(unit)) {
        return false;
    }

    if (m_bench.addUnit(unit, benchSlot)) {
        return true;
    }

    // 回滚：备战区写入失败时，恢复到原棋盘位置。
    m_board.addUnit(unit, boardPos);
    return false;
}

//新增备战区相关内容
// 对应阶段一「备战区与棋盘之间的数据同步」：将备战区单位放入棋盘目标格。
bool Game::transferUnitFromBenchToBoard(int unitId, const QPoint& target)
{
    Unit* unit = findUnitById(unitId);
    if (!unit || !m_bench.contains(unit)) {
        return false;
    }

    const int benchSlot = m_bench.slotOf(unit);
    Unit* targetUnit = m_board.getUnitAt(target);

    if (targetUnit) {
        m_board.removeUnit(targetUnit);
        m_bench.removeUnit(unit);
        m_bench.addUnit(targetUnit, benchSlot);
        m_board.addUnit(unit, target);
        return true;
    }

    if (!m_board.addUnit(unit, target)) {
        return false;
    }

    if (m_bench.removeUnit(unit)) {
        return true;
    }

    // 回滚：若从备战区移除失败，撤销刚刚写入的棋盘状态。
    m_board.removeUnit(unit);
    return false;
}

// 备战区内部交换或移动。
bool Game::moveUnitWithinBench(int fromSlot, int toSlot)
{
    return m_bench.moveWithin(fromSlot, toSlot);
}

// 对应阶段一「GUI 展示棋盘/备战区/单位信息」的场景构建。
void Game::buildScene()
{
    // 清空旧场景内容与相关缓存，准备重建。
    m_scene->clear();
    m_gridItems.clear();
    m_unitItems.clear();
    m_unitItemById.clear();
    m_benchItems.clear();
    m_benchRects.clear();
    m_benchLabel = nullptr;
    m_shopProxy = nullptr;
    // m_scene->clear() 已销毁装备 UI 相关图元，必须同步重置这些缓存指针，
    // 否则后续 buildEquipmentUI() 中的清理逻辑会对悬空指针执行 delete 导致崩溃。
    m_eqSlotItems.clear();
    m_eqTitleLabel = nullptr;
    m_dragEqIcon = nullptr;

    // 绘制六边形网格
    QRectF totalBounds;
    bool first = true;
    for (int row = 0; row < Board::ROWS; ++row) {
        for (int col = 0; col < Board::COLS; ++col) {
            const QPolygonF poly = cellHexPolygon(row, col);
            GridItem* gridItem = new GridItem(row, col, poly);
            gridItem->setZValue(kZGrid);
            gridItem->setBaseColor(row < Board::ROWS / 2 ? QColor(80, 60, 60) : QColor(60, 60, 80));

            m_scene->addItem(gridItem);
            m_gridItems.push_back(gridItem);

            const QRectF bounds = gridItem->boundingRect();
            totalBounds = first ? bounds : totalBounds.united(bounds);
            first = false;
        }
    }

    // 计算备战区参数与布局，并绘制备战区槽位
    const qreal colSpacing = m_radius * qSqrt(3.0);
    const qreal boardLeft = gridToWorld(0, 0).x() - m_radius;
    const qreal boardRight = gridToWorld(0, m_cols - 1).x() + m_radius + colSpacing * 0.5;
    const qreal boardBottom = gridToWorld(m_rows - 1, 0).y() + m_radius;
    const qreal slotSize = 70.0;
    const qreal slotSpacing = 12.0;
    const qreal benchTop = boardBottom + 70.0;
    const int benchSlots = m_bench.capacity();
    const qreal totalBenchWidth = benchSlots * slotSize + (benchSlots - 1) * slotSpacing;
    const qreal benchLeft = boardLeft + (boardRight - boardLeft - totalBenchWidth) * 0.5;

    m_benchTop = benchTop;
    m_separatorY = boardBottom + 28.0;

    QGraphicsLineItem* separator = new QGraphicsLineItem(boardLeft, m_separatorY, boardRight, m_separatorY);
    separator->setZValue(kZGrid);
    separator->setPen(QPen(QColor(90, 90, 110), 2));
    m_scene->addItem(separator);
    totalBounds = totalBounds.united(QRectF(boardLeft, m_separatorY - 1.0, boardRight - boardLeft, 2.0));

    for (int i = 0; i < benchSlots; ++i) {
        const qreal x = benchLeft + i * (slotSize + slotSpacing);
        QRectF rect(x, benchTop, slotSize, slotSize);
        QGraphicsRectItem* slotItem = new QGraphicsRectItem(rect);
        slotItem->setZValue(kZGrid);
        slotItem->setBrush(m_benchSlotColor);
        slotItem->setPen(QPen(QColor(80, 80, 100), 2));
        m_scene->addItem(slotItem);
        m_benchItems.push_back(slotItem);
        m_benchRects.push_back(rect);
        totalBounds = totalBounds.united(rect);
    }

    m_benchLabel = m_scene->addSimpleText(QString::fromUtf8("备战区"));
    if (m_benchLabel) {
        m_benchLabel->setBrush(QColor(220, 220, 220));
        m_benchLabel->setPos(benchLeft, benchTop - 40.0);
        m_benchLabel->setZValue(kZGrid);
    }

    // 为每个单位创建对应的 UnitItem，并连接拖拽信号。
    for (Unit* unit : m_units) {
        UnitItem* unitItem = new UnitItem(unit);
        unitItem->setZValue(kZUnit);
        m_scene->addItem(unitItem);
        m_unitItems.push_back(unitItem);
        m_unitItemById[unit->id()] = unitItem;

        connect(unitItem, &UnitItem::dragStarted,
                this, &Game::handleDragStarted);
        connect(unitItem, &UnitItem::dragMoved,
                this, &Game::handleDragMoved);
        connect(unitItem, &UnitItem::dragDropped,
                this, &Game::handleDropCommand);
        connect(unitItem, &UnitItem::eqDragStarted,
                this, &Game::handleUnitEqDragStarted);
        connect(unitItem, &UnitItem::eqDragMoved,
                this, &Game::handleUnitEqDragMoved);
        connect(unitItem, &UnitItem::eqDragDropped,
                this, &Game::handleUnitEqDragDropped);
    }
    
    buildShopUI();
    buildEquipmentUI();

    // 调整场景边界以适应所有内容，留出额外空间避免视觉拥挤。
    if (m_shopProxy) {
        totalBounds = totalBounds.united(m_shopProxy->boundingRect().translated(m_shopProxy->pos()));
    }
    m_scene->setSceneRect(totalBounds.adjusted(-40, -60, 40, 60));
}

// 清空布局内所有子控件，用于updateShopUI快速刷新商品格子内容。
// 使用deleteLater延迟删除，避免在控件自身的槽函数中同步删除导致崩溃。
static void clearLayout(QLayout* layout)
{
    if (!layout) return;
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void Game::buildShopUI()
{
    if (m_shopProxy) {
        m_scene->removeItem(m_shopProxy);
        m_shopProxy->deleteLater();
        m_shopProxy = nullptr;
    }

    m_shopProductLayouts.clear();
    m_shopRefreshBtn = nullptr;

    QWidget* shopWidget = new QWidget();
    shopWidget->setStyleSheet("#shopWidget { background: transparent; color: #f2f2f2; } "
                              "QPushButton { background: #3a3a3a; border: 1px solid #555; padding: 4px; border-radius: 3px; font-size: 10px; color: #f2f2f2; } "
                              "QPushButton:hover:!disabled { background: #6a8a6a; border: 1px solid #8a8a8a; } "
                              "QPushButton:disabled { background: #2f2f2f; color: #888; border: 1px solid #444; } "
                              "QLabel { font-size: 11px; color: #f2f2f2; }");
    shopWidget->setObjectName("shopWidget");
    QVBoxLayout* shopLayout = new QVBoxLayout(shopWidget);
    shopLayout->setContentsMargins(0, 0, 0, 0);
    shopLayout->setSpacing(6);

    // 商店UI标题和刷新按钮布局
    QHBoxLayout* topLayout = new QHBoxLayout();
    QLabel* shopTitle = new QLabel(QString::fromUtf8("商店"));
    shopTitle->setAlignment(Qt::AlignCenter);
    shopTitle->setStyleSheet("font-weight: bold; font-size: 16px; background: rgba(50,50,50,200); border-radius: 4px; padding: 2px;");

    QPushButton* refreshBtn = new QPushButton(QString::fromUtf8("刷新商店 (2金币)"));
    refreshBtn->setStyleSheet("QPushButton { background: #4a3a3a; font-size: 11px; padding: 6px; border: 1px solid #555; border-radius: 3px; }"
                              "QPushButton:hover:!disabled { background: #6a8a6a; border: 1px solid #8a8a8a; }"
                              "QPushButton:disabled { background: #2f2f2f; color: #888; border: 1px solid #444; }");
    refreshBtn->setEnabled(!m_inBattle);
    m_shopRefreshBtn = refreshBtn;
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        if (!m_inBattle && m_player.Gold() >= 2) {
            m_shop.refresh(this);
            emit playerInfoChanged();
            updateShopUI();
        }
    });

    topLayout->addWidget(shopTitle);
    topLayout->addWidget(refreshBtn);
    shopLayout->addLayout(topLayout);

    QHBoxLayout* blocksLayout = new QHBoxLayout();
    blocksLayout->setSpacing(10);

    // 五个单位格子，仅创建基础容器，内容由updateShopUI动态填充
    for (int i = 0; i < 5; ++i) {
        QWidget* block = new QWidget();
        block->setObjectName("productBlock");
        block->setFixedSize(90, 110);
        block->setStyleSheet("#productBlock { background: rgba(60, 60, 80, 200); border: 2px solid #5a5a7a; border-radius: 5px; }");
        QVBoxLayout* blockLayout = new QVBoxLayout(block);
        blockLayout->setContentsMargins(4, 4, 4, 4);
        m_shopProductLayouts.push_back(blockLayout);
        blocksLayout->addWidget(block);
    }

    // 升级人口格子
    {
        QWidget* block = new QWidget();
        block->setObjectName("upgradeBlock");
        block->setFixedSize(90, 110);
        block->setStyleSheet("#upgradeBlock { background: rgba(80, 60, 60, 200); border: 2px solid #7a5a5a; border-radius: 5px; }");
        QVBoxLayout* blockLayout = new QVBoxLayout(block);
        blockLayout->setContentsMargins(4, 4, 4, 4);

        QLabel* nameLabel = new QLabel(QString::fromUtf8("升级人口"));
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("border: none; background: transparent; font-weight: bold; font-size: 12px;");

        QLabel* descLabel = new QLabel(QString::fromUtf8("人口上限+2"));
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setStyleSheet("border: none; background: transparent; font-size: 10px; margin-top: 2px;");

        QPushButton* buyBtn = new QPushButton(QString::fromUtf8("购买 (5金币)"));
        buyBtn->setEnabled(!m_inBattle);
        m_shopUpgradeBtn = buyBtn;
        connect(buyBtn, &QPushButton::clicked, this, [this]() {
            if(m_inBattle) return;
            m_shop.upgradeCapacity(this);
            emit playerInfoChanged();
        });

        blockLayout->addWidget(nameLabel);
        blockLayout->addWidget(descLabel);
        blockLayout->addStretch();
        blockLayout->addWidget(buyBtn);

        blocksLayout->addWidget(block);
    }

    // 出售单位格子
    {
        QWidget* block = new QWidget();
        block->setObjectName("sellBlock");
        block->setFixedSize(90, 110);
        block->setStyleSheet("#sellBlock { background: rgba(80, 80, 60, 200); border: 2px solid #7a7a5a; border-radius: 5px; }");
        QVBoxLayout* blockLayout = new QVBoxLayout(block);
        blockLayout->setContentsMargins(4, 4, 4, 4);

        QLabel* nameLabel = new QLabel(QString::fromUtf8("出售单位"));
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("border: none; background: transparent; font-weight: bold; font-size: 14px; color: #ffaa55;");

        QLabel* descLabel = new QLabel(QString::fromUtf8("拖拽至此以出售\n获得1金币"));
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setStyleSheet("border: none; background: transparent; font-size: 10px; margin-top: 10px; color: #aaaaaa;");

        blockLayout->addStretch();
        blockLayout->addWidget(nameLabel);
        blockLayout->addWidget(descLabel);
        blockLayout->addStretch();

        m_shopSellWidget = block; // 记录出售区域的 widget 指针以便高亮
        blocksLayout->addWidget(block);
    }

    shopLayout->addLayout(blocksLayout);

    m_shopProxy = m_scene->addWidget(shopWidget);
    m_shopProxy->setZValue(kZGrid + 0.1);

    // 根据备战区位置计算商店坐标
    qreal boardLeft = gridToWorld(0, 0).x() - m_radius;
    qreal shopX = boardLeft - 20;
    qreal shopY = m_benchTop + 240.0;
    m_shopProxy->setPos(shopX, shopY);

    // 计算出售区域判定矩形（布局margin:0, spacing:10, 每个格子宽90）
    // 5个商品 + 1个升级 = 6个格子，第7个格子（出售）起始x = 6 * (90 + 10) = 600
    // topLayout高度约30px，出售格子相对坐标: x=600, y≈35, 宽90, 高110
    m_shopSellRect = QRectF(shopX + 600, shopY + 36, 90, 110);

    // 首次填充商品格子内容
    updateShopUI();
}

void Game::updateShopUI()
{
    if (m_shopProductLayouts.empty()) return;

    for (int i = 0; i < 5; ++i) {
        QVBoxLayout* layout = m_shopProductLayouts[i];
        clearLayout(layout);

        UnitInfo* info = m_shop.products().at(i);
        if (info && info->hp > 0) {
            QLabel* nameLabel = new QLabel(info->name);
            nameLabel->setAlignment(Qt::AlignCenter);
            nameLabel->setStyleSheet("border: none; background: transparent; font-weight: bold; font-size: 12px;");

            QString traitStr;
            if (info->trait == Unit::Trait::Warrior) traitStr = QString::fromUtf8("战士");
            else if (info->trait == Unit::Trait::Mage) traitStr = QString::fromUtf8("法师");
            else if (info->trait == Unit::Trait::Archer) traitStr = QString::fromUtf8("弓手");
            else traitStr = QString::fromUtf8("无");

            QLabel* statLabel = new QLabel(QString("%1\nHP: %2\nATK: %3").arg(traitStr).arg(info->hp).arg(info->atk));
            statLabel->setAlignment(Qt::AlignCenter);
            statLabel->setStyleSheet("border: none; background: transparent; font-size: 10px; margin-top: 2px;");

            QPushButton* buyBtn = new QPushButton(QString::fromUtf8("购买 (3金币)"));
            buyBtn->setEnabled(!m_inBattle);
            connect(buyBtn, &QPushButton::clicked, this, [this, i]() {
                if(m_inBattle) return;
                m_shop.buyUnit(i, this);
                emit playerInfoChanged();
                updateShopUI();
            });

            layout->addWidget(nameLabel);
            layout->addWidget(statLabel);
            layout->addStretch();
            layout->addWidget(buyBtn);
        } else {
            QLabel* emptyLabel = new QLabel(QString::fromUtf8("已售出"));
            emptyLabel->setAlignment(Qt::AlignCenter);
            emptyLabel->setStyleSheet("border: none; background: transparent; color: #777;");
            layout->addWidget(emptyLabel);
        }
    }

    if (m_shopRefreshBtn) {
        m_shopRefreshBtn->setEnabled(!m_inBattle);
    }
    if (m_shopUpgradeBtn) {
        m_shopUpgradeBtn->setEnabled(!m_inBattle);
    }
}

void Game::buildEquipmentUI()
{
    for (auto slot : m_eqSlotItems) {
        m_scene->removeItem(slot);
        delete slot;
    }
    m_eqSlotItems.clear();

    if (m_eqTitleLabel) {
        m_scene->removeItem(m_eqTitleLabel);
        delete m_eqTitleLabel;
        m_eqTitleLabel = nullptr;
    }
    if (m_dragEqIcon) {
        m_scene->removeItem(m_dragEqIcon);
        delete m_dragEqIcon;
        m_dragEqIcon = nullptr;
    }

    qreal boardLeft = gridToWorld(0, 0).x() - m_radius;
    qreal shopX = boardLeft - 20;
    qreal eqX = shopX; // 将装备栏放置在原商店位置
    qreal eqY = m_benchTop + 90.0;

    m_eqTitleLabel = new QGraphicsSimpleTextItem(QString::fromUtf8("装备栏"));
    QFont f = m_eqTitleLabel->font();
    f.setPointSize(16);
    f.setBold(true);
    m_eqTitleLabel->setFont(f);
    m_eqTitleLabel->setBrush(QColor(240, 240, 240));
    m_eqTitleLabel->setPos(eqX, eqY);
    m_scene->addItem(m_eqTitleLabel);

    for (int i = 0; i < 5; ++i) {
        auto slot = new EquipmentSlotItem(i);
        slot->setPos(eqX + i * 100, eqY + 36);
        m_scene->addItem(slot);
        m_eqSlotItems.push_back(slot);

        connect(slot, &EquipmentSlotItem::discardClicked, 
            this, &Game::handleEqDiscardClicked);
        connect(slot, &EquipmentSlotItem::dragStarted, 
            this, &Game::handleEqDragStarted);
        connect(slot, &EquipmentSlotItem::dragMoved, 
            this, &Game::handleEqDragMoved);
        connect(slot, &EquipmentSlotItem::dragDropped, 
            this, &Game::handleEqDragDropped);
    }

    updateEquipmentUI();
}

void Game::updateEquipmentUI()
{
    for (int i = 0; i < 5; ++i) {
        m_eqSlotItems[i]->setEquipment(m_player.inventory(i));
    }
}

void Game::handleEqDiscardClicked(int slotIndex)
{
    m_player.removeInventory(slotIndex);
    updateEquipmentUI();
    emit playerInfoChanged();
}

void Game::handleEqDragStarted(int slotIndex, QPointF scenePos)
{
    Equipment* eq = m_player.inventory(slotIndex);
    if (!eq) return;

    m_dragEqActive = true;
    m_activeEqSlotIndex = slotIndex;

    if (!m_dragEqIcon) {
        m_dragEqIcon = new QGraphicsSimpleTextItem(QString::fromUtf8("📦"));
        QFont f = m_dragEqIcon->font();
        f.setPointSize(18);
        m_dragEqIcon->setFont(f);
        m_dragEqIcon->setBrush(QColor(255, 200, 100));
        m_scene->addItem(m_dragEqIcon);
    }
    
    m_dragEqIcon->setPos(scenePos);
    m_dragEqIcon->setZValue(kZDraggingUnit + 1.0);
    m_dragEqIcon->show();
}

void Game::handleEqDragMoved(int slotIndex, QPointF scenePos)
{
    if (!m_dragEqActive || m_activeEqSlotIndex != slotIndex) return;

    if (m_dragEqIcon) {
        m_dragEqIcon->setPos(scenePos - QPointF(12, 12));
    }
    
    UnitItem* targetItem = nullptr;
    for (auto item : m_unitItems) {
        if (item->boundingRect().translated(item->pos()).contains(scenePos)) {
            if (item->unit() && item->unit()->owner() == Unit::Owner::PlayerCtrl) {
                targetItem = item;
                break;
            }
        }
    }
    
    for (auto item : m_unitItems) {
        if (item->unit() && item->unit()->owner() == Unit::Owner::PlayerCtrl) {
            item->setActive(item == targetItem);
        }
    }
}

void Game::handleEqDragDropped(int slotIndex, QPointF scenePos)
{
    if (!m_dragEqActive || m_activeEqSlotIndex != slotIndex) return;

    m_dragEqActive = false;
    m_activeEqSlotIndex = -1;

    if (m_dragEqIcon) {
        m_dragEqIcon->hide();
    }
    
    UnitItem* targetItem = nullptr;
    for (auto item : m_unitItems) {
        item->setActive(false);
        if (item->boundingRect().translated(item->pos()).contains(scenePos)) {
            if (item->unit() && item->unit()->owner() == Unit::Owner::PlayerCtrl) {
                targetItem = item;
            }
        }
    }
    
    if (targetItem) {
        Unit* u = targetItem->unit();
        Equipment* dropEq = m_player.inventory(slotIndex);
        Equipment* oldEq = u->equipment();
        
        u->setEquipment(dropEq);
        m_player.setInventory(slotIndex, oldEq);
        
        updateEquipmentUI();
        targetItem->update();
        emit playerInfoChanged();
    }
}

void Game::handleUnitEqDragStarted(int unitId, QPointF scenePos)
{
    Unit* unit = findUnitById(unitId);
    if (!unit || !unit->equipment()) return;

    m_dragEqActive = true;
    m_activeEqSlotIndex = -1; // -1 表示拖拽源是单位而非装备栏

    if (!m_dragEqIcon) {
        m_dragEqIcon = new QGraphicsSimpleTextItem(QString::fromUtf8("📦"));
        QFont f = m_dragEqIcon->font();
        f.setPointSize(18);
        m_dragEqIcon->setFont(f);
        m_dragEqIcon->setBrush(QColor(255, 200, 100));
        m_scene->addItem(m_dragEqIcon);
    }

    m_dragEqIcon->setPos(scenePos);
    m_dragEqIcon->setZValue(kZDraggingUnit + 1.0);
    m_dragEqIcon->show();
}

void Game::handleUnitEqDragMoved(int unitId, QPointF scenePos)
{
    if (!m_dragEqActive) return;

    if (m_dragEqIcon) {
        m_dragEqIcon->setPos(scenePos - QPointF(12, 12));
    }

    // 高亮有空位的装备栏槽位
    for (int i = 0; i < 5; ++i) {
        if (m_eqSlotItems[i]) {
            m_eqSlotItems[i]->setActive(m_player.inventory(i) == nullptr);
        }
    }

    // 高亮鼠标下方的玩家单位（作为装备互换目标）
    int targetUnitId = -1;
    for (auto item : m_unitItems) {
        if (!item || !item->unit()) continue;
        if (item->unit()->owner() != Unit::Owner::PlayerCtrl) continue;
        if (item->unit()->id() == unitId) continue;
        if (item->boundingRect().translated(item->pos()).contains(scenePos)) {
            targetUnitId = item->unit()->id();
            break;
        }
    }

    for (auto item : m_unitItems) {
        if (item && item->unit()) {
            item->setActive(item->unit()->id() == targetUnitId);
        }
    }
}

void Game::handleUnitEqDragDropped(int unitId, QPointF scenePos)
{
    if (!m_dragEqActive) return;

    m_dragEqActive = false;
    m_activeEqSlotIndex = -1;

    if (m_dragEqIcon) {
        m_dragEqIcon->hide();
    }

    // 清除装备栏高亮
    for (int i = 0; i < 5; ++i) {
        if (m_eqSlotItems[i]) {
            m_eqSlotItems[i]->setActive(false);
        }
    }

    // 清除单位高亮
    for (auto item : m_unitItems) {
        if (item) item->setActive(false);
    }

    Unit* srcUnit = findUnitById(unitId);
    if (!srcUnit || !srcUnit->equipment()) return;

    // 1. 判断是否落在某个装备栏槽位上
    int targetSlot = -1;
    for (int i = 0; i < 5; ++i) {
        if (m_eqSlotItems[i] && m_eqSlotItems[i]->contains(m_eqSlotItems[i]->mapFromScene(scenePos))) {
            targetSlot = i;
            break;
        }
    }

    if (targetSlot >= 0 && !m_player.inventory(targetSlot)) {
        // 装备栏该槽位为空，可以放入
        Equipment* eq = srcUnit->equipment();
        srcUnit->setEquipment(nullptr);
        m_player.setInventory(targetSlot, eq);

        updateEquipmentUI();
        // 刷新单位图元显示（移除装备标签）
        UnitItem* item = findUnitItem(unitId);
        if (item) item->update();
        emit playerInfoChanged();
        return;
    }

    // 2. 判断是否落在另一个玩家单位上（装备互换）
    UnitItem* targetItem = nullptr;
    for (auto item : m_unitItems) {
        if (item->boundingRect().translated(item->pos()).contains(scenePos)) {
            if (item->unit() && item->unit()->owner() == Unit::Owner::PlayerCtrl
                && item->unit()->id() != unitId) {
                targetItem = item;
                break;
            }
        }
    }

    if (targetItem) {
        Unit* dstUnit = targetItem->unit();
        Equipment* srcEq = srcUnit->equipment();
        Equipment* dstEq = dstUnit->equipment();

        // 源单位装上目标单位的装备（可能为 nullptr）
        // 先设 nullptr 再装，避免 setEquipment 内部卸装逻辑误操作
        srcUnit->setEquipment(nullptr);
        if (dstEq) dstUnit->setEquipment(nullptr);

        srcUnit->setEquipment(dstEq);
        dstUnit->setEquipment(srcEq);

        updateEquipmentUI();
        // 刷新两个单位的图元显示
        UnitItem* srcItem = findUnitItem(unitId);
        if (srcItem) srcItem->update();
        targetItem->update();
        emit playerInfoChanged();
    }
}

// 对应阶段一「实现备战区与棋盘之间的数据同步」的数据到视图同步。
void Game::syncFromBoard()
{
    clearGridHighlights();

    for (UnitItem* item : m_unitItems) {
        if (!item || !item->unit()) {
            continue;
        }

        const QPoint pos = item->unit()->position();
        if (m_board.isValidPosition(pos) && m_board.getUnitAt(pos) == item->unit()) {
            item->setVisible(true);
            item->setGridPos(pos);
            item->setBenchSlot(-1);
            item->setPos(gridToWorld(pos.y(), pos.x()));
            item->setZValue(kZUnit);
            continue;
        }

        if (m_bench.contains(item->unit())) {
            const int slot = m_bench.slotOf(item->unit());
            if (slot >= 0 && slot < static_cast<int>(m_benchRects.size())) {
                const QRectF rect = m_benchRects[slot];
                item->setVisible(true);
                item->setGridPos(QPoint(-1, -1));
                item->setBenchSlot(slot);
                item->setPos(rect.center());
                item->setZValue(kZUnit);
                continue;
            }
        }

        item->setVisible(false);
    }
    refreshTraitCounts();
}

int Game::traitIndex(Unit::Trait trait)
{
    // 将羁绊枚举压缩为数组下标。
    switch (trait) {
    case Unit::Trait::None:
        return 0;
    case Unit::Trait::Warrior:
        return 1;
    case Unit::Trait::Mage:
        return 2;
    case Unit::Trait::Archer:
        return 3;
    default:
        return -1;
    }
}

Unit::Trait Game::traitForUnit(const Unit* unit)
{
    // 直接读取单位自身的羁绊类型。
    if (!unit) {
        return Unit::Trait::None;
    }

    return unit->trait();
}

// 对应阶段一棋盘 GUI 展示：将逻辑格坐标转换为场景坐标用于渲染。
QPointF Game::gridToWorld(int row, int col) const
{
    const qreal colSpacing = m_radius * qSqrt(3.0);
    const qreal xOffset = (row % 2 == 0) ? colSpacing * 0.5 : 0.0;
    const qreal x = xOffset + col * colSpacing;
    const qreal y = row * m_rowSpacing;
    return QPointF(x, y);
}

// 对应阶段一拖拽交互：将鼠标场景坐标映射回最近逻辑格坐标。
QPoint Game::worldToGrid(const QPointF& world) const
{
    QPoint best(-1, -1);
    qreal bestDist = 1e18;

    for (int row = 0; row < m_rows; ++row) {
        for (int col = 0; col < m_cols; ++col) {
            const QPointF center = gridToWorld(row, col);
            const qreal dx = world.x() - center.x();
            const qreal dy = world.y() - center.y();
            const qreal d2 = dx * dx + dy * dy;
            if (d2 < bestDist) {
                bestDist = d2;
                best = QPoint(col, row);
            }
        }
    }

    return best;
}

// 对应阶段一棋盘 GUI 展示：生成六边形地块轮廓以绘制棋盘网格。
QPolygonF Game::cellHexPolygon(int row, int col) const
{
    const QPointF center = gridToWorld(row, col);
    QPolygonF poly;
    poly.reserve(6);

    for (int i = 0; i < 6; ++i) {
        const qreal angleDeg = 60.0 * i - 90.0;
        const qreal angleRad = qDegreesToRadians(angleDeg);
        poly.append(QPointF(
            center.x() + m_radius * qCos(angleRad),
            center.y() + m_radius * qSin(angleRad)
        ));
    }

    return poly;
}

// 统计棋盘上存活的指定阵营单位数量。
int Game::countAliveUnits(Unit::Owner owner) const
{
    int count = 0;
    for (Unit* unit : m_units) {
        if (!unit || unit->owner() != owner) {
            continue;
        }
        if (unit->hp() <= 0) {
            continue;
        }
        const QPoint pos = unit->position();
        if (m_board.isValidPosition(pos) && m_board.getUnitAt(pos) == unit) {
            ++count;
        }
    }
    return count;
}

// 按规则对失败方总血量造成伤害，并发放金币奖励（含连胜/连败/利息经济体系）。
void Game::applyRoundDamage(Unit::Owner winner, int remainingUnits)
{
    const int damage = 10 * remainingUnits;
    
    if (winner == Unit::Owner::PlayerCtrl) {
        m_winStreak++;
        m_loseStreak = 0;
        // 基础奖励 + 连胜额外奖励：连胜x轮额外给予(x-1)金币
        m_player.setGold(m_player.Gold() + 8 + (m_winStreak - 1));
    } else {
        m_loseStreak++;
        m_winStreak = 0;
        // 基础奖励 + 连败额外奖励：连败x轮额外给予2*(x-1)金币
        m_player.setGold(m_player.Gold() + 5 + 2 * (m_loseStreak - 1));
    }

    // 利息结算：每有5金币额外获得1金币
    const int interest = m_player.Gold() / 5;
    if (interest > 0) {
        m_player.addGold(interest);
    }

    if (damage <= 0) {
        return;
    }

    if (winner == Unit::Owner::PlayerCtrl) {
        m_enemyDefeatedWaves = m_enemyDefeatedWaves + 1;
        emit enemyInfoChanged();
        if (m_enemyDefeatedWaves >= m_enemyMaxWaves) {
            emit enemyDefeated(); 
        }
        return;
    }

    m_player.reduceHp(damage);
    emit playerInfoChanged();
    if (m_player.Hp() <= 0) {
        emit gameOver(); // 需要定义此信号或进行处理。
    }
}

// 根据当前棋盘状态完成一次回合结算。
void Game::resolveRoundFromCurrentBoard()
{
    const int playerCount = countAliveUnits(Unit::Owner::PlayerCtrl);
    const int enemyCount = countAliveUnits(Unit::Owner::EnemyCtrl);

    if (playerCount == 0 && enemyCount == 0) {
        return;
    }

    if (enemyCount == 0 && playerCount > 0) {
        applyRoundDamage(Unit::Owner::PlayerCtrl, playerCount);
    } else if (playerCount == 0 && enemyCount > 0) {
        applyRoundDamage(Unit::Owner::EnemyCtrl, enemyCount);
    }

}

bool Game::hasAliveUnits(Unit::Owner owner) const
{
    return countAliveUnits(owner) > 0;
}

void Game::startBattleLoop()
{
    if (!m_battleTimer->isActive()) {
        m_battleTimer->start();
    }
}

void Game::stopBattleLoop()
{
    if (m_battleTimer->isActive()) {
        m_battleTimer->stop();
    }
}

void Game::battleTick()
{
    if (!m_inBattle) {
        stopBattleLoop();
        m_glovesExtraActPending = false;
        m_glovesExtraActUnitId = -1;
        return;
    }

    // 如果是手套单位的第二次行动
    bool isGlovesExtraAct = m_glovesExtraActPending;
    Unit* unit = nullptr;

    if (isGlovesExtraAct) {
        unit = findUnitById(m_glovesExtraActUnitId);
        // 如果单位已死亡或离场，取消第二次行动，回退为普通 tick 流程
        if (!unit || !isUnitOnBoard(unit) || unit->status() == Unit::Status::Dead) {
            m_glovesExtraActPending = false;
            m_glovesExtraActUnitId = -1;
            isGlovesExtraAct = false;
            unit = nextActingUnit();
        }
    }

    if (!unit) {
        unit = nextActingUnit();
    }

    if (unit) {
        setActiveUnitItem(unit->id());
        unit->act(this);

        // 手套单位：在一次正常行动后，调度第二次行动（120ms 后）并处理高亮闪烁
        if (!isGlovesExtraAct
            && unit->equipment() && unit->equipment()->type() == Equipment::Type::Gloves
            && isUnitOnBoard(unit) && unit->status() != Unit::Status::Dead) {
            m_glovesExtraActPending = true;
            m_glovesExtraActUnitId = unit->id();

            // 100ms 后取消高亮
            QTimer::singleShot(100, this, [this, uid = unit->id()]() {
                if (m_activeActionUnitId == uid)
                    setActiveUnitItem(-1);
            });
            // 150ms 后触发第二次行动
            QTimer::singleShot(150, this, [this]() {
                if (m_glovesExtraActPending && m_inBattle)
                    battleTick();
            });
        }

        if (isGlovesExtraAct) {
            m_glovesExtraActPending = false;
            m_glovesExtraActUnitId = -1;
        }
    } else {
        setActiveUnitItem(-1);
    }

    // 检查并集中移除血量归零的单位
    for (Unit* u : m_units) {
        if (u && u->hp() <= 0 && u->status() != Unit::Status::Dead) {
            u->setStatus(Unit::Status::Dead);
            // Temporarily 100% chance to drop equipment if enemy
            if (u->owner() == Unit::Owner::EnemyCtrl) {
                if (QRandomGenerator::global()->bounded(100) < 100) {
                    Equipment::Type types[] = { Equipment::Type::Sword, Equipment::Type::Crystal, Equipment::Type::armor, Equipment::Type::Gloves };
                    Equipment::Type randomType = types[QRandomGenerator::global()->bounded(4)];
                    m_player.addInventory(new Equipment(randomType));
                    updateEquipmentUI();
                    emit playerInfoChanged();
                }
            }
        }
        if (u && u->status() == Unit::Status::Dead) {
            requestRemoveUnit(u);
        }
    }
    flushUnitRemovals();

    // 更新单位位置与显示情况
    syncFromBoard();
    // 更新单位血量与图像显示
    for (UnitItem* item : m_unitItems) {
        if (item) {
            item->update();
        }
    }

    // 战斗结束检查（手套第二次行动也可能导致战斗结束）
    if (!hasAliveUnits(Unit::Owner::PlayerCtrl) || !hasAliveUnits(Unit::Owner::EnemyCtrl)) {
        stopBattleLoop();
        m_glovesExtraActPending = false;
        m_glovesExtraActUnitId = -1;
        setActiveUnitItem(-1);
        resolveRoundFromCurrentBoard();

        // 若 resolveRoundFromCurrentBoard 中 emit enemyDefeated →
        // onEnemyDefeated → startNextStage → reset() 已将 m_inBattle 置 false，
        // 说明阶段切换已完成，直接返回避免下方重复调用 startNextRound。
        if (!m_inBattle) {
            return;
        }

        m_inBattle = false;
        updateShopUI(); // 更新按钮状态
        emit battleStateChanged(m_inBattle);

        if (m_enemyDefeatedWaves < m_enemyMaxWaves && m_player.Hp() > 0) {
            startNextRound();
        }
    }
}

Unit* Game::nextActingUnit()
{
    if (m_units.isEmpty()) {
        return nullptr;
    }

    const int total = m_units.size();
    for (int attempt = 0; attempt < total; ++attempt) {
        if (m_battleTurnIndex >= m_units.size()) {
            m_battleTurnIndex = 0;
        }

        Unit* unit = m_units[m_battleTurnIndex];
        ++m_battleTurnIndex;

        if (!unit) {
            continue;
        }
        if (!isUnitOnBoard(unit)) {
            continue;
        }
        
        if (unit->status() == Unit::Status::Dead || unit->hp() <= 0) {
            continue;
        }
        
        return unit;
    }

    return nullptr;
}

void Game::setActiveUnitItem(int unitId)
{
    if (m_activeActionUnitId == unitId) {
        return;
    }

    UnitItem* previous = findUnitItem(m_activeActionUnitId);
    if (previous) {
        previous->setActive(false);
    }

    m_activeActionUnitId = unitId;
    UnitItem* current = findUnitItem(m_activeActionUnitId);
    if (current) {
        current->setActive(true);
    }
}

void Game::flushUnitRemovals()
{
    if (m_unitsPendingRemoval.isEmpty()) {
        return;
    }

    const QList<Unit*> pending = m_unitsPendingRemoval;
    m_unitsPendingRemoval.clear();

    for (Unit* unit : pending) {
        removeUnitNow(unit);
    }
}

void Game::removeUnitNow(Unit* unit)
{
    if (!unit) {
        return;
    }

    if (unit->owner() == Unit::Owner::EnemyCtrl && m_enemyUnitCount > 0) {
        --m_enemyUnitCount;
    }

    if (m_activeActionUnitId == unit->id()) {
        setActiveUnitItem(-1);
    }

    // 应该不会出现战斗中的单位正在被拖拽
    // 暂时保留
    if (m_activeUnitId == unit->id()) {
        m_dragActive = false;
        m_activeUnitId = -1;
        m_sourceGrid = QPoint(-1, -1);
        m_sourceFromBench = false;
        m_sourceBenchSlot = -1;
    }

    for (Unit* other : m_units) {
        if (other && other->target() == unit) {
            other->setTarget(nullptr);
        }
    }

    m_board.removeUnit(unit);
    m_bench.removeUnit(unit);

    UnitItem* item = findUnitItem(unit->id());
    if (item) {
        m_scene->removeItem(item);
        m_unitItemById.erase(unit->id());
        auto it = std::remove(m_unitItems.begin(), m_unitItems.end(), item);
        m_unitItems.erase(it, m_unitItems.end());
        delete item;
    }

    m_units.removeAll(unit);
    delete unit;
}

// 命中测试：返回鼠标所在的备战区槽位索引。
int Game::hitTestBenchSlot(const QPointF& scenePos) const
{
    for (int i = 0; i < static_cast<int>(m_benchRects.size()); ++i) {
        if (m_benchRects[i].contains(scenePos)) {
            return i;
        }
    }
    return -1;
}

void Game::addUnitFromShop(Unit* unit)
{
    if (!unit) return;
    
    m_units.append(unit);
    
    // 创建图元并连接信号
    UnitItem* unitItem = new UnitItem(unit);
    unitItem->setZValue(kZUnit);
    m_scene->addItem(unitItem);
    m_unitItems.push_back(unitItem);
    m_unitItemById[unit->id()] = unitItem;

    connect(unitItem, &UnitItem::dragStarted,
            this, &Game::handleDragStarted);
    connect(unitItem, &UnitItem::dragMoved,
            this, &Game::handleDragMoved);
    connect(unitItem, &UnitItem::dragDropped,
            this, &Game::handleDropCommand);
    connect(unitItem, &UnitItem::eqDragStarted,
            this, &Game::handleUnitEqDragStarted);
    connect(unitItem, &UnitItem::eqDragMoved,
            this, &Game::handleUnitEqDragMoved);
    connect(unitItem, &UnitItem::eqDragDropped,
            this, &Game::handleUnitEqDragDropped);
            
    // 同步到视图
    syncFromBoard();
}
