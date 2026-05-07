#include "game.h"
#include "entity/unit.h"
#include "gui/griditem.h"
#include "gui/unititem.h"
#include "core/gamestate.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsScene>
#include <QTimer>
#include <QPen>
#include <QtMath>
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
    , m_enemyHp(100)
    , m_enemyMaxHp(100)
    , m_enemyUnitCap(10)
    , m_enemyUnitCount(0)
    , m_inBattle(false)
    , m_battleTimer(new QTimer(this))
    , m_battleTickMs(240)
    , m_activeActionUnitId(-1)
    , m_battleTurnIndex(0)
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
    m_enemyHp = m_enemyMaxHp;
    m_inBattle = false;
    m_player.setHp(100);
    m_player.setGold(5);
    m_player.setLevel(1);
    m_player.setPopulationCap(10);
    m_player.setCurStage(m_stage);

    // 新 stage 初始化：清空所有历史单位后重新生成初始玩家单位。
    clearAllUnits();
    createStarterUnitsIfNeeded();
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
    m_inBattle = true;
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

    m_inBattle = false;
    emit battleStateChanged(m_inBattle);

    if (m_enemyHp > 0 && m_player.getHp() > 0) {
        startNextRound();
    } else if (m_enemyHp <= 0) {
        startNextStage();
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
    setActiveUnitItem(-1);
}

GameState Game::captureState() const
{
    GameState state;
    state.player.hp = m_player.getHp();
    state.player.gold = m_player.getGold();
    state.player.level = m_player.getLevel();
    state.player.populationCap = m_player.getPopulationCap();
    state.player.stage = m_stage;
    state.player.round = m_round;
    state.player.name = m_player.getPlayerName();

    state.enemyHp = m_enemyHp;
    state.enemyMaxHp = m_enemyMaxHp;
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
        u.hp = unit->hp();
        u.maxHp = unit->maxHp();
        u.atk = unit->atk();
        u.range = unit->range();
        u.maxMana = unit->maxMana();
        u.mana = unit->mana();
        u.owner = unit->owner();
        u.status = unit->status();
        u.trait = unit->trait();
        state.units.push_back(u);
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

    return state;
}

