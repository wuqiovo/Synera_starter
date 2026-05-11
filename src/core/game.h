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

class Unit;
struct GameState;
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
    explicit Game(QObject* parent = nullptr);
    ~Game();

    void reset();
    void startNextStage();
    void startNextRound();
    void endDeployment();
    void debugEndBattle();
    // 用于调试：重置全部状态并回到初始关卡。
    void debugResetAll();
    
    bool saveToFile(const QString& filePath) const;
    bool loadFromFile(const QString& filePath);

    Player* player() { return &m_player; }
    int enemyDefeatedWaves() const { return m_enemyDefeatedWaves; }
    int enemyMaxWaves() const { return m_enemyMaxWaves; }
    int stage() const { return m_stage; }
    int round() const { return m_round; }
    bool isInBattle() const { return m_inBattle; }
    int traitCount(Unit::Owner owner, Unit::Trait trait) const;
    
    QGraphicsScene* scene() const { return m_scene; }

    void handleDragStarted(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleDragMoved(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleDropCommand(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);

    void requestRemoveUnit(Unit* unit);
    bool moveUnitDuringBattle(Unit* unit, const QPoint& target);
    bool isUnitOnBoard(const Unit* unit) const;

    void addUnitFromShop(Unit* unit);

    const QList<Unit*>& units() const { return m_units; }

    Bench* bench() { return &m_bench; }
    Board* board() { return &m_board; }
    Shop* shop() { return &m_shop; }
    
signals:
    void playerInfoChanged();
    void enemyInfoChanged();
    void stageRoundChanged();
    void battleStateChanged(bool inBattle);
    void gameOver();
    void enemyDefeated();

private:
    void clearAllUnits();
    void loadFromState(const GameState& state);
    GameState captureState() const;
    void createStarterUnitsIfNeeded();
    void spawnEnemiesForCurrentRound();
    void clearEnemyUnits();
    void movePlayerUnitsBack();
    void createPlayerUnitForCurrentRound();

    Unit* findUnitById(int unitId) const;
    GridItem* findGridItem(const QPoint& gridPos) const;
    UnitItem* findUnitItem(int unitId) const;

    void clearGridHighlights();
    void clearBenchHighlights();
    bool canApplyDrop(int unitId, const QPoint& source, const QPoint& target) const;
    bool canDropOnBoard(Unit* unit, const QPoint& target) const;
    bool canDropOnBench(Unit* unit, int slot, bool fromBench) const;
    bool applyDrop(int unitId, const QPoint& target);
    bool transferUnitFromBoardToBench(int unitId, int benchSlot);
    bool transferUnitFromBenchToBoard(int unitId, const QPoint& target);
    bool moveUnitWithinBench(int fromSlot, int toSlot);
    
    void buildScene();
    void buildShopUI();
    void updateShopUI();
    void syncFromBoard();
    void refreshTraitCounts();

    int countAliveUnits(Unit::Owner owner) const;
    void applyRoundDamage(Unit::Owner winner, int remainingUnits);
    void resolveRoundFromCurrentBoard();
    int hitTestBenchSlot(const QPointF& scenePos) const;
    void battleTick();
    void startBattleLoop();
    void stopBattleLoop();
    bool hasAliveUnits(Unit::Owner owner) const;
    void flushUnitRemovals();
    void removeUnitNow(Unit* unit);
    Unit* nextActingUnit();
    void setActiveUnitItem(int unitId);

    QPointF gridToWorld(int row, int col) const;
    QPoint worldToGrid(const QPointF& world) const;
    QPolygonF cellHexPolygon(int row, int col) const;
    // 将羁绊枚举映射为数组索引。
    static int traitIndex(Unit::Trait trait);
    // 读取单位的羁绊类型。
    static Unit::Trait traitForUnit(const Unit* unit);

    // 成员变量

    // 玩家对象
    Player m_player;

    // 棋盘逻辑数据（地块占用、半场判定、单位落位真值来源）。
    Board m_board;

    // 备战区槽位数据（未上阵单位的容器，默认 8 格）。
    Bench m_bench;
    
    // 商店逻辑数据
    Shop m_shop;

    // 游戏中托管的全部单位对象（用于生命周期管理与按 ID 查找）。
    QList<Unit*> m_units;

    // 主渲染场景，承载网格与单位图元。
    QGraphicsScene* m_scene;

    // 棋盘每个格子的图元集合（用于高亮、命中与刷新）。
    std::vector<GridItem*> m_gridItems;

    // 单位图元集合（用于位置同步与显示控制）。
    std::vector<UnitItem*> m_unitItems;

    // 备战区槽位图元与几何信息。
    std::vector<QGraphicsRectItem*> m_benchItems;
    std::vector<QRectF> m_benchRects;
    // 备战区标签文字。
    QGraphicsSimpleTextItem* m_benchLabel;
    
    // 商店UI的代理
    QGraphicsProxyWidget* m_shopProxy = nullptr;
    // 商店内5个商品格子的布局指针，用于updateShopUI快速刷新
    std::vector<QVBoxLayout*> m_shopProductLayouts;
    // 商店刷新按钮指针，用于更新启用状态
    QPushButton* m_shopRefreshBtn = nullptr;
    // 商店出售区域判定矩形(场景坐标)
    QRectF m_shopSellRect;

    // 当前是否处于拖拽流程中。
    bool m_dragActive;

    // 当前正在被拖拽的单位 ID（无激活时为 -1）。
    int m_activeUnitId;

    // 当前拖拽操作的起始棋盘坐标。
    QPoint m_sourceGrid;

    // 当前拖拽是否来自备战区，以及对应槽位。
    bool m_sourceFromBench;
    // 拖拽起点的备战区槽位索引（非备战区时为 -1）。
    int m_sourceBenchSlot;

    // 单位 ID 到单位图元的快速索引表。
    std::unordered_map<int, UnitItem*> m_unitItemById;

    // 当前关卡（stage），与 Player::curStage 保持一致。
    int m_stage;

    // 当前轮次
    int m_round;

    // 敌方已战胜波次与波次上限。
    int m_enemyDefeatedWaves;
    // 敌方最大波次。
    int m_enemyMaxWaves;

    // 敌方单位数量上限。
    int m_enemyUnitCap;
    // 当前敌方单位数量。
    int m_enemyUnitCount;

    // 当前是否处于回合中。
    bool m_inBattle;

    // 战斗节拍定时器。
    QTimer* m_battleTimer;
    // 战斗节拍间隔（毫秒）。
    int m_battleTickMs;
    // 当前行动的单位 ID（高亮用，-1 表示无）。
    int m_activeActionUnitId;
    // 战斗轮转索引。
    int m_battleTurnIndex;

    // 待删除单位列表（在战斗 tick 后集中处理）。
    QList<Unit*> m_unitsPendingRemoval;

    // 棋盘行数缓存（用于坐标换算与遍历）。
    int m_rows;

    // 棋盘列数缓存（用于坐标换算与遍历）。
    int m_cols;

    // 六边形格子的外接半径（控制单元格大小）。
    qreal m_radius;
    
    // 相邻行中心点的纵向间距（控制六边形网格排布）。
    qreal m_rowSpacing;

    // 备战区槽位默认颜色。
    QColor m_benchSlotColor;
    // 备战区槽位高亮颜色。
    QColor m_benchSlotHighlightColor;

    // 备战区顶部 y 坐标（用于交互区域判断）。
    qreal m_benchTop;
    // 棋盘与备战区分隔线 y 坐标。
    qreal m_separatorY;

    // 羁绊枚举数量（None + Warrior + Mage + Archer）。
    static constexpr int kTraitCount = 4;
    // 玩家单位羁绊计数缓存。
    std::array<int, kTraitCount> m_playerTraitCounts{};
    // 敌方单位羁绊计数缓存。
    std::array<int, kTraitCount> m_enemyTraitCounts{};
};

#endif // CORE_GAME_H
