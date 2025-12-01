#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QStyle> // Для стандартных иконок
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>

#include "../core/domain/VFSFile.h"


class NodeInfoDialog : public QDialog {
public:
    NodeInfoDialog(VFSNode* node, VFSExplorer& explorer, QWidget* parent = nullptr)
        : QDialog(parent)
        , m_node(node)
        , m_explorer(explorer)
    {
        setWindowTitle("Свойства объекта");

        // --- Основные поля ---
        nameValue = new QLabel(QString::fromStdString(node->getName()), this);
        typeValue = new QLabel(node->isDirectory() ? "Каталог" : "Файл", this);

        // Дата создания
        QDateTime dt = QDateTime::fromSecsSinceEpoch(node->getCreationTime());
        createdValue = new QLabel(dt.toString("dd.MM.yyyy hh:mm:ss"), this);

        // Реальный путь — только для файла
        std::string physicalPathStr;
        if (!node->isDirectory()) {
            if (auto* file = dynamic_cast<VFSFile*>(node)) {
                physicalPathStr = file->getPhysicalPath();
            }
        }
        physicalPathValue = new QLabel(QString::fromStdString(physicalPathStr), this);

        // --- Форма ---
        auto* form = new QFormLayout;
        form->addRow("Имя:", nameValue);
        form->addRow("Тип:", typeValue);
        if (!node->isDirectory()) {
            form->addRow("Реальный путь:", physicalPathValue);
        }
        form->addRow("Создан:", createdValue);

        // --- Кнопки ---
        auto* openButton   = new QPushButton("Открыть", this);
        auto* renameButton = new QPushButton("Переименовать", this);
        auto* deleteButton = new QPushButton("Удалить", this);
        auto* closeButton  = new QPushButton("Закрыть", this);

        // Открывать имеет смысл только файл с непустым physicalPath
        if (node->isDirectory() || physicalPathStr.empty()) {
            openButton->setEnabled(false);
        }

        auto* buttons = new QHBoxLayout;
        buttons->addWidget(openButton);
        buttons->addWidget(renameButton);
        buttons->addWidget(deleteButton);
        buttons->addStretch();
        buttons->addWidget(closeButton);

        auto* mainLayout = new QVBoxLayout;
        mainLayout->addLayout(form);
        mainLayout->addLayout(buttons);
        setLayout(mainLayout);

        // --- Логика кнопок ---

        // Открыть файл системной программой
        connect(openButton, &QPushButton::clicked, this, [physicalPathStr]() {
            if (physicalPathStr.empty()) return;
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(QString::fromStdString(physicalPathStr)));
        });

        // Переименовать
        connect(renameButton, &QPushButton::clicked, this, [this]() {
            bool ok = false;
            QString newName = QInputDialog::getText(
                this,
                "Переименование",
                "Новое имя:",
                QLineEdit::Normal,
                QString::fromStdString(m_node->getName()),
                &ok
                );
            if (!ok || newName.isEmpty()) return;

            try {
                // ВАРИАНТ 1: если есть метод в explorer
                m_explorer.renameNode(m_node->getName(), newName.toStdString());
                // ВАРИАНТ 2 (если renameNode нет) — можешь временно вызвать:
                // node->rename(newName.toStdString());
                m_modified = true;

                nameValue->setText(newName);
            } catch (const std::exception& e) {
                QMessageBox::critical(this, "Ошибка", e.what());
            }
        });

        // Удалить
        connect(deleteButton, &QPushButton::clicked, this, [this]() {
            auto ret = QMessageBox::question(
                this,
                "Удаление",
                "Точно удалить этот объект?"
                );
            if (ret != QMessageBox::Yes) return;

            try {
                m_explorer.deleteNode(m_node->getName());
                m_modified = true;
                accept(); // закрываем диалог
            } catch (const std::exception& e) {
                QMessageBox::critical(this, "Ошибка", e.what());
            }
        });

        // Закрыть
        connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    }

    bool isModified() const { return m_modified; }

private:
    VFSNode* m_node;
    VFSExplorer& m_explorer;

    QLabel* nameValue   = nullptr;
    QLabel* typeValue   = nullptr;
    QLabel* physicalPathValue = nullptr;
    QLabel* createdValue  = nullptr;

    bool m_modified = false;
};

