QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    src/main/stream-dir/utils/FileBuffer.cpp \
    src/test/main/TmpFile.cpp

HEADERS += \
    mainwindow.h \
    src/StreamLib.h \
    src/main/spellchecker/RadixTree.h \
    src/main/spellchecker/SpellChecker.h \
    src/main/spellchecker/SpellHelper.h \
    src/main/stream-dir/core/ReadStreamBuffer.h \
    src/main/stream-dir/core/ReadWriteStreamBuffer.h \
    src/main/stream-dir/core/TextFileReader.h \
    src/main/stream-dir/core/TextFileReaderWriter.h \
    src/main/stream-dir/core/TextFileWriter.h \
    src/main/stream-dir/core/WriteStreamBuffer.h \
    src/main/stream-dir/io/file/FileReadStreamBuffer.h \
    src/main/stream-dir/io/file/FileReadWriteStreamBuffer.h \
    src/main/stream-dir/io/file/FileWriteStreamBuffer.h \
    src/main/stream-dir/io/sequence/SequenceReadStreamBuffer.h \
    src/main/stream-dir/io/sequence/SequenceWriteStreamBuffer.h \
    src/main/stream-dir/io/serializers/ByteSerializers.h \
    src/main/stream-dir/streams/ReadOnlyStream.h \
    src/main/stream-dir/streams/ReadWriteStream.h \
    src/main/stream-dir/streams/WriteOnlyStream.h \
    src/main/stream-dir/utils/FileBuffer.h \
    src/test/main/ReadOnlyStreamTest.h \
    src/test/main/TestHelper.h \
    src/test/main/TmpFile.h \
    src/test/main/WriteOnlyStreamTest.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    TextEditorApp_ru_RU.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    src/resources/vocabulary.txt \
    src/test/resources/read_test.txt \
    src/test/resources/write_test.txt
