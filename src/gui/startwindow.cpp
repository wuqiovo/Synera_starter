#include "startwindow.h"
#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

// 初始窗口构造：创建基础 UI。
StartWindow::StartWindow(QWidget* parent)
	: QMainWindow(parent)
	, m_centralWidget(new QWidget(this))
	, m_mainLayout(new QVBoxLayout())
	, m_titleLabel(new QLabel("Synera Starter", this))
	, m_newGameButton(new QPushButton(tr("Start New Game"), this))
	, m_loadGameButton(new QPushButton(tr("Load Game"), this))
{
	setupUI();
}

// 触发新游戏信号。
void StartWindow::onNewGameClicked()
{
	emit startNewGame();
}

// 弹出文件选择并触发读档信号。
void StartWindow::onLoadGameClicked()
{
	const QString filePath = QFileDialog::getOpenFileName(
		this,
		tr("Load Game"),
		QString(),
		tr("Save Files (*.json *.sav);;All Files (*.*)")
	);

	if (filePath.isEmpty()) {
		return;
	}

	emit loadGameRequested(filePath);
}

// 构建初始窗口布局。
void StartWindow::setupUI()
{
	setCentralWidget(m_centralWidget);
	m_centralWidget->setLayout(m_mainLayout);

	setWindowTitle("Synera - Starter");
	setMinimumSize(420, 260);

	m_titleLabel->setAlignment(Qt::AlignCenter);
	QFont titleFont = m_titleLabel->font();
	titleFont.setPointSize(18);
	titleFont.setBold(true);
	m_titleLabel->setFont(titleFont);

	m_mainLayout->addStretch();
	m_mainLayout->addWidget(m_titleLabel);
	m_mainLayout->addSpacing(18);
	m_mainLayout->addWidget(m_newGameButton);
	m_mainLayout->addWidget(m_loadGameButton);
	m_mainLayout->addStretch();
	m_mainLayout->setContentsMargins(40, 30, 40, 30);
	m_mainLayout->setSpacing(10);

	connect(m_newGameButton, &QPushButton::clicked,
			this, &StartWindow::onNewGameClicked);
	connect(m_loadGameButton, &QPushButton::clicked,
			this, &StartWindow::onLoadGameClicked);
}