// Конструктор главного окна.
// Здесь:
// 1) создаются и настраиваются виджеты из .ui файла (ui->setupUi),
// 2) берутся стандартные системные иконки,
// 3) настраивается контекстное меню,
// 4) первый раз отрисовывается дерево файлов.
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    // Создаем объект UI и "подшиваем" к этому окну
    // (инициализирует все поля ui->..., которые описаны в .ui).
    ui->setupUi(this);

    ui->searchEdit->setPlaceholderText("Введите имя файла...");

    // === Настройка autocomplete ===
    searchModel = new QStringListModel(this);
    searchCompleter = new QCompleter(searchModel, this);
    searchCompleter->setCompletionMode(QCompleter::PopupCompletion); // выпадающий список
    searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);        // без учета регистра
    searchCompleter->setFilterMode(Qt::MatchContains);               // совпадение по вхождению

    ui->searchEdit->setCompleter(searchCompleter);

    // Берем стандартные иконки папки и файла из темы оформления системы.
    // style() — метод QWidget, возвращает текущий QStyle.
    // standardIcon(...) — дает "стандартную" иконку (папка, файл, диск и т.п.).
    dirIcon = style()->standardIcon(QStyle::SP_DirIcon);
    fileIcon = style()->standardIcon(QStyle::SP_FileIcon);

    // Настройка дерева:
    // Qt::CustomContextMenu говорит, что контекстное меню (ПКМ)
    // будет обрабатываться вручную через сигнал customContextMenuRequested.
    ui->fileTree->setContextMenuPolicy(Qt::CustomContextMenu);

    // Связка сигнала и слота:
    // когда на fileTree вызывается customContextMenuRequested(QPoint),
    // вызываем наш слот MainWindow::showContextMenu(const QPoint&).
    connect(ui->fileTree, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::showContextMenu);

    // Первая отрисовка дерева (покажет только root).
    // Дальше этот метод будет вызываться после любых изменений в VFS.
    refreshTree();
}

// Деструктор.
// Здесь важно удалить ui, чтобы освободить все созданные виджеты.
MainWindow::~MainWindow() {
    delete ui;
}

static QString formatSize(std::size_t bytes)
{
    const double KB = 1024.0;
    const double MB = 1024.0 * 1024.0;

    if (bytes < 1024) {
        // Меньше 1 КБ — показываем в байтах
        return QString::number(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        // От 1 КБ до 1 МБ — показываем в килобайтах
        double kbValue = bytes / KB;
        return QString::number(kbValue, 'f', 1) + " KB";  // например, 1.5 KB
    } else {
        // 1 МБ и больше — показываем в мегабайтах
        double mbValue = bytes / MB;
        return QString::number(mbValue, 'f', 1) + " MB";  // например, 3.2 MB
    }
}

// ==========================================
// ОТРИСОВКА (Core -> View)
// ==========================================

// Полностью перерисовывает дерево файлов.
// Берет корень из VFSExplorer, создает root-элемент дерева,
// рекурсивно добавляет все дочерние узлы.
void MainWindow::refreshTree() {
    // Удаляем все элементы из QTreeWidget.
    ui->fileTree->clear();

    // Получаем корень виртуальной файловой системы.
    VFSDirectory* root = explorer.getRoot();

    // Создаем элемент для корня в QTreeWidget.
    // Конструктор QTreeWidgetItem(ui->fileTree) автоматически добавляет
    // item как верхнеуровневый элемент в дерево.
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(ui->fileTree);

    // В первой колонке (column 0) показываем имя корня — "/".
    rootItem->setText(0, "/");

    // Ставим иконку "папка" для корня.
    rootItem->setIcon(0, dirIcon);

    // Сохраняем указатель на реальный узел (VFSDirectory) в элемент дерева.
    // setData(column, role, value) позволяет хранить произвольные данные.
    // Qt::UserRole — роль "для пользователя".
    // QVariant::fromValue(static_cast<void*>(root)) — кладем сырый указатель как void*.
    // (Это опционально, но удобно, если потом захочешь доставать VFSNode напрямую из item.)
    rootItem->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(root)));

    // Рекурсивно добавляем все дочерние элементы.
    addTreeItemsRecursive(root, rootItem);

    // Раскрываем корень, чтобы сразу видеть его содержимое.
    rootItem->setExpanded(true);
}

