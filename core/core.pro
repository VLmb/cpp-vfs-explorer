QT -= gui
TEMPLATE = lib
CONFIG += staticlib
TARGET = core

HEADERS += \
    benchmark/BenchmarkService.h \
    domain/VFSDirectory.h \
    domain/VFSExplorer.h \
    domain/VFSFile.h \
    domain/VFSNode.h \
    model/VFSDirectory.h \
    model/VFSFile.h \
    search/FileHashMap.h \
    search/FileNameTrie.h \
    search/Trie.h \
    utils/PathUtils.h \
    utils/ScriptLoader.h

SUBDIRS += \
    resources/files/TextEditorApp.pro

DISTFILES += \
    resources/files/Application.java \
    resources/files/HangmanApp.java \
    resources/script.txt
