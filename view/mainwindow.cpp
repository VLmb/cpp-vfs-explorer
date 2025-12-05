#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QStyle>          // стандартные иконки
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
#include <QSpinBox>

#include "../core/domain/VFSFile.h"
#include "../core/benchmark/BenchmarkService.h"
#include "../core/domain/VFSDirectory.h"

// Буфер обмена для VFS (статические переменные для эмуляции полей класса)
static VFSNode* g_clipboardNode = nullptr;
static bool g_isCutOperation = false;

// диалог параметров для бенчмарка
class BenchmarkParamsDialog : public QDialog {
public:
    explicit BenchmarkParamsDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Параметры сравнения поисков");

        filesSpin = new QSpinBox(this);
        itersSpin = new QSpinBox(this);

        filesSpin->setRange(1, 1'000'000);
        itersSpin->setRange(1, 1'000'000);

        filesSpin->setValue(1000);
        itersSpin->setValue(100);

        auto* form = new QFormLayout;
        form->addRow("Количество файлов:",   filesSpin);
        form->addRow("Количество итераций:", itersSpin);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            Qt::Horizontal,
            this
            );

        auto* defaultButton = new QPushButton("По умолчанию", this);

        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        connect(defaultButton, &QPushButton::clicked, this, [this]() {
            m_useDefault = true;
            accept();
        });

        auto* btnLayout = new QHBoxLayout;
        btnLayout->addWidget(defaultButton);
        btnLayout->addStretch();
        btnLayout->addWidget(buttons);

        auto* mainLayout = new QVBoxLayout;
        mainLayout->addLayout(form);
        mainLayout->addLayout(btnLayout);

        setLayout(mainLayout);
    }

    int fileCount() const       { return filesSpin->value(); }
    int iterationCount() const  { return itersSpin->value(); }
    bool useDefault() const     { return m_useDefault; }

private:
    QSpinBox* filesSpin  = nullptr;
    QSpinBox* itersSpin  = nullptr;
    bool m_useDefault    = false;
};

// диалог свойств узла vfs
class NodeInfoDialog : public QDialog {
public:
    NodeInfoDialog(VFSNode* node, VFSExplorer& explorer, QWidget* parent = nullptr)
        : QDialog(parent)
        , m_node(node)
        , m_explorer(explorer)
    {
        setWindowTitle("Свойства объекта");

        nameValue = new QLabel(QString::fromStdString(node->getName()), this);
        typeValue = new QLabel(node->isDirectory() ? "Каталог" : "Файл", this);

        QDateTime dt = QDateTime::fromSecsSinceEpoch(node->getCreationTime());
        createdValue = new QLabel(dt.toString("dd.MM.yyyy hh:mm:ss"), this);

        std::string physicalPathStr;
        if (!node->isDirectory()) {
            if (auto* file = dynamic_cast<VFSFile*>(node)) {
                physicalPathStr = file->getPhysicalPath();
            }
        }
        physicalPathValue = new QLabel(QString::fromStdString(physicalPathStr), this);

        auto* form = new QFormLayout;
        form->addRow("Имя:", nameValue);
        form->addRow("Тип:", typeValue);
        if (!node->isDirectory()) {
            form->addRow("Реальный путь:", physicalPathValue);
        }
        form->addRow("Создан:", createdValue);

        auto* openButton   = new QPushButton("Открыть", this);
        auto* renameButton = new QPushButton("Переименовать", this);
        auto* deleteButton = new QPushButton("Удалить", this);
        auto* closeButton  = new QPushButton("Закрыть", this);

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

        // открыть файл внешней программой
        connect(openButton, &QPushButton::clicked, this, [physicalPathStr]() {
            if (physicalPathStr.empty()) return;
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(QString::fromStdString(physicalPathStr)));
        });

        // переименование
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

        // удаление
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
                accept();
            } catch (const std::exception& e) {
                QMessageBox::critical(this, "Ошибка", e.what());
            }
        });

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