// Рекурсивно обходит дерево VFS и создает элементы в QTreeWidget.
// node — текущий виртуальный узел,
// parentItem — соответствующий родительский элемент в дереве Qt.
void MainWindow::addTreeItemsRecursive(VFSNode* node, QTreeWidgetItem* parentItem) {
    // Если узел не директория — дальше нечего обходить (у файлов нет детей).
    if (!node->isDirectory()) return;

    // Приводим базовый указатель VFSNode* к VFSDirectory* (мы уверены, что это папка).
    VFSDirectory* dir = static_cast<VFSDirectory*>(node);

    // Здесь можно было бы отсортировать детей (по имени, по размеру и т.д.),
    // но children хранятся как std::unique_ptr<VFSNode>, что усложняет сортировку.
    // Поэтому сейчас просто идем в текущем порядке.

    // dir->getChildren() возвращает ссылку на контейнер с дочерними узлами.
    for (const auto& child : dir->getChildren()) {
        QTreeWidgetItem* item = new QTreeWidgetItem(parentItem);

        item->setText(0, QString::fromStdString(child->getName()));
        item->setText(1, formatSize(child->getSize()));

        // ВАЖНО: привязываем VFSNode* к item
        item->setData(0, Qt::UserRole,
                      QVariant::fromValue(static_cast<void*>(child.get())));

        if (child->isDirectory()) {
            item->setIcon(0, dirIcon);
            addTreeItemsRecursive(child.get(), item);
        } else {
            item->setIcon(0, fileIcon);
        }
    }
}

void MainWindow::addSearchResultItem(VFSNode* node, const QString& tag)
{
    auto* item = new QListWidgetItem(ui->searchResultList);

    QString text = QString::fromStdString(node->getName());
    if (!tag.isEmpty()) {
        text += " " + tag;
    }
    item->setText(text);

    // Привязка VFSNode*
    item->setData(Qt::UserRole,
                  QVariant::fromValue(static_cast<void*>(node)));

    // Можно повесить tooltip
    QString tooltip = QString("Имя: %1\nПуть: %2")
                          .arg(QString::fromStdString(node->getName()))
                          .arg(QString::fromStdString("..."));
    item->setToolTip(tooltip);
}

// ==========================================
// ЛОГИКА (View -> Core)
// ==========================================

// Вспомогательная функция: собирает полный виртуальный путь
// выбранного элемента в дереве, поднимаясь от него к корню.
// Пример: если пользователь выделил "docs/hello.txt", вернет "/docs/hello.txt".
std::string MainWindow::getCurrentPath() {
    // currentItem() — текущий выделенный элемент в QTreeWidget.
    QTreeWidgetItem* item = ui->fileTree->currentItem();

    // Если ничего не выбрано — считаем, что работаем с корнем "/".
    if (!item) return "/";

    // parts будет хранить имена всех узлов от корня до выбранного (без "/").
    QStringList parts;

    // Идем от текущего элемента вверх по дереву через parent().
    while (item) {
        // Берем текст в колонке 0 (имя узла).
        QString text = item->text(0);

        // Если дошли до корня ("/") — дальше подниматься не нужно.
        if (text == "/") break;

        // Добавляем имя в начало списка (prepend),
        // чтобы потом получить правильный порядок: root/child/subchild.
        parts.prepend(text);

        // Переходим к родителю.
        item = item->parent();
    }

    // Если parts пустой — значит, это корень.
    if (parts.isEmpty()) return "/";

    // Склеиваем имена через "/" и добавляем ведущий "/".
    // Например, parts = ["home", "user"] -> "/home/user".
    return "/" + parts.join("/").toStdString();
}

