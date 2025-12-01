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
#include <QClipboard>
#include <QGuiApplication>
#include <QEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QDir>


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
                m_explorer.renameNode(m_node, newName.toStdString());
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
                m_explorer.deleteNode(m_node);
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

class CreateFileDialog : public QDialog {
public:
    explicit CreateFileDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Создать файл");

        m_virtualNameEdit  = new QLineEdit(this);
        m_physicalPathEdit = new QLineEdit(this);

        // Значения по умолчанию
        m_virtualNameEdit->setText("new_file.txt");
        m_physicalPathEdit->setText(QDir::home().filePath("new_file.txt"));

        auto* form = new QFormLayout;
        form->addRow("Имя файла в виртуальной системе:", m_virtualNameEdit);
        form->addRow("Путь к файлу на диске:",           m_physicalPathEdit);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            Qt::Horizontal,
            this
            );

        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto* mainLayout = new QVBoxLayout;
        mainLayout->addLayout(form);
        mainLayout->addWidget(buttons);

        setLayout(mainLayout);
    }

    QString virtualName() const  { return m_virtualNameEdit->text().trimmed(); }
    QString physicalPath() const { return m_physicalPathEdit->text().trimmed(); }

private:
    QLineEdit* m_virtualNameEdit  = nullptr;
    QLineEdit* m_physicalPathEdit = nullptr;
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

    ui->fileTree->setDragEnabled(true);                         // можно тянуть элементы
    ui->fileTree->setAcceptDrops(true);                         // дерево принимает drop
    ui->fileTree->setDropIndicatorShown(true);
    ui->fileTree->setDragDropMode(QAbstractItemView::DragDrop); // и drag, и drop
    ui->fileTree->viewport()->setAcceptDrops(true);
    ui->fileTree->viewport()->installEventFilter(this);

    ui->searchEdit->setPlaceholderText("Введите имя файла...");

    // === Настройка autocomplete ===
    searchModel = new QStringListModel(this);
    searchCompleter = new QCompleter(searchModel, this);
    searchCompleter->setCompletionMode(QCompleter::PopupCompletion); // выпадающий список
    searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);        // без учета регистра
    searchCompleter->setWrapAround(false);

    auto* model = new QStringListModel(searchCompleter);
    searchCompleter->setModel(model);

    searchCompleter->setWidget(ui->searchEdit);

    auto* popup = searchCompleter->popup();
    popup->setMinimumSize(150, 80);
    popup->setFont(QFont("JetBrains Mono", 10, QFont::Normal));

    popup->installEventFilter(this);
    ui->searchEdit->installEventFilter(this);

    connect(searchCompleter,
            QOverload<const QString &>::of(&QCompleter::activated),
            this,
            &MainWindow::insertSearchCompletion);


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
    ui->fileTree->expandAll();
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
        explorer.addFile(currentPath, virtName.toStdString(), physicalPath.toStdString());

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
    if (!searchCompleter) return;

    std::string prefixStd = arg1.toStdString();

    if (prefixStd.empty() || prefixStd.length() < 3) {
        // мало символов — просто прячем popup
        auto* model = qobject_cast<QStringListModel*>(searchCompleter->model());
        if (model) model->setStringList(QStringList());
        searchCompleter->popup()->hide();
        return;
    }

    // Получаем подсказки из VFSExplorer
    std::vector<std::string> suggestions = explorer.getSuggestions(prefixStd);

    QStringList list;
    list.reserve(static_cast<int>(suggestions.size()));
    for (const auto& s : suggestions) {
        list << QString::fromStdString(s);
    }

    auto* model = qobject_cast<QStringListModel*>(searchCompleter->model());
    if (!model) return;

    if (list.isEmpty()) {
        model->setStringList(QStringList());
        searchCompleter->popup()->hide();
        return;
    }

    model->setStringList(list);

    // ВАЖНО: префикс = то, что пользователь ввёл, а не текущая выбранная строка
    searchCompleter->setCompletionPrefix(arg1);

    // Показываем popup — для QLineEdit можно просто complete() без rect
    searchCompleter->complete();
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
    dlg.exec();                      // просто ждём закрытия

    if (!dlg.isModified()) {         // ничего не меняли — выходим
        return;
    }

    // что-то изменили (rename/delete) — перерисовываем и разворачиваем
    refreshTree();
}



