#include "gamewindow.h"
#include "core/game.h"
#include "entity/unit.h"
#include <QApplication>
#include <QFileDialog>
#include <QGraphicsView>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

GameWindow::GameWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_centralWidget(new QWidget(this))
    , m_mainLayout(new QVBoxLayout())
    , m_view(new QGraphicsView(this))
    , m_resetButton(new QPushButton("Reset", this))
    , m_endDeploymentButton(new QPushButton(QString::fromUtf8("结束部署"), this))
    , m_endBattleDebugButton(new QPushButton(QString::fromUtf8("调试结束战斗"), this))
    , m_resetAllDebugButton(new QPushButton(QString::fromUtf8("调试ResetAll"), this))
    , m_saveExitButton(new QPushButton(QString::fromUtf8("保存并退出"), this))
    , m_playerInfoLabel(new QLabel(this))
    , m_enemyInfoLabel(new QLabel(this))
    , m_phaseLabel(new QLabel(this))
    , m_game(new Game(this))
{
    setupUI();
    m_game->startNextStage();
    refreshInfoPanels();
}

GameWindow::~GameWindow() = default;

bool GameWindow::loadFromFile(const QString& filePath)
{
    if (!m_game) {
        return false;
    }

    const bool ok = m_game->loadFromFile(filePath);
    if (!ok) {
        QMessageBox::warning(this, tr("Load Failed"), tr("Failed to load save file."));
    }
    return ok;
}

void GameWindow::onResetButtonClicked()
{
    if (m_game) {
        m_game->reset();
    }
}

// 结束部署按钮回调。
void GameWindow::onEndDeploymentClicked()
{
    if (m_game) {
        m_game->endDeployment();
    }
}

// 调试：手动结束战斗按钮回调。
void GameWindow::onEndBattleDebugClicked()
{
    if (m_game) {
        m_game->debugEndBattle();
    }
}

// 用于调试：重置全部状态按钮回调。
void GameWindow::onResetAllDebugClicked()
{
    if (m_game) {
        m_game->debugResetAll();
    }
}

void GameWindow::onSaveAndExitClicked()
{
    if (!m_game) {
        return;
    }

    if (m_game->isInBattle()) {
        QMessageBox::warning(this, QString::fromUtf8("无法保存"), QString::fromUtf8("战斗阶段禁止存档，请在部署阶段操作。"));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save Game"),
        QString(),
        tr("Save Files (*.json);;All Files (*.*)")
    );

    if (filePath.isEmpty()) {
        return;
    }

    if (!m_game->saveToFile(filePath)) {
        QMessageBox::warning(this, tr("Save Failed"), tr("Failed to save game."));
        return;
    }

    qApp->quit();
}

void GameWindow::onGameOver()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QString::fromUtf8("游戏失败"));
    msgBox.setText(QString::fromUtf8("玩家血量归零，游戏结束！"));
    QPushButton* newGameBtn = msgBox.addButton(QString::fromUtf8("开启新游戏"), QMessageBox::ActionRole);
    QPushButton* loadGameBtn = msgBox.addButton(QString::fromUtf8("读档"), QMessageBox::ActionRole);
    
    msgBox.exec();

    if (msgBox.clickedButton() == newGameBtn) {
        if (m_game) {
            m_game->debugResetAll();
        }
    } else if (msgBox.clickedButton() == loadGameBtn) {
        const QString path = QFileDialog::getOpenFileName(
            this,
            tr("Load Game"),
            QString(),
            tr("Save Files (*.json);;All Files (*.*)")
        );
        if (!path.isEmpty()) {
            loadFromFile(path);
        }
    }
}

void GameWindow::onEnemyDefeated()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QString::fromUtf8("阶段胜利"));
    msgBox.setText(QString::fromUtf8("你通过了本阶段！"));
    QPushButton* nextStageBtn = msgBox.addButton(QString::fromUtf8("进入下一阶段"), QMessageBox::ActionRole);
    QPushButton* saveExitBtn = msgBox.addButton(QString::fromUtf8("保存并退出"), QMessageBox::ActionRole);
    
    msgBox.exec();

    if (msgBox.clickedButton() == nextStageBtn) {
        if (m_game) {
            m_game->startNextStage();
        }
    } else if (msgBox.clickedButton() == saveExitBtn) {
        if (m_game) {
            m_game->startNextStage();
            onSaveAndExitClicked();
        }
    }
}

// 刷新玩家/敌方信息面板。
void GameWindow::refreshInfoPanels()
{
    if (!m_game) {
        return;
    }

    QString phaseStr = m_game->isInBattle() ? QString::fromUtf8("战斗阶段") : QString::fromUtf8("部署阶段");
    m_phaseLabel->setText(QString::fromUtf8("%1\nStage: %2  Round: %3")
                          .arg(phaseStr)
                          .arg(m_game->stage())
                          .arg(m_game->round()));

    const int playerWarrior = m_game->traitCount(Unit::Owner::PlayerCtrl, Unit::Trait::Warrior);
    const int playerMage = m_game->traitCount(Unit::Owner::PlayerCtrl, Unit::Trait::Mage);
    const int playerArcher = m_game->traitCount(Unit::Owner::PlayerCtrl, Unit::Trait::Archer);
    const int enemyWarrior = m_game->traitCount(Unit::Owner::EnemyCtrl, Unit::Trait::Warrior);
    const int enemyMage = m_game->traitCount(Unit::Owner::EnemyCtrl, Unit::Trait::Mage);
    const int enemyArcher = m_game->traitCount(Unit::Owner::EnemyCtrl, Unit::Trait::Archer);

    const Player* player = m_game->player();
    int currentPop = 0;
    if (m_game) {
        // Here we just use countAliveUnits or write a loop in Game to get current player population on board
        const auto& units = m_game->units();
        for (auto* unit : units) {
            if (unit && unit->owner() == Unit::Owner::PlayerCtrl && m_game->isUnitOnBoard(unit)) {
                currentPop += 1;
            }
        }
    }
    
    m_playerInfoLabel->setText(QString::fromUtf8(
                                   "玩家\nHP: %1\n金币: %2\n人口: %3/%4\n羁绊: 战士 %5  法师 %6  弓手 %7")
                                   .arg(player ? player->Hp() : 0)
                                   .arg(player ? player->Gold() : 0)
                                   .arg(currentPop)
                                   .arg(player ? player->PopulationCap() : 0)
                                   .arg(playerWarrior)
                                   .arg(playerMage)
                                   .arg(playerArcher));

    m_enemyInfoLabel->setText(QString::fromUtf8(
                                  "敌方\n已战胜波次: %1/%2\n羁绊: 战士 %3  法师 %4  弓手 %5")
                                  .arg(m_game->enemyDefeatedWaves())
                                  .arg(m_game->enemyMaxWaves())
                                  .arg(enemyWarrior)
                                  .arg(enemyMage)
                                  .arg(enemyArcher));
}