// Слот для кнопки "Создать папку".
// 1) Определяет текущий путь (getCurrentPath),
// 2) спрашивает имя новой папки через QInputDialog,
// 3) вызывает explorer.createDirectory,
// 4) перерисовывает дерево.
void MainWindow::on_btnCreateFolder_clicked() {
    // 1. Узнаем, в какой директории создавать новую папку.
    std::string currentPath = getCurrentPath();

    // 2. Спрашиваем у пользователя имя папки через модальный диалог.
    bool ok;
    QString text = QInputDialog::getText(this, "Новая папка",
                                         "Имя папки:", QLineEdit::Normal,
                                         "NewFolder", &ok);
    // ok == true, если пользователь нажал ОК.
    // text.isEmpty() — если строка пустая.
    if (ok && !text.isEmpty()) {
        try {
            // 3. Вызываем логику (CORE).
            explorer.createDirectory(currentPath, text.toStdString());

            // 4. Обновляем UI, чтобы новая папка появилась в дереве.
            refreshTree();
        } catch (const std::exception& e) {
            // В случае ошибки показываем диалог критической ошибки.
            QMessageBox::critical(this, "Ошибка", e.what());
        }
    }
}

void MainWindow::on_btnInitFS_clicked()
{
    // Инициализируем виртуальную ФС по скрипту
    ScriptLoader::load(explorer);          // путь по умолчанию: resources/script.txt

    // Обновляем дерево, чтобы сразу увидеть созданные папки/файлы
    refreshTree();

    // Делаем кнопку неактивной, чтобы её нельзя было нажать второй раз
    ui->btnInitFS->setEnabled(false);

    // (опционально) можно вывести пользователю сообщение:
    QMessageBox::information(this, "Инициализация", "Файловая система инициализирована.");
}

// Слот для кнопки "Добавить файл" (монтирование реального файла).
// 1) Определяет текущий виртуальный путь,
// 2) открывает QFileDialog для выбора реального файла,
// 3) создает виртуальный файл в VFSExplorer и перерисовывает дерево.
void MainWindow::on_btnMountFile_clicked() {
    std::string currentPath = getCurrentPath();

    // Диалог выбора реального файла.
    // this — родительское окно, "Выберите файл" — заголовок,
    // QDir::homePath() — стартовая директория (домашняя).
    QString physicalPath = QFileDialog::getOpenFileName(this, "Выберите файл", QDir::homePath());

    // Если пользователь нажал "Отмена" — строка будет пустой.
    if (physicalPath.isEmpty()) return;

    // QFileInfo даёт доступ к различным частям пути:
    // имя файла, расширение, абсолютный путь и т.д.
    QFileInfo fileInfo(physicalPath);

    // Имя виртуального файла по умолчанию — просто имя реального файла.
    QString virtName = fileInfo.fileName();

    try {
        // Создаем файл в виртуальной ФС, передавая:
        // 1) виртуальный путь (директория),
        // 2) виртуальное имя,
        // 3) реальный путь к файлу.
        explorer.createFile(currentPath, virtName.toStdString(), physicalPath.toStdString());

        // Перерисовываем дерево после изменения.
        refreshTree();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Ошибка", e.what());
    }
}

// Слот для кнопки "Удалить".
// Удаляет текущий выбранный узел (файл или папку) в виртуальной ФС.
void MainWindow::on_btnDelete_clicked() {
    std::string currentPath = getCurrentPath();

    // Защита от удаления корня.
    if (currentPath == "/") {
        QMessageBox::warning(this, "Стоп", "Нельзя удалить корень!");
        return;
    }

    try {
        // Удаляем узел по пути.
        explorer.deleteNode(currentPath);

        // Перерисовываем дерево.
        refreshTree();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Ошибка", e.what());
    }
}

// ==========================================
// ПОИСК
// ==========================================

// Слот, вызывается при каждом изменении текста в поле поиска.
// Планируется использовать для автодополнения по Trie.
void MainWindow::on_searchEdit_textChanged(const QString &arg1) {
    // Строка префикса для Trie
    std::string prefix = arg1.toStdString();

    if (prefix.length() < 4) {
        return;
    }

    // Очищаем список результатов «медленного» поиска, если был
    ui->searchResultList->clear();

    // Если ничего не введено — скрываем все подсказки
    if (prefix.empty()) {
        searchModel->setStringList(QStringList());
        return;
    }

    // Получаем подсказки из VFSExplorer
    std::vector<std::string> suggestions = explorer.getSuggestions(prefix);

    // Перекладываем в QStringList для модели
    QStringList list;
    list.reserve(static_cast<int>(suggestions.size()));
    for (const auto& s : suggestions) {
        list << QString::fromStdString(s);
    }

    // Обновляем модель комплитера
    searchModel->setStringList(list);
}