// диалог создания файла (вирт. имя + физ. путь)
class CreateFileDialog : public QDialog {
public:
    explicit CreateFileDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Создать файл");

        m_virtualNameEdit  = new QLineEdit(this);
        m_physicalPathEdit = new QLineEdit(this);

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

// конструктор главного окна
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->fileTree->setDragEnabled(true);
    ui->fileTree->setAcceptDrops(true);
    ui->fileTree->setDropIndicatorShown(true);
    ui->fileTree->setDragDropMode(QAbstractItemView::DragDrop);
    ui->fileTree->viewport()->setAcceptDrops(true);
    ui->fileTree->viewport()->installEventFilter(this);

    ui->searchEdit->setPlaceholderText("Введите имя файла...");

    // настройка autocomplete
    searchModel = new QStringListModel(this);
    searchCompleter = new QCompleter(searchModel, this);
    searchCompleter->setCompletionMode(QCompleter::PopupCompletion);
    searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
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

    dirIcon = style()->standardIcon(QStyle::SP_DirIcon);
    fileIcon = style()->standardIcon(QStyle::SP_FileIcon);

    ui->fileTree->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->fileTree, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::showContextMenu);

    refreshTree();
}

MainWindow::~MainWindow() {
    delete ui;
}

static QString formatSize(std::size_t bytes)
{
    const double KB = 1024.0;
    const double MB = 1024.0 * 1024.0;

    if (bytes < 1024) {
        return QString::number(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        double kbValue = bytes / KB;
        return QString::number(kbValue, 'f', 1) + " KB";
    } else {
        double mbValue = bytes / MB;
        return QString::number(mbValue, 'f', 1) + " MB";
    }
}

// полная перерисовка дерева
void MainWindow::refreshTree() {
    ui->fileTree->clear();

    VFSDirectory* root = explorer.getRoot();

    QTreeWidgetItem* rootItem = new QTreeWidgetItem(ui->fileTree);
    rootItem->setText(0, "/");
    rootItem->setIcon(0, dirIcon);

    rootItem->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(root)));

    addTreeItemsRecursive(root, rootItem);

    rootItem->setExpanded(true);
    ui->fileTree->expandAll();
}

// рекурсивное заполнение дерева на основе vfs
void MainWindow::addTreeItemsRecursive(VFSNode* node, QTreeWidgetItem* parentItem) {
    if (!node->isDirectory()) return;

    VFSDirectory* dir = static_cast<VFSDirectory*>(node);

    for (const auto& child : dir->getChildren()) {
        QTreeWidgetItem* item = new QTreeWidgetItem(parentItem);

        item->setText(0, QString::fromStdString(child->getName()));
        item->setText(1, formatSize(child->getSize()));

        // привязываем указатель на vfs-узел
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

    item->setData(Qt::UserRole,
                  QVariant::fromValue(static_cast<void*>(node)));

    // простой tooltip, пока путь заглушка
    QString tooltip = QString("Имя: %1\nПуть: %2")
                          .arg(QString::fromStdString(node->getName()))
                          .arg(QString::fromStdString("..."));
    item->setToolTip(tooltip);
}

// восстановление полного виртуального пути из выбранного item
std::string MainWindow::getCurrentPath() {
    QTreeWidgetItem* item = ui->fileTree->currentItem();

    if (!item) return "/";

    QStringList parts;

    while (item) {
        QString text = item->text(0);
        if (text == "/") break;

        parts.prepend(text);
        item = item->parent();
    }

    if (parts.isEmpty()) return "/";

    return "/" + parts.join("/").toStdString();
}

// создание папки
void MainWindow::on_btnCreateFolder_clicked() {
    std::string currentPath = getCurrentPath();

    bool ok;
    QString text = QInputDialog::getText(this, "Новая папка",
                                         "Имя папки:", QLineEdit::Normal,
                                         "NewFolder", &ok);
    if (ok && !text.isEmpty()) {
        try {
            explorer.createDirectory(currentPath, text.toStdString());
            refreshTree();
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Ошибка", e.what());
        }
    }
}

void MainWindow::on_btnInitFS_clicked()
{
    ScriptLoader::load(explorer);

    refreshTree();

    ui->btnInitFS->setEnabled(false);

    QMessageBox::information(this, "Инициализация", "Файловая система инициализирована.");
}

// добавление реального файла в vfs
void MainWindow::on_btnMountFile_clicked() {
    std::string currentPath = getCurrentPath();

    QString physicalPath = QFileDialog::getOpenFileName(this, "Выберите файл", QDir::homePath());

    if (physicalPath.isEmpty()) return;

    QFileInfo fileInfo(physicalPath);
    QString virtName = fileInfo.fileName();

    try {
        explorer.addFile(currentPath, virtName.toStdString(), physicalPath.toStdString());
        refreshTree();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Ошибка", e.what());
    }
}

// удаление выделенного узла
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

// обработка изменения текста поиска (автодополнение)
void MainWindow::on_searchEdit_textChanged(const QString &arg1) {
    if (!searchCompleter) return;

    std::string prefixStd = arg1.toStdString();

    if (prefixStd.empty() || prefixStd.length() < 3) {
        auto* model = qobject_cast<QStringListModel*>(searchCompleter->model());
        if (model) model->setStringList(QStringList());
        searchCompleter->popup()->hide();
        return;
    }

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
    searchCompleter->setCompletionPrefix(arg1);
    searchCompleter->complete();
}

// быстрый поиск по индексу
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

// медленный поиск обходом дерева
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
    dlg.exec();

    if (!dlg.isModified()) {
        return;
    }

    refreshTree();
}