// Контекстное меню для дерева файлов.
// Вызывается, когда пользователь нажимает ПКМ по QTreeWidget
// (сигнал customContextMenuRequested).
void MainWindow::showContextMenu(const QPoint &pos) {
    // Создаем контекстное меню. tr() — для поддержки переводов.
    QMenu contextMenu(tr("Context menu"), this);

    QAction actionRename("Переименовать", this);
    connect(&actionRename, &QAction::triggered, this, &MainWindow::on_btnRename_clicked);
    contextMenu.addAction(&actionRename);

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

void MainWindow::on_btnRename_clicked()
{
    QTreeWidgetItem* item = ui->fileTree->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Переименование",
                             "Выберите объект в дереве.");
        return;
    }

    QVariant v = item->data(0, Qt::UserRole);
    auto* node = static_cast<VFSNode*>(v.value<void*>());
    if (!node) return;

    bool ok = false;
    QString newName = QInputDialog::getText(
        this,
        "Переименование",
        "Новое имя:",
        QLineEdit::Normal,
        QString::fromStdString(node->getName()),
        &ok
        );
    if (!ok || newName.isEmpty()) return;

    try {
        // такой же вызов, как в диалоге
        explorer.renameNode(node, newName.toStdString());

        refreshTree();
        ui->fileTree->expandAll();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Ошибка", e.what());
    }
}

