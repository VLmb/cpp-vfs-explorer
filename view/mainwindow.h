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

#include "../core/domain/VFSExplorer.h"
#include "../core/utils/ScriptLoader.h"
#include "../core/benchmark/BenchmarkService.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

    ~MainWindow();

private slots:
    void on_btnCreateFolder_clicked();

    void on_btnMountFile_clicked();

    void on_btnInitFS_clicked();

    void on_btnDelete_clicked();

    void on_btnSearchFast_clicked();

    void on_btnSearchSlow_clicked();

    void on_searchEdit_textChanged(const QString &arg1);

    void on_searchResultList_itemClicked(QListWidgetItem *item);

    void showContextMenu(const QPoint &pos);

    void on_btnExpandAll_clicked();

    void on_btnRename_clicked();

    void on_btnCopyPath_clicked();

    void on_fileTree_itemDoubleClicked(QTreeWidgetItem* item, int column);

    VFSDirectory* getTargetDirForItem(QTreeWidgetItem* item);

    void insertSearchCompletion(const QString& completion);

    void on_btnCreateFile_clicked();

    void on_btnRunBenchmark_clicked();

    // void onContextCopy();
    // void onContextCut();
    // void onContextPaste();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    Ui::MainWindow *ui;

    VFSExplorer explorer;

    void refreshTree();

    void addTreeItemsRecursive(VFSNode* node, QTreeWidgetItem* parentItem);
    void addSearchResultItem(VFSNode* node, const QString& tag = QString());
    void showNodeInfo(VFSNode* node);

    std::string getCurrentPath();

    QIcon dirIcon;
    QIcon fileIcon;

    QCompleter* searchCompleter = nullptr;
    QStringListModel* searchModel = nullptr;

    // enum class ClipboardMode {
    //     None,
    //     Copy,
    //     Cut
    // };

    // VFSNode*     m_clipboardNode = nullptr;
    // ClipboardMode m_clipboardMode = ClipboardMode::None;

};
