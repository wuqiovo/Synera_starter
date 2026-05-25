<h1 style="text-align:center">项目文档</h1>

## 1.基本信息

姓名：吴淇

学号：251220033

各阶段完成度：

- 阶段一
  - [x] 棋盘与备战区、双方半场与地块占用规则
  - [x] 备战区与棋盘的数据同步
  - [x] Unit基类
  - [x] Owner 区分归属，Traits 区分羁绊（职业）
  - [x] 玩家实体与敌方轮次生成
  - [x] 拖拽摆放与非法放置处理
  - [x] GUI展示棋盘/备战区/单位信息
- 阶段二
  - [x] 完成准备/战斗/结算三阶段循环与轮次推进
  - [x] 敌方单位按关卡配置自动生成并随轮次增强。
  - [x] 实现单位状态机（Idle/Moving/Attacking/Casting/Dead）。
  - [x] 实现索敌规则（欧氏距离 + 平局优先级）。
  - [x] 实现寻路、阻挡与防重叠碰撞。
  - [x] 实现普攻、回蓝、技能多态（3–5 英雄）与胜负结算。
- 阶段三
  - [x] 实现金币系统、关卡奖励与商店（5 个招募位）。
  - [x] 实现购买、刷新与备战区落位逻辑。
  - [x] 实现人口上限升级与上阵人数限制。
  - [x] 实现 4–6 种职业/种族（至少出现 2 种属性光环 + 1 种机制改变类型的羁绊）。
  - [x] 实现升星（3 合 1）与 2 星属性提升。
  - [x] 实现装备掉落、穿戴限制与至少 4 种基础装备。
  - [x] 实现存档/读档机制。
  - [x] GUI 完整展示经济、商店、羁绊、星级、轮次与阶段信息。
- 阶段四
  - [x] 选择并实现至少 1 个具备实质工作量的扩展功能。
  - [x] 扩展功能与现有系统正确集成（流程可正常闭环）。
  - [x] 完成对应 GUI/交互或系统逻辑展示。
  - [x] 在 README 中补充扩展设计、实现说明与演示要点。

## 2.目录结构

```
Synera_starter/
├── CMakeLists.txt              # CMake 构建脚本（Qt6 + C++17）
├── README.md                   # 本说明文档
├── PA说明文档.pdf              # 课程作业说明
├── assets/                     # 美术资源（角色精灵图等）
│   ├── craftpix-reaper-man-chibi-2d-game-sprites/
│   └── craftpix-satyr-tiny-style-2d-sprites/
├── build/                      # 构建产物（不入档）
└── src/
    ├── main.cpp                # 程序入口，构造 StartWindow 与 GameWindow
    ├── core/                   # 游戏核心逻辑（无 Qt GUI 依赖以外的耦合）
    │   ├── board.{h,cpp}       # 8×8 六边形棋盘与占位/移动判定
    │   ├── bench.{h,cpp}       # 备战区槽位管理
    │   ├── shop.{h,cpp}        # 商店生成、刷新、购买与升星
    │   ├── game.{h,cpp}        # 游戏主循环、轮次/阶段控制、战斗调度、存档
    │   └── gamestate.h         # 存档用的纯数据结构（UnitState/PlayerState/GameState）
    ├── entity/                 # 游戏对象
    │   ├── unit.{h,cpp}        # Unit 基类与 Warrior/Mage/Archer/Boss 派生
    │   ├── player.{h,cpp}      # 玩家（HP、金币、等级、人口、装备栏）
    │   └── equipment.{h,cpp}   # 装备类型与属性加成
    └── gui/                    # 基于 Qt Graphics View 的视图层
        ├── startwindow.{h,cpp} # 开始界面（新开局/读档）
        ├── gamewindow.{h,cpp}  # 主游戏窗口
        ├── griditem.{h,cpp}    # 单个六边形格子图元
        ├── unititem.{h,cpp}    # 单位图元（含拖拽与血条/法力条绘制）
        └── equipmentslot.{h,cpp} # 装备槽图元（拖拽穿戴/丢弃）
```

## 3.核心类与数据结构

### Board 类（[src/core/board.h](src/core/board.h)）

8×8 棋盘，按行优先方式存储 `QVector<Unit*>`，并维护 `QHash<Unit*, QPoint>` 反向索引，保证 O(1) 查到任一单位当前坐标。