void MainWindow::on_btnCopyPath_clicked()
{
    QTreeWidgetItem* item = ui->fileTree->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Копирование пути",
                             "Выберите файл в дереве.");
        return;
    }

    QVariant v = item->data(0, Qt::UserRole);
    auto* node = static_cast<VFSNode*>(v.value<void*>());
    if (!node) return;

    // Путь есть только у файла
    auto* file = dynamic_cast<VFSFile*>(node);
    if (!file) {
        QMessageBox::warning(this, "Копирование пути",
                             "У каталога нет физического пути.");
        return;
    }

    QString path = QString::fromStdString(file->getPhysicalPath());
    if (path.isEmpty()) {
        QMessageBox::warning(this, "Копирование пути",
                             "Физический путь пуст.");
        return;
    }

    QClipboard* cb = QGuiApplication::clipboard();
    cb->setText(path);

    // опционально — уведомление
    QMessageBox::information(this, "Копирование пути",
                             "Путь скопирован в буфер обмена:\n" + path);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->searchEdit &&
        event->type() == QEvent::KeyPress &&
        searchCompleter && searchCompleter->popup()->isVisible()) {

        auto* keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Tab ||
            keyEvent->key() == Qt::Key_Return ||
            keyEvent->key() == Qt::Key_Enter) {

            QAbstractItemView* popup = searchCompleter->popup();
            QModelIndex idx = popup->currentIndex();
            QString completion = idx.data(Qt::DisplayRole).toString();

            if (completion.isEmpty()) {
                completion = searchCompleter->currentCompletion();
            }

            if (!completion.isEmpty()) {
                insertSearchCompletion(completion);
            }

            return true; // событие съели
        }
    }

    if (obj == ui->fileTree->viewport()) {

        // DRAG ENTER
        if (event->type() == QEvent::DragEnter) {
            auto* de = static_cast<QDragEnterEvent*>(event);
            const QMimeData* mime = de->mimeData();

            // Внешние файлы (urls) или внутренний drag от самого дерева
            auto* src = de->source();
            if (mime->hasUrls()
                || src == ui->fileTree
                || src == ui->fileTree->viewport()) {
                de->acceptProposedAction();
                return true;
            }
        }

        // DRAG MOVE
        if (event->type() == QEvent::DragMove) {
            auto* dm = static_cast<QDragMoveEvent*>(event);
            const QMimeData* mime = dm->mimeData();

            auto* src = dm->source();
            if (mime->hasUrls()
                || src == ui->fileTree
                || src == ui->fileTree->viewport()) {
                dm->acceptProposedAction();
                return true;
            }
        }

        // DROP
        if (event->type() == QEvent::Drop) {
            auto* drop = static_cast<QDropEvent*>(event);
            const QMimeData* mime = drop->mimeData();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QPoint vpPos = drop->position().toPoint();
#else
            QPoint vpPos = drop->pos();
#endif
            QTreeWidgetItem* targetItem = ui->fileTree->itemAt(vpPos);

            // ---- 4.1 Внутренний d&d (из нашего дерева) ----
            if (drop->source() == ui->fileTree) {
                // Перетаскивали что-то из дерева
                QTreeWidgetItem* draggedItem = ui->fileTree->currentItem();
                if (!draggedItem) {
                    return false;
                }

                // Узел, который переносим
                QVariant dv = draggedItem->data(0, Qt::UserRole);
                auto* draggedNode = static_cast<VFSNode*>(dv.value<void*>());
                if (!draggedNode) {
                    return false;
                }

                // Куда переносим — директория назначения
                VFSDirectory* targetDirNode = getTargetDirForItem(targetItem);
                if (!targetDirNode) {
                    return false;
                }

                // Можно защититься от перемещения корня
                if (draggedNode == explorer.getRoot()) {
                    QMessageBox::warning(this, "Перемещение",
                                         "Нельзя перемещать корневой каталог.");
                    return true;
                }

                try {
                    explorer.moveNode(draggedNode, targetDirNode);
                    refreshTree();
                    ui->fileTree->expandAll();
                } catch (const std::exception& e) {
                    QMessageBox::critical(this, "Ошибка перемещения", e.what());
                    // На всякий случай вернуть дерево в консистентное состояние
                    refreshTree();
                    ui->fileTree->expandAll();
                }

                drop->acceptProposedAction();
                return true;
            }

            // ---- 4.2 Внешний d&d (из проводника) ----
            if (mime->hasUrls()) {
                VFSDirectory* targetDirNode = getTargetDirForItem(targetItem);
                // Для addFile у тебя, скорее всего, нужен путь, а не VFSDirectory*,
                // тут можно использовать explorer.findVirtualPath(targetDirNode),
                // если такой метод есть.
                std::string targetDirPath = explorer.findVirtualPath(targetDirNode);

                for (const QUrl& url : mime->urls()) {
                    if (!url.isLocalFile()) continue;

                    QString physicalPath = url.toLocalFile();
                    if (physicalPath.isEmpty()) continue;

                    QFileInfo fi(physicalPath);
                    QString virtName = fi.fileName();

                    try {
                        explorer.addFile(targetDirPath,
                                            virtName.toStdString(),
                                            physicalPath.toStdString());
                    } catch (const std::exception& e) {
                        QMessageBox::critical(this, "Ошибка при импорте файла", e.what());
                    }
                }

                refreshTree();
                ui->fileTree->expandAll();

                drop->acceptProposedAction();
                return true;
            }

            // Ни внутренний, ни внешние файлы — игнорируем
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

VFSDirectory* MainWindow::getTargetDirForItem(QTreeWidgetItem* item)
{
    if (!item) {
        // Бросили "в пустоту" → корень
        return explorer.getRoot();
    }

    QVariant v = item->data(0, Qt::UserRole);
    auto* node = static_cast<VFSNode*>(v.value<void*>());
    if (!node) {
        return explorer.getRoot();
    }

    if (node->isDirectory()) {
        // Кидаем прямо в эту директорию
        return static_cast<VFSDirectory*>(node);
    }

    // Если это файл — берём его родительский item (директорию)
    QTreeWidgetItem* parentItem = item->parent();
    if (!parentItem) {
        return explorer.getRoot();
    }

    QVariant pv = parentItem->data(0, Qt::UserRole);
    auto* parentNode = static_cast<VFSNode*>(pv.value<void*>());
    if (!parentNode || !parentNode->isDirectory()) {
        return explorer.getRoot();
    }

    return static_cast<VFSDirectory*>(parentNode);
}

void MainWindow::insertSearchCompletion(const QString& completion)
{
    if (!searchCompleter || searchCompleter->widget() != ui->searchEdit) {
        return;
    }

    if (completion.isEmpty())
        return;

    ui->searchEdit->setText(completion);
    ui->searchEdit->setCursorPosition(completion.length());
    searchCompleter->popup()->hide();
}

void MainWindow::on_btnCreateFile_clicked()
{
    // В какой виртуальной директории создаём
    std::string currentPath = getCurrentPath();

    CreateFileDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return; // нажали Cancel/закрыли окно
    }

    QString vName        = dlg.virtualName();
    QString physicalPath = dlg.physicalPath();

    if (vName.isEmpty() || physicalPath.isEmpty()) {
        QMessageBox::warning(this,
                             "Создание файла",
                             "Имя файла и путь к файлу не должны быть пустыми.");
        return;
    }

    try {
        explorer.createFile(
            currentPath,
            vName.toStdString(),
            physicalPath.toStdString()
            );

        refreshTree();
        ui->fileTree->expandAll();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Ошибка создания файла", e.what());
    }
}

