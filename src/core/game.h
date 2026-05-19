#ifndef CORE_GAME_H
#define CORE_GAME_H

#include <QObject>
#include <QList>
#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <array>
#include <unordered_map>
#include <vector>
#include "board.h"
#include "bench.h"
#include "shop.h"
#include "entity/player.h"
#include "gamestate.h"

class Unit;
class QGraphicsScene;
class QGraphicsRectItem;
class QGraphicsSimpleTextItem;
class GridItem;
class UnitItem;
class QTimer;
class QGraphicsProxyWidget;
class QPushButton;
class QVBoxLayout;

class Game : public QObject
{
    Q_OBJECT

public:
    // ── 构造/析构 ──
    explicit Game(QObject* parent = nullptr);
    ~Game();

    // ── 状态控制 ──
    void reset();
    void startNextStage();
    void startNextRound();
    void endDeployment();

    // ── 调试 ──
    void debugEndBattle();
    void debugResetAll();

    // ── 存档 ──
    bool saveToFile(const QString& filePath) const;
    bool loadFromFile(const QString& filePath);

    // ── 访问器 ──
    Player* player() { return &m_player; }
    int enemyDefeatedWaves() const { return m_enemyDefeatedWaves; }
    int enemyMaxWaves() const { return m_enemyMaxWaves; }
    int stage() const { return m_stage; }
    int round() const { return m_round; }
    bool isInBattle() const { return m_inBattle; }
    int traitCount(Unit::Owner owner, Unit::Trait trait) const;

    QGraphicsScene* scene() const { return m_scene; }
    const QList<Unit*>& units() const { return m_units; }
    Bench* bench() { return &m_bench; }
    Board* board() { return &m_board; }
    Shop* shop() { return &m_shop; }