- `addUnit`：将单位放入指定坐标的格子，若坐标非法或已被占用则静默失败。
- `removeUnit`：将单位（如果确实在棋盘上）从棋盘上移出，并不 delete。
- `moveUnit`：把单位从当前格移动到目标格，同步更新正/反向索引。
- `getUnitAt` / `hasUnitAt`：分别返回指针或 bool，用于占位查询。
- `isValidPosition`：判断坐标是否在 `[0, COLS) × [0, ROWS)` 之内。
- `isPlayerHalf`：将棋盘上下分半，玩家位于下半区、敌方位于上半区。
- `clear`：清空棋盘（不释放单位指针，所有权在 Game）。
- `indexOf`：将 `QPoint` 转换为内部 `QVector` 下标的小工具。

### Bench 类（[src/core/bench.h](src/core/bench.h)）

备战区，容量默认 8。结构上与 Board 类似——`QVector<Unit*>` 存储槽位 + `QHash<Unit*, int>` 反向索引。

- `addUnit(unit, slot)` / `removeUnit` / `moveWithin`：槽位级别的增删与互换。
- `unitAt` / `hasUnitAt` / `contains` / `slotOf`：查询接口。
- `firstEmptySlot` / `unitCount` / `capacity`：用于商店落位与人口判定。

### Unit 类（[src/entity/unit.h](src/entity/unit.h)）

游戏中所有可作战实体的基类。属性按"基础值 + 加成（羁绊）+ 装备加成"分层存储，`maxHp()/atk()/range()/maxMana()` 会即时合算。

主要字段：

- 标识：`m_id`（全局自增）、`m_name`、`m_position`、`m_level`。
- 属性：`m_hp / m_maxHp`、`m_atk`、`m_range`、`m_maxMana / m_mana`。
- 加成：`m_bonusMaxHp / m_bonusAtk / m_bonusRange`（来自羁绊）。
- 归属与分类：`m_owner ∈ {PlayerCtrl, EnemyCtrl}`、`m_trait ∈ {None, Warrior, Mage, Archer, Boss}`。
- 状态机：`m_status ∈ {Idle, Moving, Attacking, Casting, Dead}`。
- 战斗修正：`m_stunTurns`、`m_damageOutputReduction*`、`m_vunerable*`（眩晕 / 输出衰减 / 易伤）。
- 装备：`m_equipment` 持有一件 `Equipment*`。

主要行为（皆设计为 `virtual`，支持多态）：

- `act(Game*)`：状态机驱动入口，先 `prepareForAct` 再按当前 `Status` 进入相应分支。
- `findTarget(Game*)`：基于"曼哈顿距离最近 + 先到优先"的索敌（详见第 4 节）。
- `moveTowardsTarget`：在六边形网格上做 BFS 寻路并执行一步移动。
- `attackTarget` / `resolveAttack` / `takeDamage`：普攻、攻后清算与受击。
- `skill(Game*)`：技能接口，在 `Warrior/Mage/Archer/Boss` 中被重写为各自效果。
- `upgrade()`：升星（属性 ×1.6，回满血量）。

派生类与技能：

- `Warrior`（近战 range=1）：技能"重击"——单体击晕目标 1 回合。
- `Mage`（远程 range=3, maxMana=80）：技能"AOE 爆发"——以目标为中心 1 格内 AOE；当玩家场上法师羁绊 ≥3 时扩大为 2 格。
- `Archer`（远程 range=2）：技能"削弱"——目标输出降低 20% 持续 2 回合。
- `Boss`（近战 range=1, hp=200, atk=30）：技能"狂暴"——自身永久 +10 攻击，并对目标施加 2 回合易伤（受伤 +30%）。

### Player 类（[src/entity/player.h](src/entity/player.h)）

承载玩家级状态：`hp`（默认 100）、`gold`、`level`、`populationCap`、`curStage`、`player_name`，以及容量为 5 的装备栏 `m_inventory[5]`。

封装的便捷接口：`reduceHp / healHp / addGold / costGold`（金币不足时返回 false 而不是抛异常）；`addInventory` 会找首个空位入栏，满则丢弃并返回 false。

### Equipment 类（[src/entity/equipment.h](src/entity/equipment.h)）

四种基础装备及其加成：

| 类型      | 效果                          |
| --------- | ----------------------------- |
| `Sword`   | +3 攻击力                     |
| `armor`   | +30 生命值                    |
| `Crystal` | -20 最大法力（更快释放技能）  |
| `Gloves`  | 一回合行动两次（在 Game 处理）|

装备装/卸通过 `Unit::setEquipment` 完成，会自动维护当前 HP/Mana 不超过新的上限。

### Shop 类（[src/core/shop.h](src/core/shop.h)）

商店持有 5 个 `UnitInfo*` 商品位。

