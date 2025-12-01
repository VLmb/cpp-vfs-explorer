#pragma once

#include <QMainWindow>
#include <QTreeWidget>
#include <QListWidget>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QStandardPaths>
#include <QCompleter>
#include <QStringListModel>

// Подключаем наш менеджер из CORE
// Путь зависит от INCLUDEPATH, но обычно так:
#include "../core/domain/VFSExplorer.h"
#include "../core/utils/ScriptLoader.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// Главное окно приложения.
// Отвечает за визуальную часть (Qt-виджеты) и дергает методы VFSExplorer,
// который реализует логику виртуальной файловой системы.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    // Конструктор главного окна.
    // parent передается в базовый QMainWindow (обычно nullptr — верхнеуровневое окно).
    MainWindow(QWidget *parent = nullptr);

    // Деструктор. Освобождает сгенерированный класс Ui::MainWindow.
    ~MainWindow();

private slots:
    // Слот для кнопки "Создать папку".
    // Qt автоматически привяжет его к QPushButton с objectName = "btnCreateFolder"
    // благодаря имени on_<objectName>_clicked().
    void on_btnCreateFolder_clicked();

    // Слот для кнопки "Добавить файл" (монтирование реального файла в виртуальную ФС).
    void on_btnMountFile_clicked();

    void on_btnInitFS_clicked();

    // Слот для кнопки "Удалить" (удаление выбранного узла).
    void on_btnDelete_clicked();

    // Слот "Быстрый поиск" (по индексу, Hash Map).
    void on_btnSearchFast_clicked();

    // Слот "Медленный поиск" (полный обход дерева).
    void on_btnSearchSlow_clicked();

    // Слот, вызывается при изменении текста в поле поиска.
    // Используется для автодополнения (Trie).
    void on_searchEdit_textChanged(const QString &arg1);

    // Слот, вызывается при клике по элементу в списке результатов поиска.
    void on_searchResultList_itemClicked(QListWidgetItem *item);

    // Слот, показывающий контекстное меню по правому клику по дереву.
    void showContextMenu(const QPoint &pos);

    void on_btnExpandAll_clicked();

    void on_btnRename_clicked();

    void on_btnCopyPath_clicked();

    void on_fileTree_itemDoubleClicked(QTreeWidgetItem* item, int column);

    VFSDirectory* getTargetDirForItem(QTreeWidgetItem* item);

    void insertSearchCompletion(const QString& completion);

    void on_btnCreateFile_clicked();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    // Указатель на сгенерованный класс UI (из .ui файла).
    // В нем хранятся все виджеты (fileTree, searchEdit и т.д.).
    Ui::MainWindow *ui;

    // "Мозг" приложения — объект, управляющий виртуальной файловой системой.
    VFSExplorer explorer;

    // --- Вспомогательные методы ---

    // Полная перерисовка дерева (Core -> UI).
    // Берет состояние из VFSExplorer и строит QTreeWidget.
    void refreshTree();

    // Рекурсивный обход дерева VFS и наполнение QTreeWidget.
    // node — текущий виртуальный узел (директория),
    // parentItem — соответствующий элемент в QTreeWidget.
    void addTreeItemsRecursive(VFSNode* node, QTreeWidgetItem* parentItem);
    void addSearchResultItem(VFSNode* node, const QString& tag = QString());
    void showNodeInfo(VFSNode* node);

    // Получить полный путь выбранного элемента в дереве (например "/home/user/doc").
    // Собирает путь, поднимаясь от QTreeWidgetItem к корню.
    std::string getCurrentPath();

    // Иконки для отображения папок и файлов в QTreeWidget.
    QIcon dirIcon;
    QIcon fileIcon;

    QCompleter* searchCompleter = nullptr;
    QStringListModel* searchModel = nullptr;

};