// контекстное меню по правому клику по дереву
void MainWindow::showContextMenu(const QPoint &pos) {
    QMenu contextMenu(tr("Context menu"), this);

    // --- Стандартные действия ---
    QAction actionRename("Переименовать", this);
    connect(&actionRename, &QAction::triggered, this, &MainWindow::on_btnRename_clicked);
    contextMenu.addAction(&actionRename);

    QAction actionCreate("Создать папку", this);
    connect(&actionCreate, &QAction::triggered, this, &MainWindow::on_btnCreateFolder_clicked);
    contextMenu.addAction(&actionCreate);

    QAction actionMount("Добавить файл", this);
    connect(&actionMount, &QAction::triggered, this, &MainWindow::on_btnMountFile_clicked);
    contextMenu.addAction(&actionMount);

    QAction actionDel("Удалить", this);
    connect(&actionDel, &QAction::triggered, this, &MainWindow::on_btnDelete_clicked);
    contextMenu.addAction(&actionDel);

    contextMenu.addSeparator();

    // --- Действия буфера обмена ---

    // КОПИРОВАТЬ
    QAction* actionCopy = new QAction("Копировать", this);
    connect(actionCopy, &QAction::triggered, this, [this]() {
        QTreeWidgetItem* item = ui->fileTree->currentItem();
        if (!item) return;
        QVariant v = item->data(0, Qt::UserRole);
        g_clipboardNode = static_cast<VFSNode*>(v.value<void*>());
        g_isCutOperation = false;
    });
    contextMenu.addAction(actionCopy);

    // ВЫРЕЗАТЬ
    QAction* actionCut = new QAction("Вырезать", this);
    connect(actionCut, &QAction::triggered, this, [this]() {
        QTreeWidgetItem* item = ui->fileTree->currentItem();
        if (!item) return;
        QVariant v = item->data(0, Qt::UserRole);
        g_clipboardNode = static_cast<VFSNode*>(v.value<void*>());
        g_isCutOperation = true;
    });
    contextMenu.addAction(actionCut);

    // ВСТАВИТЬ
    QAction* actionPaste = new QAction("Вставить", this);

    // Делаем кнопку активной только если что-то есть в буфере
    if (!g_clipboardNode) {
        actionPaste->setEnabled(false);
    }

    connect(actionPaste, &QAction::triggered, this, [this]() {
        if (!g_clipboardNode) return;

        QTreeWidgetItem* targetItem = ui->fileTree->currentItem();
        VFSDirectory* destDir = getTargetDirForItem(targetItem);
        if (!destDir) return;

        std::string destPath = explorer.findVirtualPath(destDir);
        std::string originalName = g_clipboardNode->getName();

        // 1. ПРОВЕРКА НА РЕКУРСИЮ (Папка в себя или в подпапку)
        // Если мы пытаемся вставить папку, нужно убедиться, что destPath не начинается с пути этой папки.
        if (g_clipboardNode->isDirectory()) {
            std::string srcPath = explorer.findVirtualPath(g_clipboardNode);
            // Добавляем слэш, чтобы избежать ложных срабатываний (напр. /Folder и /Folder2)
            std::string srcCheck = srcPath + "/";
            std::string dstCheck = destPath + "/";

            // Если путь назначения содержит путь источника или они равны
            if (destDir == g_clipboardNode || dstCheck.find(srcCheck) == 0) {
                QMessageBox::warning(this, "Ошибка", "Невозможно переместить папку внутрь самой себя или своего подкаталога.");
                return;
            }
        }

        // 2. ПРОВЕРКА НА КОНФЛИКТ ИМЕН
        bool exists = (destDir->getChild(originalName) != nullptr);

        if (exists) {
            // --- Если файл существует, спрашиваем пользователя ---
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("Конфликт имен");
            msgBox.setText("Объект с именем \"" + QString::fromStdString(originalName) + "\" уже существует.");
            msgBox.setInformativeText("Выберите действие:");

            QAbstractButton* btnReplace = msgBox.addButton("Заменить", QMessageBox::YesRole);
            QAbstractButton* btnJustCopy = msgBox.addButton("Создать копию", QMessageBox::NoRole);
            QAbstractButton* btnRename  = msgBox.addButton("Переименовать", QMessageBox::NoRole);
            msgBox.addButton("Отмена", QMessageBox::RejectRole);

            msgBox.exec();

            try {
                if (msgBox.clickedButton() == btnReplace) {
                    // ЗАМЕНИТЬ (replace = true)
                    if (g_isCutOperation) {
                        explorer.cutNode(g_clipboardNode, destPath, true);
                        g_clipboardNode = nullptr;
                    } else {
                        explorer.copyNode(g_clipboardNode, destPath, true);
                    }
                }
                else if (msgBox.clickedButton() == btnRename) {
                    // ПЕРЕИМЕНОВАТЬ (ввод нового имени)
                    bool ok = false;
                    QString newName = QInputDialog::getText(
                        this,
                        "Новое имя",
                        "Введите имя:",
                        QLineEdit::Normal,
                        "",
                        &ok
                        );

                    if (ok && !newName.isEmpty()) {
                        if (g_isCutOperation) {
                            explorer.cutNode(g_clipboardNode, destPath, false, newName.toStdString());
                            g_clipboardNode = nullptr;
                        } else {
                            explorer.copyNode(g_clipboardNode, destPath, false, newName.toStdString());
                        }
                    }
                }
                else if (msgBox.clickedButton() == btnJustCopy) {
                    // СОЗДАТЬ КОПИЮ (replace = false, newName = "")
                    // Backend сам создаст имя_copy1
                    if (g_isCutOperation) {
                        explorer.cutNode(g_clipboardNode, destPath, false);
                        g_clipboardNode = nullptr;
                    } else {
                        explorer.copyNode(g_clipboardNode, destPath, false);
                    }
                }
                // Если нажали Отмена — ничего не делаем
            } catch (const std::exception& e) {
                QMessageBox::critical(this, "Ошибка операции", e.what());
            }

        } else {
            // --- ЭТОТ БЛОК ОТСУТСТВОВАЛ ИЛИ БЫЛ НЕ ТАМ ---
            // Если конфликта нет, просто выполняем вставку
            try {
                if (g_isCutOperation) {
                    explorer.cutNode(g_clipboardNode, destPath, false);
                    g_clipboardNode = nullptr;
                } else {
                    explorer.copyNode(g_clipboardNode, destPath, false);
                }
            } catch (const std::exception& e) {
                QMessageBox::critical(this, "Ошибка операции", e.what());
            }
        }

        refreshTree();
        ui->fileTree->expandAll();
    });
    contextMenu.addAction(actionPaste);

    contextMenu.exec(ui->fileTree->mapToGlobal(pos));
}

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

    QMessageBox::information(this, "Копирование пути",
                             "Путь скопирован в буфер обмена:\n" + path);
}