- `generate / createUnit`：按职业等概率刷新商品。
- `refresh(Game*)`：花费 2 金币重抛 5 个商品。
- `buyUnit(index, Game*)`：花费 3 金币购买；自动落入备战区末位，若已满则退款。**购买后立即检测同职业同 1 星 ≥3 的情况，触发 3 合 1 升星**，并保留其中一件装备。
- `upgradeCapacity(Game*)`：5 金币购买"人口 +2"。
- `sellUnit(Unit*, Game*)`：卖出获得 1 金币并调用 `requestRemoveUnit` 安全清理。

### GameState / UnitState / PlayerState（[src/core/gamestate.h](src/core/gamestate.h)）

纯数据 POD 结构，专门用于存档/读档的中间表示，序列化为 JSON 时一一对应（详见第 4.5 节）。

### Game 类（[src/core/game.h](src/core/game.h)）

整盘游戏的中枢，继承自 `QObject`。负责：

- 拥有并管理 `Player m_player`、`Board m_board`、`Bench m_bench`、`Shop m_shop`、`QList<Unit*> m_units`。
- 阶段/轮次推进：`startNextStage / startNextRound / endDeployment / reset`。
- 战斗循环：通过 `QTimer m_battleTimer`（默认 300ms 一 tick）驱动 `battleTick → nextActingUnit → unit->act`。
- 拖拽交互：处理棋盘 ↔ 备战区、装备栏 ↔ 单位的所有拖拽事件，并做合法性检查。
- 羁绊缓存：`m_playerTraitCounts / m_enemyTraitCounts`，每次场上变化后 `refreshTraitCounts` 全量重算并下发到 Unit 的 `bonusXxx`。
- 信号广播：`playerInfoChanged / enemyInfoChanged / stageRoundChanged / battleStateChanged / gameOver / enemyDefeated`，供 GUI 层订阅。

## 4.关键算法

### 4.1 单位状态机

每个 `Unit` 在 `act(Game*)` 中执行一次"准备—状态分派—收尾"流程：

```
prepareForAct()
   ├─ 倒计减少 stun / 输出衰减 / 易伤回合
   ├─ 非攻击/施法状态下重新 findTarget()
   ├─ 若 stunTurns>0 → return false（本回合跳过）
   ├─ 若 mana 满 → 切到 Casting
   └─ 检查目标存活/距离，必要时回 Idle 或切 Moving

switch (m_status):
    Idle      → normalIdleBehavior：回 10 蓝，按距离/法力决定下一态
    Moving    → normalMoveBehavior：到位则切 Attacking/Casting，否则 moveTowardsTarget
    Attacking → attackTarget + resolveAttack（攻后 +10 mana）
    Casting   → skill(game) + resolveAttack，并清空 mana
    Dead      → Game::requestRemoveUnit 排队清理
```

注意 `Idle` 分支故意 **不写 `break`**，会落穿到 `Moving`，保证空闲态在本 tick 内就能直接迈出第一步。

### 4.2 索敌

`Unit::findTarget` 在 `Game::units()` 中遍历所有"敌对、非死亡、在棋盘上"的单位，按 **曼哈顿距离最小** 取目标，距离相同则取遍历顺序靠前者（隐式地以 `m_units` 的入场顺序作为平局打破）。状态机调用方在每个 tick 重新索敌，因此目标可以随场上变化动态切换。

### 4.3 六边形寻路

棋盘使用偶数行右移（offset coord）布局，`unit.cpp` 中以匿名命名空间提供：

- `toAxialCoord / toOffsetCoord`：与轴向坐标互转。
- `hexDistance`：标准的轴向 cube 距离。
- `hexNeighbors`：返回六个方向邻居。

`moveTowardsTarget` 用 **BFS** 从起点扩散，把其它单位的格子视为障碍：

1. 维护 `cameFrom / stepsFromStart`，记录最短路径与已用步数。
2. 一旦展开到的格子与目标的 `hexDistance ≤ range()` 即视作"可攻击位"，提前结束。
3. 若全图扩散完仍未找到攻击位，则选 **离目标最近且步数最短** 的可达格作为本回合方向（保证不会原地踏步）。
4. 从目的地沿 `cameFrom` 回溯到"起点的相邻格"，作为这一 tick 的实际移动一步，最后由 `Game::moveUnitDuringBattle` 落子。

### 4.4 战斗调度与"手套双动"

`Game::battleTick`：

