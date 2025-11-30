#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QStyle> // Для стандартных иконок

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Берем стандартные иконки папки и файла из системы
    dirIcon = style()->standardIcon(QStyle::SP_DirIcon);
    fileIcon = style()->standardIcon(QStyle::SP_FileIcon);

    // Настройка дерева
    ui->fileTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->fileTree, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::showContextMenu);

    // Первая отрисовка (покажет только root)
    refreshTree();
}

MainWindow::~MainWindow() {
    delete ui;
}

// ==========================================
// ОТРИСОВКА (Core -> View)
// ==========================================

void MainWindow::refreshTree() {
    ui->fileTree->clear();

    // Получаем корень
    VFSDirectory* root = explorer.getRoot();

    // Создаем элемент для корня
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(ui->fileTree);
    rootItem->setText(0, "/");
    rootItem->setIcon(0, dirIcon);
    // Сохраняем указатель на реальный узел (опционально, но полезно)
    rootItem->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(root)));

    // Запускаем рекурсию
    addTreeItemsRecursive(root, rootItem);

    // Раскрываем корень
    rootItem->setExpanded(true);
}

void MainWindow::addTreeItemsRecursive(VFSNode* node, QTreeWidgetItem* parentItem) {
    if (!node->isDirectory()) return;

    VFSDirectory* dir = static_cast<VFSDirectory*>(node);

    // Сортировка для красоты (опционально)
    // std::vector<std::unique_ptr<VFSNode>>& children = dir->getChildren();
    // ... тут можно отсортировать, но у тебя вектор unique_ptr, это сложно.
    // Оставим как есть.

    for (const auto& child : dir->getChildren()) {
        QTreeWidgetItem* item = new QTreeWidgetItem(parentItem);

        // Имя
        item->setText(0, QString::fromStdString(child->getName()));

        // Размер
        item->setText(1, QString::number(child->getSize()) + " B");

        // Иконка
        if (child->isDirectory()) {
            item->setIcon(0, dirIcon);
            // Рекурсия внутрь
            addTreeItemsRecursive(child.get(), item);
        } else {
            item->setIcon(0, fileIcon);
        }
    }
}

// ==========================================
// ЛОГИКА (View -> Core)
// ==========================================

// Вспомогательная функция: собирает путь от выбранного элемента к корню
std::string MainWindow::getCurrentPath() {
    QTreeWidgetItem* item = ui->fileTree->currentItem();
    if (!item) return "/"; // Если ничего не выбрано - корень

    // Собираем путь снизу вверх
    QStringList parts;
    while (item) {
        QString text = item->text(0);
        if (text == "/") break; // Дошли до корня
        parts.prepend(text);
        item = item->parent();
    }

    // Склеиваем в "/home/user"
    if (parts.isEmpty()) return "/";
    return "/" + parts.join("/").toStdString();
}

void MainWindow::on_btnCreateFolder_clicked() {
    // 1. Узнаем, где создавать
    std::string currentPath = getCurrentPath();

    // 2. Спрашиваем имя
    bool ok;
    QString text = QInputDialog::getText(this, "Новая папка",
                                         "Имя папки:", QLineEdit::Normal,
                                         "NewFolder", &ok);
    if (ok && !text.isEmpty()) {
        try {
            // 3. Вызываем CORE
            explorer.createDirectory(currentPath, text.toStdString());
            // 4. Обновляем UI
            refreshTree();
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Ошибка", e.what());
        }
    }
}

void MainWindow::on_btnMountFile_clicked() {
    std::string currentPath = getCurrentPath();

    // Открываем диалог выбора реального файла
    QString realPath = QFileDialog::getOpenFileName(this, "Выберите файл", QDir::homePath());
    if (realPath.isEmpty()) return;

    // Имя виртуального файла = имя реального файла (по умолчанию)
    QFileInfo fileInfo(realPath);
    QString virtName = fileInfo.fileName();

    try {
        explorer.createFile(currentPath, virtName.toStdString(), realPath.toStdString());
        refreshTree();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Ошибка", e.what());
    }
}

void MainWindow::on_btnDelete_clicked() {
    std::string currentPath = getCurrentPath();
    if (currentPath == "/") {
        QMessageBox::warning(this, "Стоп", "Нельзя удалить корень!");
        return;
    }

    try {
        explorer.deleteNode(currentPath);
        refreshTree();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Ошибка", e.what());
    }
}

// ==========================================
// ПОИСК
// ==========================================

void MainWindow::on_searchEdit_textChanged(const QString &arg1) {
    // АВТОДОПОЛНЕНИЕ (Trie)
    std::string prefix = arg1.toStdString();
    if (prefix.empty()) {
        ui->searchResultList->clear();
        return;
    }

    // Вызываем Trie
    // Предполагается, что ты добавил метод getSuggestions в VFSExplorer
    // std::vector<std::string> suggestions = explorer.getSuggestions(prefix);

    // Пока заглушка, если метод не готов:
    ui->searchResultList->clear();
    // for (const auto& s : suggestions) ui->searchResultList->addItem(QString::fromStdString(s));
}

void MainWindow::on_btnSearchFast_clicked() {
    QString query = ui->searchEdit->text();
    if (query.isEmpty()) return;

    auto start = std::chrono::high_resolution_clock::now();

    // Вызываем Hash Map
    auto results = explorer.searchByIndex(query.toStdString());

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    QString msg = QString("Найдено файлов: %1\nВремя (Hash): %2 ns")
                      .arg(results.size())
                      .arg(duration);

    QMessageBox::information(this, "Результат", msg);

    // Можно выделить найденные файлы в дереве (это сложнее, пока просто покажем кол-во)
}

void MainWindow::on_btnSearchSlow_clicked() {
    QString query = ui->searchEdit->text();
    ui->searchResultList->clear();

    auto start = std::chrono::high_resolution_clock::now();
    auto results = explorer.searchByTraversal(query.toStdString()); // Твой метод Tree
    auto end = std::chrono::high_resolution_clock::now();

    for (VFSNode* node : results) {
        ui->searchResultList->addItem(QString::fromStdString(node->getName()) + " [SLOW]");
    }

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    QMessageBox::information(this, "Результат", "Найдено: " + QString::number(results.size()) + "\nВремя: " + QString::number(duration) + " ns");
}


// Контекстное меню для удобства
void MainWindow::showContextMenu(const QPoint &pos) {
    QMenu contextMenu(tr("Context menu"), this);

    QAction actionCreate("Создать папку", this);
    connect(&actionCreate, &QAction::triggered, this, &MainWindow::on_btnCreateFolder_clicked);
    contextMenu.addAction(&actionCreate);

    QAction actionMount("Добавить файл", this);
    connect(&actionMount, &QAction::triggered, this, &MainWindow::on_btnMountFile_clicked);
    contextMenu.addAction(&actionMount);

    QAction actionDel("Удалить", this);
    connect(&actionDel, &QAction::triggered, this, &MainWindow::on_btnDelete_clicked);
    contextMenu.addAction(&actionDel);

    contextMenu.exec(ui->fileTree->mapToGlobal(pos));
}

void MainWindow::on_searchResultList_itemClicked(QListWidgetItem *item) {
}