// 战斗状态变化时禁用/启用部署按钮。
void GameWindow::onBattleStateChanged(bool inBattle)
{
    m_endDeploymentButton->setEnabled(!inBattle);
    m_endBattleDebugButton->setEnabled(inBattle);
    m_saveExitButton->setEnabled(!inBattle); // 战斗中禁用保存并退出按钮
    refreshInfoPanels();
}

void GameWindow::setupUI()
{
    // 设置主窗口与样式
    setCentralWidget(m_centralWidget);
    m_centralWidget->setLayout(m_mainLayout);

    // 配置样式表，定义整体配色和按钮样式。
    setStyleSheet(R"(
        QMainWindow {
            background-color: #2b2b2b;
        }
        QWidget {
            background-color: #2b2b2b;
            color: #f2f2f2;
        }
        QPushButton {
            background-color: #2f2f2f;
            color: #f2f2f2;
            border: 1px solid #565656;
            border-radius: 4px;
            padding: 6px 14px;
            font-size: 13px;
        }
        QPushButton:hover:!disabled {
            background-color: #3a3a3a;
        }
        QPushButton:pressed:!disabled {
            background-color: #242424;
        }
        QPushButton:disabled {
            background-color: #2f2f2f;
            color: #888888;
            border: 1px solid #444444;
        }
    )");

    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setDragMode(QGraphicsView::NoDrag);
    // 调用默认处理完成鼠标滑轮上下移动棋盘
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setMouseTracking(true);
    m_view->viewport()->setMouseTracking(true);

    // 信息栏布局：左侧敌方信息、右侧玩家信息，中间预留商店区域。
    QWidget* infoBar = new QWidget(this);
    QHBoxLayout* infoLayout = new QHBoxLayout(infoBar);
    infoLayout->setContentsMargins(6, 6, 6, 6);
    infoLayout->setSpacing(12);

    m_enemyInfoLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_enemyInfoLabel->setMinimumWidth(140);
    m_playerInfoLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_playerInfoLabel->setMinimumWidth(160);

    m_phaseLabel->setAlignment(Qt::AlignCenter);
    m_phaseLabel->setMinimumWidth(140);

    infoLayout->addWidget(m_enemyInfoLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_phaseLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_playerInfoLabel);

    m_mainLayout->addWidget(infoBar, 0);
    m_mainLayout->addWidget(m_view, 1);

    // 控制栏布局：左侧部署/调试按钮，右侧保存退出按钮，中间预留扩展空间。
    QWidget* controlBar = new QWidget(this);
    QHBoxLayout* controlLayout = new QHBoxLayout(controlBar);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->addWidget(m_endDeploymentButton);
    controlLayout->addWidget(m_saveExitButton);
    controlLayout->addWidget(m_resetButton);
    controlLayout->addStretch();
    controlLayout->addWidget(m_endBattleDebugButton);
    controlLayout->addWidget(m_resetAllDebugButton);
    m_mainLayout->addWidget(controlBar);

    connect(m_resetButton, &QPushButton::clicked,
            this, &GameWindow::onResetButtonClicked);
    connect(m_endDeploymentButton, &QPushButton::clicked,
        this, &GameWindow::onEndDeploymentClicked);
    connect(m_endBattleDebugButton, &QPushButton::clicked,
        this, &GameWindow::onEndBattleDebugClicked);
    connect(m_resetAllDebugButton, &QPushButton::clicked,
        this, &GameWindow::onResetAllDebugClicked);
    connect(m_saveExitButton, &QPushButton::clicked,
        this, &GameWindow::onSaveAndExitClicked);
    connect(m_game, &Game::playerInfoChanged,
        this, &GameWindow::refreshInfoPanels);
    connect(m_game, &Game::enemyInfoChanged,
        this, &GameWindow::refreshInfoPanels);
    connect(m_game, &Game::stageRoundChanged,
        this, &GameWindow::refreshInfoPanels);
    connect(m_game, &Game::battleStateChanged,
        this, &GameWindow::onBattleStateChanged);
    connect(m_game, &Game::gameOver,
        this, &GameWindow::onGameOver);
    connect(m_game, &Game::enemyDefeated,
        this, &GameWindow::onEnemyDefeated);

    const bool inBattle = m_game->isInBattle();
    m_endDeploymentButton->setEnabled(!inBattle);
    m_endBattleDebugButton->setEnabled(inBattle);    

    m_view->setScene(m_game->scene());
}