// Слот "Быстрый поиск" (по Hash Map / индексу).
// Измеряет время поиска и показывает количество найденных файлов.
void MainWindow::on_btnSearchFast_clicked() {
    QString query = ui->searchEdit->text();
    if (query.isEmpty()) return;

    ui->searchResultList->clear();

    auto start = std::chrono::high_resolution_clock::now();
    auto results = explorer.searchByIndex(query.toStdString());
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    for (VFSNode* node : results) {
        addSearchResultItem(node, "[FAST]");
    }

    QString msg = QString("Найдено файлов: %1\nВремя (Hash): %2 ns")
                      .arg(results.size())
                      .arg(duration);
    QMessageBox::information(this, "Результат", msg);
}

void MainWindow::on_btnSearchSlow_clicked() {
    QString query = ui->searchEdit->text();
    ui->searchResultList->clear();

    auto start = std::chrono::high_resolution_clock::now();
    auto results = explorer.searchByTraversal(query.toStdString());
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    for (VFSNode* node : results) {
        addSearchResultItem(node, "[SLOW]");
    }

    QMessageBox::information(this, "Результат",
                             "Найдено: " + QString::number(results.size()) +
                                 "\nВремя: " + QString::number(duration) + " ns");
}

void MainWindow::showNodeInfo(VFSNode* node)
{
    if (!node) return;

    NodeInfoDialog dlg(node, explorer, this);
    int result = dlg.exec();

    if (result != QDialog::Accepted || !dlg.isModified()) {
        return;
    }

    // После возможного удаления/переименования — перерисуем дерево
    refreshTree();
}



// Контекстное меню для дерева файлов.
// Вызывается, когда пользователь нажимает ПКМ по QTreeWidget
// (сигнал customContextMenuRequested).
void MainWindow::showContextMenu(const QPoint &pos) {
    // Создаем контекстное меню. tr() — для поддержки переводов.
    QMenu contextMenu(tr("Context menu"), this);

    // Действие "Создать папку".
    QAction actionCreate("Создать папку", this);
    // Привязываем сигнал triggered() к уже существующему слоту создания папки.
    connect(&actionCreate, &QAction::triggered, this, &MainWindow::on_btnCreateFolder_clicked);
    contextMenu.addAction(&actionCreate);

    // Действие "Добавить файл".
    QAction actionMount("Добавить файл", this);
    connect(&actionMount, &QAction::triggered, this, &MainWindow::on_btnMountFile_clicked);
    contextMenu.addAction(&actionMount);

    // Действие "Удалить".
    QAction actionDel("Удалить", this);
    connect(&actionDel, &QAction::triggered, this, &MainWindow::on_btnDelete_clicked);
    contextMenu.addAction(&actionDel);

    // Показываем меню в глобальных координатах экрана.
    // mapToGlobal(pos) переводит координаты относительно виджета
    // в координаты всего экрана.
    contextMenu.exec(ui->fileTree->mapToGlobal(pos));
}

// Слот, вызывается при клике по элементу списка результатов поиска.
// Сейчас пустой — сюда можно добавить логику:
// например, найти соответствующий элемент в дереве и выделить его.
void MainWindow::on_searchResultList_itemClicked(QListWidgetItem *item) {
    if (!item) return;

    QVariant v = item->data(Qt::UserRole);
    auto* node = static_cast<VFSNode*>(v.value<void*>());
    showNodeInfo(node);
}

void MainWindow::on_fileTree_itemDoubleClicked(QTreeWidgetItem* item, int)
{
    if (!item) return;

    QVariant v = item->data(0, Qt::UserRole);
    auto* node = static_cast<VFSNode*>(v.value<void*>());
    showNodeInfo(node);
}

void MainWindow::on_btnExpandAll_clicked()
{
    ui->fileTree->expandAll();
}
