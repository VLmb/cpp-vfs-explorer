#pragma once

#include <QMainWindow>
#include <QTreeWidget>
#include <QListWidget>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QStandardPaths>

// Подключаем наш менеджер из CORE
// Путь зависит от INCLUDEPATH, но обычно так:
#include "../core/domain/VFSExplorer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Слоты для кнопок
    void on_btnCreateFolder_clicked();
    void on_btnMountFile_clicked();
    void on_btnDelete_clicked();

    // Слоты поиска
    void on_btnSearchFast_clicked();
    void on_btnSearchSlow_clicked();
    void on_searchEdit_textChanged(const QString &arg1); // Для автодополнения
    void on_searchResultList_itemClicked(QListWidgetItem *item);

    // Контекстное меню (ПКМ)
    void showContextMenu(const QPoint &pos);

private:
    Ui::MainWindow *ui;

    // НАШ МОЗГ
    VFSExplorer explorer;

    // --- Вспомогательные методы ---

    // Полная перерисовка дерева (Core -> UI)
    void refreshTree();

    // Рекурсивный обход для отрисовки
    void addTreeItemsRecursive(VFSNode* node, QTreeWidgetItem* parentItem);

    // Получить полный путь выбранного элемента в дереве ("/home/user/doc")
    std::string getCurrentPath();

    // Иконки (для красоты)
    QIcon dirIcon;
    QIcon fileIcon;
};
