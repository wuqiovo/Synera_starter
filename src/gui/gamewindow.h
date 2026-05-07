#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QMainWindow>

class Game;
class QGraphicsView;
class QLabel;
class QPushButton;
class QVBoxLayout;

class GameWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget* parent = nullptr);
    ~GameWindow();
    bool loadFromFile(const QString& filePath);

private slots:
    void onResetButtonClicked();
    void onEndDeploymentClicked();
    void onEndBattleDebugClicked();
    void onResetAllDebugClicked();
    void onSaveAndExitClicked();
    void refreshInfoPanels();
    void onBattleStateChanged(bool inBattle);
    void onGameOver();
    void onEnemyDefeated();

private:
    void setupUI();

    // 主窗口的中心容器
    QWidget* m_centralWidget;
    // 主布局：包含信息栏、游戏视图和控制栏
    QVBoxLayout* m_mainLayout;
    // 游戏视图：展示棋盘、单位等图元
    QGraphicsView* m_view;
    QPushButton* m_resetButton;
    // 结束部署按钮。
    QPushButton* m_endDeploymentButton;
    // 调试：结束战斗按钮。
    QPushButton* m_endBattleDebugButton;
    // 调试：重置全部状态按钮。
    QPushButton* m_resetAllDebugButton;
    // 保存并退出按钮。
    QPushButton* m_saveExitButton;
    // 玩家信息面板。
    QLabel* m_playerInfoLabel;
    // 敌方信息面板。
    QLabel* m_enemyInfoLabel;
    // 战斗/部署阶段提示。
    QLabel* m_phaseLabel;
    Game* m_game;
};

#endif // GAMEWINDOW_H