    // ── 拖拽交互 ──
    void handleDragStarted(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleDragMoved(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleDropCommand(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);

    // ── 装备栏 UI ──
    void buildEquipmentUI();
    void updateEquipmentUI();

    // ── 单位管理 ──
    void requestRemoveUnit(Unit* unit);
    void removeUnitNow(Unit* unit);
    bool moveUnitDuringBattle(Unit* unit, const QPoint& target);
    bool isUnitOnBoard(const Unit* unit) const;
    void addUnitFromShop(Unit* unit);

public slots:
    // ── 装备交互 ──
    void handleEqDiscardClicked(int slotIndex);
    void handleEqDragStarted(int slotIndex, QPointF scenePos);
    void handleEqDragMoved(int slotIndex, QPointF scenePos);
    void handleEqDragDropped(int slotIndex, QPointF scenePos);

    // ── 单位装备拖拽 ──
    void handleUnitEqDragStarted(int unitId, QPointF scenePos);
    void handleUnitEqDragMoved(int unitId, QPointF scenePos);
    void handleUnitEqDragDropped(int unitId, QPointF scenePos);

signals:
    void playerInfoChanged();
    void enemyInfoChanged();
    void stageRoundChanged();
    void battleStateChanged(bool inBattle);
    void gameOver();
    void enemyDefeated();

private:
    // ── 状态管理 ──
    void clearAllUnits();
    void loadFromState(const GameState& state);
    GameState captureState() const;
    void createStarterUnitsIfNeeded();
    void spawnEnemiesForCurrentRound();
    void clearEnemyUnits();
    void restorePlayerUnits();

    // ── 查找 ──
    Unit* findUnitById(int unitId) const;
    GridItem* findGridItem(const QPoint& gridPos) const;
    UnitItem* findUnitItem(int unitId) const;

    // ── Drop 逻辑 ──
    void clearGridHighlights();
    void clearBenchHighlights();
    bool canApplyDrop(int unitId, const QPoint& source, const QPoint& target) const;
    bool canDropOnBoard(Unit* unit, const QPoint& target) const;
    bool canDropOnBench(Unit* unit, int slot, bool fromBench) const;
    bool applyDrop(int unitId, const QPoint& target);
    bool transferUnitFromBoardToBench(int unitId, int benchSlot);
    bool transferUnitFromBenchToBoard(int unitId, const QPoint& target);
    bool moveUnitWithinBench(int fromSlot, int toSlot);

    // ── UI 构建 ──
    void buildScene();
    void buildShopUI();
    void updateShopUI();
    void syncFromBoard();
    void refreshTraitCounts();

    // ── 战斗 ──
    int countAliveUnits(Unit::Owner owner) const;
    void applyRoundDamage(Unit::Owner winner, int remainingUnits);
    void resolveRoundFromCurrentBoard();
    int hitTestBenchSlot(const QPointF& scenePos) const;
    void battleTick();
    void startBattleLoop();
    void stopBattleLoop();
    bool hasAliveUnits(Unit::Owner owner) const;
    void flushUnitRemovals();
    Unit* nextActingUnit();
    void setActiveUnitItem(int unitId);

    // ── 坐标工具 ──
    QPointF gridToWorld(int row, int col) const;
    QPoint worldToGrid(const QPointF& world) const;
    QPolygonF cellHexPolygon(int row, int col) const;
    static int traitIndex(Unit::Trait trait);
    static Unit::Trait traitForUnit(const Unit* unit);

    // ── 核心数据 ──
    Player m_player;
    Board m_board;
    Bench m_bench;
    Shop m_shop;
    QList<Unit*> m_units;

    // ── 场景/图元 ──
    QGraphicsScene* m_scene;
    std::vector<GridItem*> m_gridItems;
    std::vector<UnitItem*> m_unitItems;

    // ── 单位查找索引 ──
    std::unordered_map<int, UnitItem*> m_unitItemById;

    // ── 备战区 UI ──
    std::vector<QGraphicsRectItem*> m_benchItems;
    std::vector<QRectF> m_benchRects;
    QGraphicsSimpleTextItem* m_benchLabel;

    // ── 商店 UI ──
    QGraphicsProxyWidget* m_shopProxy = nullptr;
    std::vector<QVBoxLayout*> m_shopProductLayouts;
    QPushButton* m_shopRefreshBtn = nullptr;
    QPushButton* m_shopUpgradeBtn = nullptr;
    QRectF m_shopSellRect;
    QWidget* m_shopSellWidget = nullptr;

    // ── 装备栏 UI ──
    QGraphicsSimpleTextItem* m_eqTitleLabel = nullptr;
    std::vector<class EquipmentSlotItem*> m_eqSlotItems;

    // ── 装备拖拽 ──
    bool m_dragEqActive;
    int m_activeEqSlotIndex;
    QGraphicsSimpleTextItem* m_dragEqIcon = nullptr;

    // ── 单位拖拽 ──
    bool m_dragActive;
    int m_activeUnitId;
    QPoint m_sourceGrid;
    bool m_sourceFromBench;
    int m_sourceBenchSlot;

    // ── 关卡/轮次 ──
    int m_stage;
    int m_round;
    int m_enemyDefeatedWaves;
    int m_enemyMaxWaves;

    // ── 战斗前玩家存档 ──
    QVector<UnitState> m_preBattlePlayerUnits;

    // ── 敌方 ──
    int m_enemyUnitCap;
    int m_enemyUnitCount;

    // ── 战斗状态 ──
    bool m_inBattle;
    QTimer* m_battleTimer;
    int m_battleTickMs;
    int m_activeActionUnitId;
    int m_battleTurnIndex;

    // ── 手套双击 ──
    bool m_glovesExtraActPending = false;
    int m_glovesExtraActUnitId = -1;

    // ── 待删除单位 ──
    QList<Unit*> m_unitsPendingRemoval;

    // ── 棋盘尺寸 ──
    int m_rows;
    int m_cols;
    qreal m_radius;
    qreal m_rowSpacing;

    // ── 备战区外观 ──
    QColor m_benchSlotColor;
    QColor m_benchSlotHighlightColor;
    qreal m_benchTop;
    qreal m_separatorY;

    // ── 羁绊 ──
    static constexpr int kTraitCount = 4;
    std::array<int, kTraitCount> m_playerTraitCounts{};
    std::array<int, kTraitCount> m_enemyTraitCounts{};
};

#endif // CORE_GAME_H
