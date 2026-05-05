#ifndef STARTWINDOW_H
#define STARTWINDOW_H

#include <QMainWindow>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

class StartWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit StartWindow(QWidget* parent = nullptr);

signals:
	// 新游戏按钮触发信号。
	void startNewGame();
	// 读档按钮触发信号，携带存档路径。
	void loadGameRequested(const QString& savePath);

private slots:
	// 新游戏按钮回调。
	void onNewGameClicked();
	// 读档按钮回调。
	void onLoadGameClicked();

private:
	void setupUI();

	// 窗口中心容器。
	QWidget* m_centralWidget;
	// 主布局（纵向）。
	QVBoxLayout* m_mainLayout;
	// 标题文本。
	QLabel* m_titleLabel;
	// 新游戏按钮。
	QPushButton* m_newGameButton;
	// 读档按钮。
	QPushButton* m_loadGameButton;
};

#endif // STARTWINDOW_H