// тут фильтруем события для поиска и d&d по дереву
bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    // автодополнение по enter/tab
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

            return true;
        }
    }

    if (obj == ui->fileTree->viewport()) {

        // drag enter
        if (event->type() == QEvent::DragEnter) {
            auto* de = static_cast<QDragEnterEvent*>(event);
            const QMimeData* mime = de->mimeData();

            auto* src = de->source();
            if (mime->hasUrls()
                || src == ui->fileTree
                || src == ui->fileTree->viewport()) {
                de->acceptProposedAction();
                return true;
            }
        }

        // drag move
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

        // drop
        if (event->type() == QEvent::Drop) {
            auto* drop = static_cast<QDropEvent*>(event);
            const QMimeData* mime = drop->mimeData();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QPoint vpPos = drop->position().toPoint();
#else
            QPoint vpPos = drop->pos();
#endif
            QTreeWidgetItem* targetItem = ui->fileTree->itemAt(vpPos);

            // внутренний d&d по дереву
            if (drop->source() == ui->fileTree) {
                QTreeWidgetItem* draggedItem = ui->fileTree->currentItem();
                if (!draggedItem) {
                    return false;
                }

                QVariant dv = draggedItem->data(0, Qt::UserRole);
                auto* draggedNode = static_cast<VFSNode*>(dv.value<void*>());
                if (!draggedNode) {
                    return false;
                }

                VFSDirectory* targetDirNode = getTargetDirForItem(targetItem);
                if (!targetDirNode) {
                    return false;
                }

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
                    refreshTree();
                    ui->fileTree->expandAll();
                }

                drop->acceptProposedAction();
                return true;
            }

            // внешний d&d из проводника
            if (mime->hasUrls()) {
                VFSDirectory* targetDirNode = getTargetDirForItem(targetItem);
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
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

// определяем, в какую директорию кидать d&d
VFSDirectory* MainWindow::getTargetDirForItem(QTreeWidgetItem* item)
{
    if (!item) {
        return explorer.getRoot();
    }

    QVariant v = item->data(0, Qt::UserRole);
    auto* node = static_cast<VFSNode*>(v.value<void*>());
    if (!node) {
        return explorer.getRoot();
    }

    if (node->isDirectory()) {
        return static_cast<VFSDirectory*>(node);
    }

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

// вставка выбранного completion в строку поиска
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
    std::string currentPath = getCurrentPath();

    CreateFileDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
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

// запуск бенчмарка двух поисков
void MainWindow::on_btnRunBenchmark_clicked()
{
    BenchmarkParamsDialog dlg(this);

    int fileCount = 1000;
    int iterations = 100;

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    if (!dlg.useDefault()) {
        fileCount  = dlg.fileCount();
        iterations = dlg.iterationCount();
    }

    try {
        BenchmarkResult res = BenchmarkService::run(explorer, fileCount, iterations);
        refreshTree();

        long long tTraversal = res.searchByTraversalTime;
        long long tIndex     = res.searchByIndexTime;
        long long diff       = tTraversal - tIndex;

        QString msg = QString(
                          "Время поиска обходом дерева:  %1 ns   \n"
                          "Время поиска по индексу:      %2 ns   \n"
                          "Разница:                      %3 ns   ")
                          .arg(tTraversal)
                          .arg(tIndex)
                          .arg(diff);

        QMessageBox::information(this, "Результат", msg);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Ошибка бенчмарка", e.what());
    }
}