void Game::loadFromState(const GameState& state)
{
    m_board.clear();
    m_bench.clear();
    clearAllUnits();

    m_stage = state.player.stage;
    m_round = state.player.round;
    m_enemyHp = state.enemyHp;
    m_enemyMaxHp = state.enemyMaxHp;
    m_inBattle = state.inBattle;

    m_player.setHp(state.player.hp);
    m_player.setGold(state.player.gold);
    m_player.setLevel(state.player.level);
    m_player.setPopulationCap(state.player.populationCap);
    m_player.setCurStage(state.player.stage);
    m_player.setPlayerName(state.player.name);

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
        unit->setPosition(u.position);
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

// 根据当前单位列表刷新羁绊计数缓存。
void Game::refreshTraitCounts()
{
    // 全量重算当前所有单位的羁绊计数。
    m_playerTraitCounts.fill(0);
    m_enemyTraitCounts.fill(0);

    for (Unit* unit : m_units) {
        if (!unit) {
            continue;
        }

        const int index = traitIndex(traitForUnit(unit));
        if (index < 0 || index >= kTraitCount) {
            continue;
        }

        if (unit->owner() == Unit::Owner::EnemyCtrl) {
            ++m_enemyTraitCounts[static_cast<size_t>(index)];
        } else {
            ++m_playerTraitCounts[static_cast<size_t>(index)];
        }
    }
}

bool Game::saveToFile(const QString& filePath) const
{
    GameState state = captureState();

    QJsonObject root;
    QJsonObject player;
    player["hp"] = state.player.hp;
    player["gold"] = state.player.gold;
    player["level"] = state.player.level;
    player["populationCap"] = state.player.populationCap;
    player["stage"] = state.player.stage;
    player["round"] = state.player.round;
    player["name"] = state.player.name;
    root["player"] = player;

    root["enemyHp"] = state.enemyHp;
    root["enemyMaxHp"] = state.enemyMaxHp;
    root["inBattle"] = state.inBattle;

    QJsonArray units;
    for (const UnitState& u : state.units) {
        QJsonObject obj;
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
    state.player.name = player.value("name").toString();

    state.enemyHp = root.value("enemyHp").toInt(100);
    state.enemyMaxHp = root.value("enemyMaxHp").toInt(100);
    state.inBattle = root.value("inBattle").toBool(false);

    const QJsonArray units = root.value("units").toArray();
    for (const QJsonValue& value : units) {
        const QJsonObject obj = value.toObject();
        UnitState u;
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
        return;
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

    Unit* unit = findUnitById(m_activeUnitId);

    if (unit && benchSlot >= 0 && canDropOnBench(unit, benchSlot, m_sourceFromBench)) {
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

    Unit* player1 = new Warrior("战士", 150, 10);
    Unit* player2 = new Archer("弓手", 100, 10);
    Unit* player3 = new Mage("法师", 80, 15);

    m_units.append(player1);
    m_units.append(player2);
    m_units.append(player3);
}


// 最简敌方轮次生成：每关固定 3 个敌方单位，并优先放置到上半区固定坐标。
void Game::spawnEnemiesForCurrentRound()
{
    const int availableSlots = m_enemyUnitCap - m_enemyUnitCount;
    if (availableSlots <= 0) {
        return;
    }

    const QPoint enemyInitialPositions[] = {
        QPoint(2, 0),
        QPoint(3, 0),
        QPoint(4, 0)
    };

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
    };

    int spawned = 0;
    for (int i = 0; i < 3 && spawned < availableSlots; ++i) {
        Unit* newEnemy = nullptr;
        switch (i) {
        case 0:
            newEnemy = new Warrior(QString::fromUtf8("敌方战士"), 150, 10);
            break;
        case 1:
            newEnemy = new Mage(QString::fromUtf8("敌方法师"), 80, 15);
            break;
        case 2:
            newEnemy = new Archer(QString::fromUtf8("敌方弓手"), 100, 10);
            break;
        default:
            break;
        }

        if (!newEnemy) {
            continue;
        }
        newEnemy->setOwner(Unit::Owner::EnemyCtrl);

        const QPoint preferredPos = enemyInitialPositions[i];
        if (m_board.addUnit(newEnemy, preferredPos)) {
            m_units.append(newEnemy);
            attachUnitItemFor(newEnemy);
            ++m_enemyUnitCount;
            ++spawned;
            continue;
        }

        bool placed = false;

        // 固定点被占时，先回退到敌方半场任意空格。
        for (int row = 0; row < Board::ROWS / 2 && !placed; ++row) {
            for (int col = 0; col < Board::COLS; ++col) {
                const QPoint fallback(col, row);
                if (m_board.addUnit(newEnemy, fallback)) {
                    m_units.append(newEnemy);
                    attachUnitItemFor(newEnemy);
                    ++m_enemyUnitCount;
                    ++spawned;
                    placed = true;
                    break;
                }
            }
        }
        if (!placed) {
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
    if (m_board.hasUnitAt(target)) {
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

    if (fromBench) {
        return true;
    }

    return !m_bench.hasUnitAt(slot);
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

    const QPoint boardPos = unit->position();

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
    }

    m_scene->setSceneRect(totalBounds.adjusted(-40, -60, 40, 60));
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

// 按规则对失败方总血量造成伤害，并发放金币奖励。
void Game::applyRoundDamage(Unit::Owner winner, int remainingUnits)
{
    const int damage = 10 * remainingUnits;
    
    if (winner == Unit::Owner::PlayerCtrl) {
        m_player.setGold(m_player.getGold() + 6);
    } else {
        m_player.setGold(m_player.getGold() + 3);
    }

    if (damage <= 0) {
        return;
    }

    if (winner == Unit::Owner::PlayerCtrl) {
        m_enemyHp = qMax(0, m_enemyHp - damage);
        emit enemyInfoChanged();
        if (m_enemyHp <= 0) {
            emit enemyDefeated(); 
        }
        return;
    }

    m_player.reduceHp(damage);
    emit playerInfoChanged();
    if (m_player.getHp() <= 0) {
        emit gameOver(); // Need to define this signal or handle it.
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
        return;
    }

    Unit* unit = nextActingUnit();
    if (unit) {
        setActiveUnitItem(unit->id());
        unit->act(this);
    } else {
        setActiveUnitItem(-1);
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

    if (!hasAliveUnits(Unit::Owner::PlayerCtrl) || !hasAliveUnits(Unit::Owner::EnemyCtrl)) {
        stopBattleLoop();
        setActiveUnitItem(-1);
        resolveRoundFromCurrentBoard();
        m_inBattle = false;
        emit battleStateChanged(m_inBattle);
        
        if (m_enemyHp > 0 && m_player.getHp() > 0) {
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
        /* 已经死亡的单位需要调用act方法彻底清除
        if (unit->status() == Unit::Status::Dead || unit->hp() <= 0) {
            continue;
        }
        */
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