1. `nextActingUnit` 用 `m_battleTurnIndex` 在 `m_units` 中轮转，跳过死亡/离场单位。
2. 选中的单位调用 `act(this)`；若它佩戴了 `Gloves` 装备，则 150ms 后再次触发 `battleTick`，实现"一回合两动"。
3. 行动后扫一遍单位列表，对 `hp<=0` 者置为 `Dead` 并 `requestRemoveUnit` 入队，统一在 `flushUnitRemovals` 中真正移除/释放。
4. 敌方阵亡掉落：每个被击杀的敌方单位掉落一件随机基础装备（`Sword/Crystal/armor/Gloves`），写入玩家装备栏。
5. 若任一方全灭，停止计时器并进入 `resolveRoundFromCurrentBoard`。

### 4.5 经济与回合结算

`applyRoundDamage(winner, remainingUnits)` 控制三件事：

- **伤害**：失败方扣血 `10 × 残余单位数`；若失败方是玩家，玩家 HP 归零触发 `gameOver`。
- **连胜/连败奖励**：胜方基础 +8 金币 +（连胜 -1），败方基础 +5 金币 +2×（连败 -1）。
- **利息**：结算后按 `gold/5` 取整再额外 +1（封顶在金额自身，无封顶上限）。

`refreshTraitCounts` 维护羁绊缓存并把光环写回单位：

- **战士**：场上同阵营战士 ≥2 给 `+30 HP`，≥4 再叠 `+5 ATK`。
- **弓手**：≥3 给 `+1 range`。
- **法师**：羁绊以"机制改变"的形式生效——技能 AOE 半径从 1 扩到 2 格（在 `Mage::skill` 内部读取 `traitCount` 决定）。
- 二星单位在计数中算 2 个，体现"高星位等同于多张同名牌"。

### 4.6 三合一升星

发生在 `Shop::buyUnit` 落位之后：扫描场上同 trait、1 星、同阵营单位，若 ≥3 则销毁这三个、生成对应职业的新 Unit、调用 `Unit::upgrade()`（属性 ×1.6 并回满血）、保留其中至多 1 件装备转移到新单位。

### 4.7 存档/读档

存盘的入口是 `Game::saveToFile`，先 `captureState()` 把 `m_player / m_units / m_bench / 阶段轮次 / Unit::s_nextId` 全部塞进 `GameState`，再用 `QJsonObject` 序列化为 UTF-8 JSON 文本写盘；读盘 `loadFromFile` 反过来构造 `GameState` 后交给 `loadFromState` 重新拉起棋盘、备战区、装备栏与 UI。
**战斗中禁止存档**（`saveToFile` 在 `m_inBattle` 时直接返回 false），避免出现中间态。

## 5.编译与运行

### 5.1 环境要求

- 操作系统：Windows / Linux / macOS 均可，开发环境为 Windows 11。
- C++ 编译器：支持 C++17（MinGW-w64 13、MSVC 2022 或 Clang 16+ 均可）。
- Qt：6.x（开发环境为 **Qt 6.11.0 MinGW 64-bit**，对应 `CMakeLists.txt` 中的 `CMAKE_PREFIX_PATH`）。
- CMake：≥ 3.16。

### 5.2 构建（命令行）

```powershell
# 1. 如果你的 Qt 安装路径与默认不同，请编辑 CMakeLists.txt 第 14 行 CMAKE_PREFIX_PATH
#    例如：set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/mingw_64")

# 2. 配置并生成
cmake -S . -B build -G "MinGW Makefiles"

# 3. 编译
cmake --build build -j

# 4. 运行（Windows 下产物在 build/Synera_Starter.exe）
.\build\Synera_Starter.exe
```

也可以直接在 Qt Creator 中打开根目录的 `CMakeLists.txt` 作为项目，选择对应 Kit 后构建运行。

### 5.3 使用方式

1. 启动后进入 **开始界面**，可选"新游戏"或"读档"（读档需选择此前保存的 `.json` 文件）。
2. 进入对局后处于 **准备阶段**：
   - 鼠标拖拽备战区单位到下半棋盘上场；
   - 点击商店底部"刷新（2 金）"/"购买（3 金）"/"升人口（5 金）"按钮；
   - 拖拽装备栏中的装备到棋盘单位身上即可穿戴；
   - 拖拽场上单位回备战区可下场。
3. 点击"开始战斗"按钮进入 **战斗阶段**，由 `battleTick` 自动驱动，每 300ms 一帧。
4. 战斗结束后自动结算金币与伤害并进入下一轮；玩家 HP 归零或击败全部 5 波敌人后结束。
5. 准备阶段可通过菜单中的"存档"保存当前进度；战斗阶段禁止存档。