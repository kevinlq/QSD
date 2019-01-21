/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "tst_blackbox.h"

#include "../shared.h"

#include <api/languageinfo.h>
#include <tools/hostosinfo.h>
#include <tools/installoptions.h>
#include <tools/profile.h>
#include <tools/qttools.h>
#include <tools/shellutils.h>
#include <tools/stlutils.h>
#include <tools/version.h>

#include <QtCore/qdebug.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qlocale.h>
#include <QtCore/qregexp.h>
#include <QtCore/qtemporarydir.h>
#include <QtCore/qtemporaryfile.h>

#include <functional>
#include <regex>
#include <utility>

#define WAIT_FOR_NEW_TIMESTAMP() waitForNewTimestamp(testDataDir)

using qbs::Internal::HostOsInfo;
using qbs::Profile;

class MacosTarHealer {
public:
    MacosTarHealer() {
        if (HostOsInfo::hostOs() == HostOsInfo::HostOsMacos) {
            // work around absurd tar behavior on macOS
            qputenv("COPY_EXTENDED_ATTRIBUTES_DISABLE", "true");
            qputenv("COPYFILE_DISABLE", "true");
        }
    }

    ~MacosTarHealer() {
        if (HostOsInfo::hostOs() == HostOsInfo::HostOsMacos) {
            qunsetenv("COPY_EXTENDED_ATTRIBUTES_DISABLE");
            qunsetenv("COPYFILE_DISABLE");
        }
    }
};

QMap<QString, QString> TestBlackbox::findCli(int *status)
{
    QTemporaryDir temp;
    QDir::setCurrent(testDataDir + "/find");
    QbsRunParameters params = QStringList() << "-f" << "find-cli.qbs";
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-cli") + "/cli.json");
    if (!file.open(QIODevice::ReadOnly))
        return QMap<QString, QString> { };
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return QMap<QString, QString> {
        {"path", QDir::fromNativeSeparators(tools["path"].toString())},
    };
}

QMap<QString, QString> TestBlackbox::findNodejs(int *status)
{
    QTemporaryDir temp;
    QDir::setCurrent(testDataDir + "/find");
    QbsRunParameters params = QStringList() << "-f" << "find-nodejs.qbs";
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-nodejs") + "/nodejs.json");
    if (!file.open(QIODevice::ReadOnly))
        return QMap<QString, QString> { };
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return QMap<QString, QString> {
        {"node", QDir::fromNativeSeparators(tools["node"].toString())}
    };
}

QMap<QString, QString> TestBlackbox::findTypeScript(int *status)
{
    QTemporaryDir temp;
    QDir::setCurrent(testDataDir + "/find");
    QbsRunParameters params = QStringList() << "-f" << "find-typescript.qbs";
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-typescript") + "/typescript.json");
    if (!file.open(QIODevice::ReadOnly))
        return QMap<QString, QString> { };
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return QMap<QString, QString> {
        {"tsc", QDir::fromNativeSeparators(tools["tsc"].toString())}
    };
}

QString TestBlackbox::findArchiver(const QString &fileName, int *status)
{
    if (fileName == "jar")
        return findJdkTools(status)[fileName];

    QString binary = findExecutable(QStringList(fileName));
    if (binary.isEmpty()) {
        const SettingsPtr s = settings();
        Profile p(profileName(), s.get());
        binary = findExecutable(p.value("archiver.command").toStringList());
    }
    return binary;
}

void TestBlackbox::sevenZip()
{
    QDir::setCurrent(testDataDir + "/archiver");
    QString binary = findArchiver("7z");
    if (binary.isEmpty())
        QSKIP("7zip not found");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "modules.archiver.type:7zip")), 0);
    const QString outputFile = relativeProductBuildDir("archivable") + "/archivable.7z";
    QVERIFY2(regularFileExists(outputFile), qPrintable(outputFile));
    QProcess listContents;
    listContents.start(binary, QStringList() << "l" << outputFile);
    QVERIFY2(listContents.waitForStarted(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.waitForFinished(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.exitCode() == 0, listContents.readAllStandardError().constData());
    const QByteArray output = listContents.readAllStandardOutput();
    QVERIFY2(output.contains("2 files"), output.constData());
    QVERIFY2(output.contains("test.txt"), output.constData());
    QVERIFY2(output.contains("archivable.qbs"), output.constData());
}

void TestBlackbox::sourceArtifactInInputsFromDependencies()
{
    QDir::setCurrent(testDataDir + "/source-artifact-in-inputs-from-dependencies");
    QCOMPARE(runQbs(), 0);
    QFile outFile(relativeProductBuildDir("p") + "/output.txt");
    QVERIFY2(outFile.exists(), qPrintable(outFile.fileName()));
    QVERIFY2(outFile.open(QIODevice::ReadOnly), qPrintable(outFile.errorString()));
    const QByteArrayList output = outFile.readAll().trimmed().split('\n');
    QCOMPARE(output.size(), 2);
    bool header1Found = false;
    bool header2Found = false;
    for (const QByteArray &line : output) {
        const QByteArray &path = line.trimmed();
        if (path == "include1/header.h")
            header1Found = true;
        else if (path == "include2/header.h")
            header2Found = true;
    }
    QVERIFY(header1Found);
    QVERIFY(header2Found);
}

void TestBlackbox::staticLibWithoutSources()
{
    QDir::setCurrent(testDataDir + "/static-lib-without-sources");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::suspiciousCalls()
{
    const QString projectDir = testDataDir + "/suspicious-calls";
    QDir::setCurrent(projectDir);
    rmDirR(relativeBuildDir());
    QFETCH(QString, projectFile);
    QbsRunParameters params(QStringList() << "-f" << projectFile);
    QCOMPARE(runQbs(params), 0);
    QFETCH(QByteArray, expectedWarning);
    QVERIFY2(m_qbsStderr.contains(expectedWarning), m_qbsStderr.constData());
}

void TestBlackbox::suspiciousCalls_data()
{
    QTest::addColumn<QString>("projectFile");
    QTest::addColumn<QByteArray>("expectedWarning");
    QTest::newRow("File.copy() in Probe") << "copy-probe.qbs" << QByteArray();
    QTest::newRow("File.copy() during evaluation") << "copy-eval.qbs" << QByteArray("File.copy()");
    QTest::newRow("File.copy() in prepare script")
            << "copy-prepare.qbs" << QByteArray("File.copy()");
    QTest::newRow("File.copy() in command") << "copy-command.qbs" << QByteArray();
    QTest::newRow("File.directoryEntries() in Probe") << "direntries-probe.qbs" << QByteArray();
    QTest::newRow("File.directoryEntries() during evaluation")
            << "direntries-eval.qbs" << QByteArray("File.directoryEntries()");
    QTest::newRow("File.directoryEntries() in prepare script")
            << "direntries-prepare.qbs" << QByteArray();
    QTest::newRow("File.directoryEntries() in command") << "direntries-command.qbs" << QByteArray();
}

void TestBlackbox::systemIncludePaths()
{
    const QString projectDir = testDataDir + "/system-include-paths";
    QDir::setCurrent(projectDir);
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::tar()
{
    if (HostOsInfo::hostOs() == HostOsInfo::HostOsWindows)
        QSKIP("Beware of the msys tar");
    MacosTarHealer tarHealer;
    QDir::setCurrent(testDataDir + "/archiver");
    rmDirR(relativeBuildDir());
    QString binary = findArchiver("tar");
    if (binary.isEmpty())
        QSKIP("tar not found");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "modules.archiver.type:tar")), 0);
    const QString outputFile = relativeProductBuildDir("archivable") + "/archivable.tar.gz";
    QVERIFY2(regularFileExists(outputFile), qPrintable(outputFile));
    QProcess listContents;
    listContents.start(binary, QStringList() << "tf" << outputFile);
    QVERIFY2(listContents.waitForStarted(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.waitForFinished(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.exitCode() == 0, listContents.readAllStandardError().constData());
    QFile listFile("list.txt");
    QVERIFY2(listFile.open(QIODevice::ReadOnly), qPrintable(listFile.errorString()));
    QCOMPARE(listContents.readAllStandardOutput(), listFile.readAll());
}

static QStringList sortedFileList(const QByteArray &ba)
{
    auto list = QString::fromUtf8(ba).split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
    std::sort(list.begin(), list.end());
    return list;
}

void TestBlackbox::zip()
{
    QFETCH(QString, binaryName);
    int status = 0;
    const QString binary = findArchiver(binaryName, &status);
    QCOMPARE(status, 0);
    if (binary.isEmpty())
        QSKIP("zip tool not found");

    QDir::setCurrent(testDataDir + "/archiver");
    rmDirR(relativeBuildDir());
    QbsRunParameters params(QStringList()
                            << "modules.archiver.type:zip" << "modules.archiver.command:" + binary);
    QCOMPARE(runQbs(params), 0);
    const QString outputFile = relativeProductBuildDir("archivable") + "/archivable.zip";
    QVERIFY2(regularFileExists(outputFile), qPrintable(outputFile));
    QProcess listContents;
    if (binaryName == "zip") {
        // zipinfo is part of Info-Zip
        listContents.start("zipinfo", QStringList() << "-1" << outputFile);
    } else {
        listContents.start(binary, QStringList() << "tf" << outputFile);
    }
    QVERIFY2(listContents.waitForStarted(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.waitForFinished(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.exitCode() == 0, listContents.readAllStandardError().constData());
    QFile listFile("list.txt");
    QVERIFY2(listFile.open(QIODevice::ReadOnly), qPrintable(listFile.errorString()));
    QCOMPARE(sortedFileList(listContents.readAllStandardOutput()),
             sortedFileList(listFile.readAll()));

    // Make sure the module is still loaded when the java/jar fallback is not available
    params.command = "resolve";
    params.arguments << "modules.java.jdkPath:/blubb";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::zip_data()
{
    QTest::addColumn<QString>("binaryName");
    QTest::newRow("zip") << "zip";
    QTest::newRow("jar") << "jar";
}

void TestBlackbox::zipInvalid()
{
    QDir::setCurrent(testDataDir + "/archiver");
    rmDirR(relativeBuildDir());
    QbsRunParameters params(QStringList() << "modules.archiver.type:zip"
                            << "modules.archiver.command:/bin/something");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("unrecognized archive tool: 'something'"), m_qbsStderr.constData());
}

TestBlackbox::TestBlackbox() : TestBlackboxBase (SRCDIR "/testdata", "blackbox")
{
}

void TestBlackbox::addFileTagToGeneratedArtifact()
{
    QDir::setCurrent(testDataDir + "/add-filetag-to-generated-artifact");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compressing my_app"), m_qbsStdout.constData());
    const QString compressedAppFilePath
            = relativeProductBuildDir("my_compressed_app") + '/'
            + qbs::Internal::HostOsInfo::appendExecutableSuffix("compressed-my_app");
    QVERIFY(regularFileExists(compressedAppFilePath));
}

void TestBlackbox::alwaysRun()
{
    QFETCH(QString, projectFile);

    QDir::setCurrent(testDataDir + "/always-run");
    rmDirR(relativeBuildDir());
    QbsRunParameters params("build", QStringList() << "-f" << projectFile);
    if (projectFile.contains("transformer")) {
        params.expectFailure = true;
        QVERIFY(runQbs(params) != 0);
        QVERIFY2(m_qbsStderr.contains("removed"), m_qbsStderr.constData());
        return;
    }
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("yo"));
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("yo"));
    WAIT_FOR_NEW_TIMESTAMP();
    QFile f(projectFile);
    QVERIFY2(f.open(QIODevice::ReadWrite), qPrintable(f.errorString()));
    QByteArray content = f.readAll();
    content.replace("alwaysRun: false", "alwaysRun: true");
    f.resize(0);
    f.write(content);
    f.close();

    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("yo"));
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("yo"));
}

void TestBlackbox::alwaysRun_data()
{
    QTest::addColumn<QString>("projectFile");
    QTest::newRow("Transformer") << "transformer.qbs";
    QTest::newRow("Rule") << "rule.qbs";
}

void TestBlackbox::artifactScanning()
{
    const QString projectDir = testDataDir + "/artifact-scanning";
    QDir::setCurrent(projectDir);
    QbsRunParameters params(QStringList("-vv"));

    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"external.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("p1.cpp");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("p2.cpp");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("p3.cpp");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("shared.h");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"external.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("external.h");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("subdir/external2.h");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("external-indirect.h");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("p1.cpp");
    params.command = "resolve";
    params.arguments << "modules.cpp.treatSystemHeadersAsDependencies:true";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 1);
}

void TestBlackbox::buildDirectories()
{
    const QString projectDir
            = QDir::cleanPath(testDataDir + QLatin1String("/build-directories"));
    const QString projectBuildDir = projectDir + '/' + relativeBuildDir();
    QDir::setCurrent(projectDir);
    QCOMPARE(runQbs(), 0);
    const QStringList outputLines
            = QString::fromLocal8Bit(m_qbsStdout.trimmed()).split('\n', QString::SkipEmptyParts);
    QVERIFY2(outputLines.contains(projectDir + '/' + relativeProductBuildDir("p1")),
             m_qbsStdout.constData());
    QVERIFY2(outputLines.contains(projectDir + '/' + relativeProductBuildDir("p2")),
             m_qbsStdout.constData());
    QVERIFY2(outputLines.contains(projectBuildDir), m_qbsStdout.constData());
    QVERIFY2(outputLines.contains(projectDir), m_qbsStdout.constData());
}

void TestBlackbox::buildEnvChange()
{
    QDir::setCurrent(testDataDir + "/buildenv-change");
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << "-k";
    QVERIFY(runQbs(params) != 0);
    const bool isMsvc = m_qbsStdout.contains("msvc");
    QVERIFY2(m_qbsStdout.contains("compiling file.c"), m_qbsStdout.constData());
    QString includePath = QDir::currentPath() + "/subdir";
    params.environment.insert("CPLUS_INCLUDE_PATH", includePath);
    params.environment.insert("CL", "/I" + includePath);
    QVERIFY(runQbs(params) != 0);
    params.command = "resolve";
    params.expectFailure = false;
    params.arguments.clear();
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QCOMPARE(m_qbsStdout.contains("compiling file.c"), isMsvc);
    includePath = QDir::currentPath() + "/subdir2";
    params.environment.insert("CPLUS_INCLUDE_PATH", includePath);
    params.environment.insert("CL", "/I" + includePath);
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QCOMPARE(m_qbsStdout.contains("compiling file.c"), isMsvc);
    params.environment = QProcessEnvironment::systemEnvironment();
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
}

void TestBlackbox::buildGraphVersions()
{
    QDir::setCurrent(testDataDir + "/build-graph-versions");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QFile bgFile(relativeBuildGraphFilePath());
    QVERIFY2(bgFile.open(QIODevice::ReadWrite), qPrintable(bgFile.errorString()));
    bgFile.write("blubb");
    bgFile.close();

    // The first attempt at simple rebuilding as well as subsequent ones must fail.
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Cannot use stored build graph"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains("Use the 'resolve' command"), m_qbsStderr.constData());
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Cannot use stored build graph"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains("Use the 'resolve' command"), m_qbsStderr.constData());

    // On re-resolving, the error turns into a warning and a new build graph is created.
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    QVERIFY2(m_qbsStderr.contains("Cannot use stored build graph"), m_qbsStderr.constData());
    QVERIFY2(!m_qbsStderr.contains("Use the 'resolve' command"), m_qbsStderr.constData());

    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStderr.contains("Cannot use stored build graph"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::changedFiles_data()
{
    QTest::addColumn<bool>("useChangedFilesForInitialBuild");
    QTest::newRow("initial build with changed files") << true;
    QTest::newRow("initial build without changed files") << false;
}

void TestBlackbox::changedFiles()
{
    QFETCH(bool, useChangedFilesForInitialBuild);

    QDir::setCurrent(testDataDir + "/changed-files");
    rmDirR(relativeBuildDir());
    const QString changedFile = QDir::cleanPath(QDir::currentPath() + "/file1.cpp");
    QbsRunParameters params1;
    if (useChangedFilesForInitialBuild)
        params1 = QbsRunParameters(QStringList("--changed-files") << changedFile);

    // Initial run: Build all files, even though only one of them was marked as changed
    //              (if --changed-files was used).
    QCOMPARE(runQbs(params1), 0);
    QCOMPARE(m_qbsStdout.count("compiling"), 3);
    QCOMPARE(m_qbsStdout.count("creating"), 3);

    WAIT_FOR_NEW_TIMESTAMP();
    touch(QDir::currentPath() + "/main.cpp");

    // Now only the file marked as changed must be compiled, even though it hasn't really
    // changed and another one has.
    QbsRunParameters params2(QStringList("--changed-files") << changedFile);
    QCOMPARE(runQbs(params2), 0);
    QCOMPARE(m_qbsStdout.count("compiling"), 1);
    QCOMPARE(m_qbsStdout.count("creating"), 1);
    QVERIFY2(m_qbsStdout.contains("file1.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::changeInDisabledProduct()
{
    QDir::setCurrent(testDataDir + "/change-in-disabled-product");
    QCOMPARE(runQbs(), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile("change-in-disabled-product.qbs");
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray content = projectFile.readAll();
    content.replace("// 'test2.txt'", "'test2.txt'");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::changeInImportedFile()
{
    QDir::setCurrent(testDataDir + "/change-in-imported-file");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("old output"), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    QFile jsFile("prepare.js");
    QVERIFY2(jsFile.open(QIODevice::ReadWrite), qPrintable(jsFile.errorString()));
    QByteArray content = jsFile.readAll();
    content.replace("old output", "new output");
    jsFile.resize(0);
    jsFile.write(content);
    jsFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("new output"), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(jsFile.open(QIODevice::ReadWrite), qPrintable(jsFile.errorString()));
    jsFile.resize(0);
    jsFile.write(content);
    jsFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("output"), m_qbsStdout.constData());
}

void TestBlackbox::changeTrackingAndMultiplexing()
{
    QDir::setCurrent(testDataDir + "/change-tracking-and-multiplexing");
    QCOMPARE(runQbs(QStringList("modules.cpp.staticLibraryPrefix:prefix1")), 0);
    QCOMPARE(m_qbsStdout.count("compiling lib.cpp"), 2);
    QCOMPARE(m_qbsStdout.count("creating prefix1l"), 2);
    QCOMPARE(runQbs(QbsRunParameters("resolve",
                                     QStringList("modules.cpp.staticLibraryPrefix:prefix2"))), 0);
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.count("compiling lib.cpp"), 0);
    QCOMPARE(m_qbsStdout.count("creating prefix2l"), 2);
}

static QJsonObject findByName(const QJsonArray &objects, const QString &name)
{
    for (const QJsonValue v : objects) {
        if (!v.isObject())
            continue;
        QJsonObject obj = v.toObject();
        const QString objName = obj.value(QLatin1String("name")).toString();
        if (objName == name)
            return obj;
    }
    return QJsonObject();
}

void TestBlackbox::dependenciesProperty()
{
    QDir::setCurrent(testDataDir + QLatin1String("/dependenciesProperty"));
    QCOMPARE(runQbs(), 0);
    QFile depsFile(relativeProductBuildDir("product1") + QLatin1String("/product1.deps"));
    QVERIFY(depsFile.open(QFile::ReadOnly));

    QJsonParseError jsonerror;
    QJsonDocument jsondoc = QJsonDocument::fromJson(depsFile.readAll(), &jsonerror);
    if (jsonerror.error != QJsonParseError::NoError) {
        qDebug() << jsonerror.errorString();
        QFAIL("JSON parsing failed.");
    }
    QVERIFY(jsondoc.isArray());
    QJsonArray dependencies = jsondoc.array();
    QCOMPARE(dependencies.size(), 2);
    QJsonObject product2 = findByName(dependencies, QStringLiteral("product2"));
    QJsonArray product2_type = product2.value(QLatin1String("type")).toArray();
    QCOMPARE(product2_type.size(), 1);
    QCOMPARE(product2_type.first().toString(), QLatin1String("application"));
    QCOMPARE(product2.value(QLatin1String("narf")).toString(), QLatin1String("zort"));
    QJsonArray product2_deps = product2.value(QLatin1String("dependencies")).toArray();
    QVERIFY(!product2_deps.empty());
    QJsonObject product2_qbs = findByName(product2_deps, QStringLiteral("qbs"));
    QVERIFY(!product2_qbs.empty());
    QJsonObject product2_cpp = findByName(product2_deps, QStringLiteral("cpp"));
    QJsonArray product2_cpp_defines = product2_cpp.value(QLatin1String("defines")).toArray();
    QCOMPARE(product2_cpp_defines.size(), 1);
    QCOMPARE(product2_cpp_defines.first().toString(), QLatin1String("SMURF"));
    QJsonArray cpp_dependencies = product2_cpp.value("dependencies").toArray();
    QVERIFY(!cpp_dependencies.isEmpty());
    int qbsCount = 0;
    for (int i = 0; i < cpp_dependencies.size(); ++i) {
        if (cpp_dependencies.at(i).toObject().value("name").toString() == "qbs")
            ++qbsCount;
    }
    QCOMPARE(qbsCount, 1);
}

void TestBlackbox::dependencyProfileMismatch()
{
    QDir::setCurrent(testDataDir + "/dependency-profile-mismatch");
    const SettingsPtr s = settings();
    qbs::Internal::TemporaryProfile depProfile("qbs_autotests_profileMismatch", s.get());
    depProfile.p.setValue("qbs.architecture", "x86"); // Profiles must not be empty...
    s->sync();
    QbsRunParameters params(QStringList() << ("project.mainProfile:" + profileName())
                            << ("project.depProfile:" + depProfile.p.name()));
    params.expectFailure = true;
    QVERIFY2(runQbs(params) != 0, m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains(profileName().toLocal8Bit())
             && m_qbsStderr.contains("', which does not exist"),
             m_qbsStderr.constData());
}

void TestBlackbox::deprecatedProperty()
{
    QDir::setCurrent(testDataDir + "/deprecated-property");
    QbsRunParameters params(QStringList("-q"));
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    m_qbsStderr = QDir::fromNativeSeparators(QString::fromLocal8Bit(m_qbsStderr)).toLocal8Bit();
    QVERIFY2(m_qbsStderr.contains("deprecated-property.qbs:6:24 The property 'oldProp' is "
            "deprecated and will be removed in Qbs 99.9.0."), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains("deprecated-property.qbs:7:28 The property 'veryOldProp' can no "
            "longer be used. It was removed in Qbs 1.3.0."), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains("Property 'forgottenProp' was scheduled for removal in version "
                                  "1.8.0, but is still present."), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains("themodule/m.qbs:22:5 Removal version for 'forgottenProp' "
                                  "specified here."), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.count("Use newProp instead.") == 2, m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.count("is deprecated") == 1, m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.count("was removed") == 1, m_qbsStderr.constData());
}

void TestBlackbox::disappearedProfile()
{
    QDir::setCurrent(testDataDir + "/disappeared-profile");
    QbsRunParameters resolveParams;

    // First, we need to fail, because we don't tell qbs where the module is.
    resolveParams.expectFailure = true;
    QVERIFY(runQbs(resolveParams) != 0);

    // Now we set up a profile with all the necessary information, and qbs succeeds.
    qbs::Settings settings(QDir::currentPath() + "/settings-dir");
    qbs::Profile profile("p", &settings);
    profile.setValue("m.p1", "p1 from profile");
    profile.setValue("m.p2", "p2 from profile");
    profile.setValue("preferences.qbsSearchPaths",
                     QStringList({QDir::currentPath() + "/modules-dir"}));
    settings.sync();
    resolveParams.command = "resolve";
    resolveParams.expectFailure = false;
    resolveParams.settingsDir = settings.baseDirectory();
    resolveParams.profile = profile.name();
    QCOMPARE(runQbs(resolveParams), 0);

    // Now we change a property in the profile, but because we don't use the "resolve" command,
    // the old profile contents stored in the build graph are used.
    profile.setValue("m.p2", "p2 new from profile");
    settings.sync();
    QbsRunParameters buildParams;
    buildParams.profile.clear();
    QCOMPARE(runQbs(buildParams), 0);
    QVERIFY2(m_qbsStdout.contains("Creating dummy1.txt with p1 from profile"),
             m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("Creating dummy2.txt with p2 from profile"),
             m_qbsStdout.constData());

    // Now we do use the "resolve" command, so the new property value is taken into account.
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(buildParams), 0);
    QVERIFY2(!m_qbsStdout.contains("Creating dummy1.txt"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("Creating dummy2.txt with p2 new from profile"),
             m_qbsStdout.constData());

    // Now we change the profile again without a "resolve" command. However, this time we
    // force re-resolving indirectly by changing a project file. The updated property value
    // must still not be taken into account.
    profile.setValue("m.p1", "p1 new from profile");
    settings.sync();
    WAIT_FOR_NEW_TIMESTAMP();
    QFile f(QDir::currentPath() + "/modules-dir/modules/m/m.qbs");
    QVERIFY2(f.open(QIODevice::ReadWrite), qPrintable(f.errorString()));
    QByteArray contents = f.readAll();
    contents.replace("property string p1", "property string p1: 'p1 from module'");
    f.seek(0);
    f.write(contents);
    f.close();
    QCOMPARE(runQbs(buildParams), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("Creating dummy1.txt"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("Creating dummy2.txt"), m_qbsStdout.constData());

    // Now we run the "resolve" command without giving the necessary settings path to find
    // the profile.
    resolveParams.expectFailure = true;
    resolveParams.settingsDir.clear();
    resolveParams.profile.clear();
    QVERIFY(runQbs(resolveParams) != 0);
    QVERIFY2(m_qbsStderr.contains("profile"), m_qbsStderr.constData());
}

void TestBlackbox::discardUnusedData()
{
    QDir::setCurrent(testDataDir + "/discard-unused-data");
    rmDirR(relativeBuildDir());
    QFETCH(QString, discardString);
    QFETCH(bool, symbolPresent);
    QbsRunParameters params;
    if (!discardString.isEmpty())
        params.arguments << ("modules.cpp.discardUnusedData:" + discardString);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("is Darwin"), m_qbsStdout.constData());
    const bool isDarwin = m_qbsStdout.contains("is Darwin: true");
    const QString output = QString::fromLocal8Bit(m_qbsStdout);
    QRegExp pattern(".*---(.*)---.*");
    QVERIFY2(pattern.exactMatch(output), qPrintable(output));
    QCOMPARE(pattern.captureCount(), 1);
    const QString nmPath = pattern.capturedTexts().at(1);
    if (!QFile::exists(nmPath))
        QSKIP("Cannot check for symbol presence: No nm found.");
    QProcess nm;
    nm.start(nmPath, QStringList(QDir::currentPath() + '/' + relativeExecutableFilePath("app")));
    QVERIFY(nm.waitForStarted());
    QVERIFY(nm.waitForFinished());
    const QByteArray nmOutput = nm.readAllStandardOutput();
    QVERIFY2(nm.exitCode() == 0, nm.readAllStandardError().constData());
    if (!symbolPresent && !isDarwin)
        QSKIP("Unused symbol detection only supported on Darwin");
    QVERIFY2(nmOutput.contains("unusedFunc") == symbolPresent, nmOutput.constData());
}

void TestBlackbox::discardUnusedData_data()
{
    QTest::addColumn<QString>("discardString");
    QTest::addColumn<bool>("symbolPresent");

    QTest::newRow("discard") << QString("true") << false;
    QTest::newRow("don't discard") << QString("false") << true;
    QTest::newRow("default") << QString() << true;
}

void TestBlackbox::driverLinkerFlags()
{
    QDir::setCurrent(testDataDir + QLatin1String("/driver-linker-flags"));
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("-n"))), 0);
    if (!m_qbsStdout.contains("toolchain is GCC-like"))
        QSKIP("Test applies on GCC-like toolchains only");
    QFETCH(QString, linkerMode);
    QFETCH(bool, expectDriverOption);
    const QString linkerModeArg = "modules.cpp.linkerMode:" + linkerMode;
    QCOMPARE(runQbs(QStringList({"-n", "--command-echo-mode", "command-line", linkerModeArg})), 0);
    const QByteArray driverArg = "-nostartfiles";
    const QByteArrayList output = m_qbsStdout.split('\n');
    QByteArray compileLine;
    QByteArray linkLine;
    for (const QByteArray &line : output) {
        if (line.contains(" -c "))
            compileLine = line;
        else if (line.contains("main.cpp.o"))
            linkLine = line;
    }
    QVERIFY(!compileLine.isEmpty());
    QVERIFY(!linkLine.isEmpty());
    QVERIFY2(!compileLine.contains(driverArg), compileLine.constData());
    QVERIFY2(linkLine.contains(driverArg) == expectDriverOption, linkLine.constData());
}

void TestBlackbox::driverLinkerFlags_data()
{
    QTest::addColumn<QString>("linkerMode");
    QTest::addColumn<bool>("expectDriverOption");

    QTest::newRow("link using compiler driver") << "automatic" << true;
    QTest::newRow("link using linker") << "manual" << false;
}

void TestBlackbox::dynamicLibraryInModule()
{
    QDir::setCurrent(testDataDir + "/dynamic-library-in-module");
    const QString installRootSpec = QString("qbs.installRoot:") + QDir::currentPath();
    QbsRunParameters libParams(QStringList({"-f", "thelibs.qbs", installRootSpec}));
    libParams.buildDirectory = "libbuild";
    QCOMPARE(runQbs(libParams), 0);
    QbsRunParameters appParams("run", QStringList({"-f", "theapp.qbs", installRootSpec}));
    appParams.buildDirectory = "appbuild";
    QCOMPARE(runQbs(appParams), 0);
    QVERIFY2(m_qbsStdout.contains("Hello from thelib"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("Hello from theotherlib"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("thirdlib"), m_qbsStdout.constData());
    QVERIFY(!QFileInfo::exists(appParams.buildDirectory + '/'
                               + qbs::InstallOptions::defaultInstallRoot()));
}

void TestBlackbox::symlinkRemoval()
{
    if (HostOsInfo::isWindowsHost())
        QSKIP("No symlink support on Windows.");
    QDir::setCurrent(testDataDir + "/symlink-removal");
    QVERIFY(QDir::current().mkdir("dir1"));
    QVERIFY(QDir::current().mkdir("dir2"));
    QVERIFY(QFile::link("dir2", "dir1/broken-link"));
    QVERIFY(QFile::link(QFileInfo("dir2").absoluteFilePath(), "dir1/valid-link-to-dir"));
    QVERIFY(QFile::link(QFileInfo("symlink-removal.qbs").absoluteFilePath(),
                        "dir1/valid-link-to-file"));
    QCOMPARE(runQbs(), 0);
    QVERIFY(!QFile::exists("dir1"));
    QVERIFY(QFile::exists("dir2"));
    QVERIFY(QFile::exists("symlink-removal.qbs"));
}

void TestBlackbox::usingsAsSoleInputsNonMultiplexed()
{
    QDir::setCurrent(testDataDir + QLatin1String("/usings-as-sole-inputs-non-multiplexed"));
    QCOMPARE(runQbs(), 0);
    const QString p3BuildDir = relativeProductBuildDir("p3");
    QVERIFY(regularFileExists(p3BuildDir + "/custom1.out.plus"));
    QVERIFY(regularFileExists(p3BuildDir + "/custom2.out.plus"));
}

void TestBlackbox::variantSuffix()
{
    QDir::setCurrent(testDataDir + "/variant-suffix");
    QFETCH(bool, multiplex);
    QFETCH(bool, expectFailure);
    QFETCH(QString, variantSuffix);
    QFETCH(QString, buildVariant);
    QFETCH(QVariantMap, fileNames);
    QbsRunParameters params;
    params.command = "resolve";
    params.arguments << "--force-probe-execution";
    if (multiplex)
        params.arguments << "products.l.multiplex:true";
    else
        params.arguments << ("modules.qbs.buildVariant:" + buildVariant);
    if (!variantSuffix.isEmpty())
        params.arguments << ("modules.cpp.variantSuffix:" + variantSuffix);
    QCOMPARE(runQbs(params), 0);
    const QString fileNameMapKey = m_qbsStdout.contains("is Windows: true")
            ? "windows" : m_qbsStdout.contains("is Apple: true") ? "apple" : "unix";
    if (variantSuffix.isEmpty() && multiplex && fileNameMapKey == "unix")
        expectFailure = true;
    params.command = "build";
    params.expectFailure = expectFailure;
    params.arguments = QStringList("--clean-install-root");
    QCOMPARE(runQbs(params) == 0, !expectFailure);
    if (expectFailure)
        return;
    const QStringList fileNameList = fileNames.value(fileNameMapKey).toStringList();
    for (const QString &fileName : fileNameList) {
        QFile libFile("default/install-root/lib/" + fileName);
        QVERIFY2(libFile.exists(), qPrintable(libFile.fileName()));
    }
}

void TestBlackbox::variantSuffix_data()
{
    QTest::addColumn<bool>("multiplex");
    QTest::addColumn<bool>("expectFailure");
    QTest::addColumn<QString>("variantSuffix");
    QTest::addColumn<QString>("buildVariant");
    QTest::addColumn<QVariantMap>("fileNames");

    QTest::newRow("default suffix, debug") << false << false << QString() << QString("debug")
            << QVariantMap({std::make_pair(QString("windows"), QStringList("libl.ext")),
                            std::make_pair(QString("apple"), QStringList("libl.ext")),
                            std::make_pair(QString("unix"), QStringList("libl.ext"))});
    QTest::newRow("default suffix, release") << false << false << QString() << QString("release")
            << QVariantMap({std::make_pair(QString("windows"), QStringList("libl.ext")),
                            std::make_pair(QString("apple"), QStringList("libl.ext")),
                            std::make_pair(QString("unix"), QStringList("libl.ext"))});
    QTest::newRow("custom suffix, debug") << false << false << QString("blubb") << QString("debug")
            << QVariantMap({std::make_pair(QString("windows"), QStringList("liblblubb.ext")),
                            std::make_pair(QString("apple"), QStringList("liblblubb.ext")),
                            std::make_pair(QString("unix"), QStringList("liblblubb.ext"))});
    QTest::newRow("custom suffix, release") << false << false << QString("blubb")
            << QString("release")
            << QVariantMap({std::make_pair(QString("windows"), QStringList("liblblubb.ext")),
                            std::make_pair(QString("apple"), QStringList("liblblubb.ext")),
                            std::make_pair(QString("unix"), QStringList("liblblubb.ext"))});
    QTest::newRow("default suffix, multiplex") << true << false << QString() << QString()
            << QVariantMap({std::make_pair(QString("windows"),
                            QStringList({"libl.ext", "libld.ext"})),
                            std::make_pair(QString("apple"),
                            QStringList({"libl.ext", "libl_debug.ext"})),
                            std::make_pair(QString("unix"), QStringList())});
    QTest::newRow("custom suffix, multiplex") << true << true << QString("blubb") << QString()
            << QVariantMap({std::make_pair(QString("windows"), QStringList()),
                            std::make_pair(QString("apple"), QStringList()),
                            std::make_pair(QString("unix"), QStringList())});
}

static bool waitForProcessSuccess(QProcess &p)
{
    if (!p.waitForStarted() || !p.waitForFinished()) {
        qDebug() << p.errorString();
        return false;
    }
    if (p.exitCode() != 0) {
        qDebug() << p.readAllStandardError();
        return false;
    }
    return true;
}

void TestBlackbox::vcsGit()
{
    const QString gitFilePath = findExecutable(QStringList("git"));
    if (gitFilePath.isEmpty())
        QSKIP("git not found");

    // Set up repo.
    QTemporaryDir repoDir;
    QVERIFY(repoDir.isValid());
    ccp(testDataDir + "/vcs", repoDir.path());
    QDir::setCurrent(repoDir.path());

    QProcess git;
    git.start(gitFilePath, QStringList("init"));
    QVERIFY(waitForProcessSuccess(git));
    git.start(gitFilePath, QStringList({"config", "user.name", "My Name"}));
    QVERIFY(waitForProcessSuccess(git));
    git.start(gitFilePath, QStringList({"config", "user.email", "me@example.com"}));
    QVERIFY(waitForProcessSuccess(git));

    // First qbs run fails: No git metadata yet.
    QbsRunParameters failParams;
    failParams.expectFailure = true;
    QVERIFY(runQbs(failParams) != 0);

    // Initial commit
    git.start(gitFilePath, QStringList({"add", "main.cpp"}));
    QVERIFY(waitForProcessSuccess(git));
    git.start(gitFilePath, QStringList({"commit", "-m", "initial commit"}));
    QVERIFY(waitForProcessSuccess(git));

    // Initial run.
    QbsRunParameters params(QStringList{"-f", repoDir.path()});
    params.workingDir = repoDir.path() + "/..";
    params.buildDirectory = repoDir.path();
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("generating vcs-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());

    // Run with no changes.
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(!m_qbsStdout.contains("generating vcs-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());

    // Run with changed source file.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("main.cpp");
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(!m_qbsStdout.contains("generating vcs-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());

    // Add new file to repo.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("blubb.txt");
    git.start(gitFilePath, QStringList({"add", "blubb.txt"}));
    QVERIFY(waitForProcessSuccess(git));
    git.start(gitFilePath, QStringList({"commit", "-m", "blubb!"}));
    QVERIFY(waitForProcessSuccess(git));
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("generating vcs-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
}

void TestBlackbox::vcsSubversion()
{
    const QString svnadminFilePath = findExecutable(QStringList("svnadmin"));
    if (svnadminFilePath.isEmpty())
        QSKIP("svnadmin not found");
    const QString svnFilePath = findExecutable(QStringList("svn"));
    if (svnFilePath.isEmpty())
        QSKIP("svn not found");

    // Set up repo.
    QTemporaryDir repoDir;
    QVERIFY(repoDir.isValid());
    QProcess proc;
    proc.setWorkingDirectory(repoDir.path());
    proc.start(svnadminFilePath, QStringList({"create", "vcstest"}));
    QVERIFY(waitForProcessSuccess(proc));
    const QString projectUrl = "file://" + repoDir.path() + "/vcstest/trunk";
    proc.start(svnFilePath, QStringList({"import", testDataDir + "/vcs", projectUrl, "-m",
                                         "initial import"}));
    QVERIFY(waitForProcessSuccess(proc));
    QTemporaryDir checkoutDir;
    QVERIFY(checkoutDir.isValid());
    proc.setWorkingDirectory(checkoutDir.path());
    proc.start(svnFilePath, QStringList({"co", projectUrl, "."}));
    QVERIFY(waitForProcessSuccess(proc));

    // Initial runs
    QDir::setCurrent(checkoutDir.path());
    QbsRunParameters failParams;
    failParams.command = "run";
    failParams.expectFailure = true;
    const int retval = runQbs(failParams);
    if (m_qbsStderr.contains("svn too old"))
        QSKIP("svn too old");
    QCOMPARE(retval, 0);
    QVERIFY2(m_qbsStdout.contains("I was built from 1"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(!m_qbsStdout.contains("generating vcs-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("I was built from 1"), m_qbsStdout.constData());

    // Run with changed source file.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("main.cpp");
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(!m_qbsStdout.contains("generating vcs-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("I was built from 1"), m_qbsStdout.constData());

    // Add new file to repo.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("blubb.txt");
    proc.start(svnFilePath, QStringList({"add", "blubb.txt"}));
    QVERIFY(waitForProcessSuccess(proc));
    proc.start(svnFilePath, QStringList({"commit", "-m", "blubb!"}));
    QVERIFY(waitForProcessSuccess(proc));
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(m_qbsStdout.contains("I was built from 2"), m_qbsStdout.constData());
}

void TestBlackbox::versionCheck()
{
    QDir::setCurrent(testDataDir + "/versioncheck");
    QFETCH(QString, requestedMinVersion);
    QFETCH(QString, requestedMaxVersion);
    QFETCH(QString, actualVersion);
    QFETCH(QString, errorMessage);
    QbsRunParameters params;
    params.expectFailure = !errorMessage.isEmpty();
    params.arguments << "-n"
                     << ("products.versioncheck.requestedMinVersion:'" + requestedMinVersion + "'")
                     << ("products.versioncheck.requestedMaxVersion:'" + requestedMaxVersion + "'")
                     << ("modules.lower.version:'" + actualVersion + "'");
    QCOMPARE(runQbs(params) == 0, errorMessage.isEmpty());
    if (params.expectFailure)
        QVERIFY2(QString(m_qbsStderr).contains(errorMessage), m_qbsStderr.constData());
}

void TestBlackbox::versionCheck_data()
{
    QTest::addColumn<QString>("requestedMinVersion");
    QTest::addColumn<QString>("requestedMaxVersion");
    QTest::addColumn<QString>("actualVersion");
    QTest::addColumn<QString>("errorMessage");

    QTest::newRow("ok1") << "1.0" << "1.1" << "1.0" << QString();
    QTest::newRow("ok2") << "1.0" << "2.0.1" << "2.0" << QString();
    QTest::newRow("ok3") << "1.0" << "2.5" << "1.5" << QString();
    QTest::newRow("ok3") << "1.0" << "2.0" << "1.99" << QString();
    QTest::newRow("bad1") << "2.0" << "2.1" << "1.5" << "needs to be at least";
    QTest::newRow("bad2") << "2.0" << "3.0" << "1.5" << "needs to be at least";
    QTest::newRow("bad3") << "2.0" << "3.0" << "3.5" << "needs to be lower than";
    QTest::newRow("bad4") << "2.0" << "3.0" << "3.0" << "needs to be lower than";

    // "bad" because the "higer" module has stronger requirements.
    QTest::newRow("bad5") << "0.1" << "0.9" << "0.5" << "Impossible version constraint";
}

void TestBlackbox::versionScript()
{
    const SettingsPtr s = settings();
    Profile buildProfile(profileName(), s.get());
    QStringList toolchain = buildProfile.value("qbs.toolchain").toStringList();
    if (!toolchain.contains("gcc") || targetOs() != HostOsInfo::HostOsLinux)
        QSKIP("version script test only applies to Linux");
    QDir::setCurrent(testDataDir + "/versionscript");
    QCOMPARE(runQbs(QbsRunParameters(QStringList("-q")
                                     << ("qbs.installRoot:" + QDir::currentPath()))), 0);
    const QString output = QString::fromLocal8Bit(m_qbsStderr);
    QRegExp pattern(".*---(.*)---.*");
    QVERIFY2(pattern.exactMatch(output), qPrintable(output));
    QCOMPARE(pattern.captureCount(), 1);
    const QString nmPath = pattern.capturedTexts().at(1);
    if (!QFile::exists(nmPath))
        QSKIP("Cannot check for symbol presence: No nm found.");
    QProcess nm;
    nm.start(nmPath, QStringList(QDir::currentPath() + "/libversionscript.so"));
    QVERIFY(nm.waitForStarted());
    QVERIFY(nm.waitForFinished());
    const QByteArray allSymbols = nm.readAllStandardOutput();
    QCOMPARE(nm.exitCode(), 0);
    QVERIFY2(allSymbols.contains("dummyLocal"), allSymbols.constData());
    QVERIFY2(allSymbols.contains("dummyGlobal"), allSymbols.constData());
    nm.start(nmPath, QStringList() << "-g" << QDir::currentPath() + "/libversionscript.so");
    QVERIFY(nm.waitForStarted());
    QVERIFY(nm.waitForFinished());
    const QByteArray globalSymbols = nm.readAllStandardOutput();
    QCOMPARE(nm.exitCode(), 0);
    QVERIFY2(!globalSymbols.contains("dummyLocal"), allSymbols.constData());
    QVERIFY2(globalSymbols.contains("dummyGlobal"), allSymbols.constData());
}

void TestBlackbox::wholeArchive()
{
    QDir::setCurrent(testDataDir + "/whole-archive");
    QFETCH(QString, wholeArchiveString);
    QFETCH(bool, ruleInvalidationExpected);
    QFETCH(bool, dllLinkingExpected);
    const QbsRunParameters resolveParams("resolve",
            QStringList("-vv") << "products.dynamiclib.linkWholeArchive:" + wholeArchiveString);
    QCOMPARE(runQbs(QbsRunParameters(resolveParams)), 0);
    const QByteArray resolveStderr = m_qbsStderr;
    QCOMPARE(runQbs(QbsRunParameters(QStringList({ "-p", "dynamiclib" }))), 0);
    const bool wholeArchive = !wholeArchiveString.isEmpty();
    const bool outdatedVisualStudio = wholeArchive && m_qbsStderr.contains("upgrade");
    const QByteArray invalidationOutput
            = "Value for property 'staticlib 1:cpp.linkWholeArchive' has changed.";
    if (!outdatedVisualStudio)
        QCOMPARE(resolveStderr.contains(invalidationOutput), ruleInvalidationExpected);
    QCOMPARE(m_qbsStdout.contains("linking"), dllLinkingExpected && !outdatedVisualStudio);
    QbsRunParameters buildParams(QStringList("-p"));
    buildParams.expectFailure = !wholeArchive || outdatedVisualStudio;
    buildParams.arguments << "app1";
    QCOMPARE(runQbs(QbsRunParameters(buildParams)) == 0, wholeArchive && !outdatedVisualStudio);
    buildParams.arguments.last() = "app2";
    QCOMPARE(runQbs(QbsRunParameters(buildParams)) == 0, wholeArchive && !outdatedVisualStudio);
    buildParams.arguments.last() = "app4";
    QCOMPARE(runQbs(QbsRunParameters(buildParams)) == 0, wholeArchive && !outdatedVisualStudio);
    buildParams.arguments.last() = "app3";
    buildParams.expectFailure = true;
    QVERIFY(runQbs(QbsRunParameters(buildParams)) != 0);
}

void TestBlackbox::wholeArchive_data()
{
    QTest::addColumn<QString>("wholeArchiveString");
    QTest::addColumn<bool>("ruleInvalidationExpected");
    QTest::addColumn<bool>("dllLinkingExpected");
    QTest::newRow("link normally") << QString() << false << true;
    QTest::newRow("link whole archive") << "true" << true << true;
    QTest::newRow("link whole archive again") << "notfalse" << false << false;
}

static bool symlinkExists(const QString &linkFilePath)
{
    return QFileInfo(linkFilePath).isSymLink();
}

void TestBlackbox::clean()
{
    const QString appObjectFilePath = relativeProductBuildDir("app") + '/' + inputDirHash(".")
            + objectFileName("/main.cpp", profileName());
    const QString appExeFilePath = relativeExecutableFilePath("app");
    const QString depObjectFilePath = relativeProductBuildDir("dep") + '/' + inputDirHash(".")
            + objectFileName("/dep.cpp", profileName());
    const QString depLibBase = relativeProductBuildDir("dep")
            + '/' + QBS_HOST_DYNAMICLIB_PREFIX + "dep";
    QString depLibFilePath;
    QStringList symlinks;
    if (qbs::Internal::HostOsInfo::isMacosHost()) {
        depLibFilePath = depLibBase + ".1.1.0" + QBS_HOST_DYNAMICLIB_SUFFIX;
        symlinks << depLibBase + ".1.1" + QBS_HOST_DYNAMICLIB_SUFFIX
                 << depLibBase + ".1"  + QBS_HOST_DYNAMICLIB_SUFFIX
                 << depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX;
    } else if (qbs::Internal::HostOsInfo::isAnyUnixHost()) {
        depLibFilePath = depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX + ".1.1.0";
        symlinks << depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX + ".1.1"
                 << depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX + ".1"
                 << depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX;
    } else {
        depLibFilePath = depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX;
    }

    QDir::setCurrent(testDataDir + "/clean");

    // Can't clean without a build graph.
    QbsRunParameters failParams("clean");
    failParams.expectFailure = true;
    QVERIFY(runQbs(failParams) != 0);

    // Default behavior: Remove all.
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("clean"))), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depLibFilePath).exists());
    for (const QString &symLink : qAsConst(symlinks))
        QVERIFY2(!symlinkExists(symLink), qPrintable(symLink));

    // Remove all, with a forced re-resolve in between.
    // This checks that rescuable artifacts are also removed.
    QCOMPARE(runQbs(QbsRunParameters("resolve",
                                     QStringList() << "modules.cpp.optimization:none")), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QCOMPARE(runQbs(QbsRunParameters("resolve",
                                     QStringList() << "modules.cpp.optimization:fast")), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QCOMPARE(runQbs(QbsRunParameters("clean")), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depLibFilePath).exists());
    for (const QString &symLink : qAsConst(symlinks))
        QVERIFY2(!symlinkExists(symLink), qPrintable(symLink));

    // Dry run.
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("clean"), QStringList("-n"))), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QVERIFY(regularFileExists(depObjectFilePath));
    QVERIFY(regularFileExists(depLibFilePath));
    for (const QString &symLink : qAsConst(symlinks))
        QVERIFY2(symlinkExists(symLink), qPrintable(symLink));

    // Product-wise, dependency only.
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QVERIFY(regularFileExists(depObjectFilePath));
    QVERIFY(regularFileExists(depLibFilePath));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("clean"), QStringList("-p") << "dep")), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depLibFilePath).exists());
    for (const QString &symLink : qAsConst(symlinks))
        QVERIFY2(!symlinkExists(symLink), qPrintable(symLink));

    // Product-wise, dependent product only.
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QVERIFY(regularFileExists(depObjectFilePath));
    QVERIFY(regularFileExists(depLibFilePath));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("clean"), QStringList("-p") << "app")), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(regularFileExists(depObjectFilePath));
    QVERIFY(regularFileExists(depLibFilePath));
    for (const QString &symLink : qAsConst(symlinks))
        QVERIFY2(symlinkExists(symLink), qPrintable(symLink));
}

void TestBlackbox::concurrentExecutor()
{
    QDir::setCurrent(testDataDir + "/concurrent-executor");
    QCOMPARE(runQbs(QStringList() << "-j" << "2"), 0);
    QVERIFY2(!m_qbsStderr.contains("ASSERT"), m_qbsStderr.constData());
}

void TestBlackbox::conditionalExport()
{
    QDir::setCurrent(testDataDir + "/conditional-export");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("missing define"), m_qbsStderr.constData());

    params.expectFailure = false;
    params.arguments << "project.enableExport:true";
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::conditionalFileTagger()
{
    QDir::setCurrent(testDataDir + "/conditional-filetagger");
    QbsRunParameters params(QStringList("products.theApp.enableTagger:false"));
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling"));
    params.arguments = QStringList("products.theApp.enableTagger:true");
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling"));
}

void TestBlackbox::configure()
{
    QDir::setCurrent(testDataDir + "/configure");
    QbsRunParameters params;
    params.command = "run";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("Configured at"), m_qbsStdout.constData());
}

void TestBlackbox::conflictingArtifacts()
{
    QDir::setCurrent(testDataDir + "/conflicting-artifacts");
    QbsRunParameters params(QStringList() << "-n");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Conflicting artifacts"), m_qbsStderr.constData());
}

void TestBlackbox::cxxLanguageVersion()
{
    QDir::setCurrent(testDataDir + "/cxx-language-version");
    rmDirR(relativeBuildDir());
    QFETCH(QString, version);
    QFETCH(QVariantMap, requiredFlags);
    QFETCH(QVariantMap, forbiddenFlags);
    QbsRunParameters resolveParams;
    resolveParams.command = "resolve";
    resolveParams.arguments << "--force-probe-execution";
    resolveParams.arguments << "modules.cpp.useLanguageVersionFallback:true";
    if (!version.isEmpty())
        resolveParams.arguments << ("modules.cpp.cxxLanguageVersion:" + version);
    QCOMPARE(runQbs(resolveParams), 0);
    QString mapKey;
    if (version == "c++17" && m_qbsStdout.contains("is even newer MSVC: true"))
        mapKey = "msvc-brandnew";
    if (m_qbsStdout.contains("is newer MSVC: true"))
        mapKey = "msvc-new";
    else if (m_qbsStdout.contains("is older MSVC: true"))
        mapKey = "msvc_old";
    else if (m_qbsStdout.contains("is GCC: true"))
        mapKey = "gcc";
    QVERIFY2(!mapKey.isEmpty(), m_qbsStdout.constData());
    QbsRunParameters buildParams;
    buildParams.expectFailure = mapKey == "gcc" && (version == "c++17" || version == "c++21");
    buildParams.arguments = QStringList({"--command-echo-mode", "command-line"});
    const int retVal = runQbs(buildParams);
    if (!buildParams.expectFailure)
        QCOMPARE(retVal, 0);
    const QString requiredFlag = requiredFlags.value(mapKey).toString();
    if (!requiredFlag.isEmpty())
        QVERIFY2(m_qbsStdout.contains(requiredFlag.toLocal8Bit()), m_qbsStdout.constData());
    const QString forbiddenFlag = forbiddenFlags.value(mapKey).toString();
    if (!forbiddenFlag.isEmpty())
        QVERIFY2(!m_qbsStdout.contains(forbiddenFlag.toLocal8Bit()), m_qbsStdout.constData());
}

void TestBlackbox::cxxLanguageVersion_data()
{
    QTest::addColumn<QString>("version");
    QTest::addColumn<QVariantMap>("requiredFlags");
    QTest::addColumn<QVariantMap>("forbiddenFlags");

    QTest::newRow("C++98")
            << QString("c++98")
            << QVariantMap({std::make_pair(QString("gcc"), QString("-std=c++98"))})
            << QVariantMap({std::make_pair(QString("msvc-old"), QString("/std:")),
                            std::make_pair(QString("msvc-new"), QString("/std:"))});
    QTest::newRow("C++11")
            << QString("c++11")
            << QVariantMap({std::make_pair(QString("gcc"), QString("-std=c++0x"))})
            << QVariantMap({std::make_pair(QString("msvc-old"), QString("/std:")),
                            std::make_pair(QString("msvc-new"), QString("/std:"))});
    QTest::newRow("C++14")
            << QString("c++14")
            << QVariantMap({std::make_pair(QString("gcc"), QString("-std=c++1y")),
                            std::make_pair(QString("msvc-new"), QString("/std:c++14"))
                           })
            << QVariantMap({std::make_pair(QString("msvc-old"), QString("/std:"))});
    QTest::newRow("C++17")
            << QString("c++17")
            << QVariantMap({std::make_pair(QString("gcc"), QString("-std=c++1z")),
                            std::make_pair(QString("msvc-new"), QString("/std:c++latest")),
                            std::make_pair(QString("msvc-brandnew"), QString("/std:c++17"))
                           })
            << QVariantMap({std::make_pair(QString("msvc-old"), QString("/std:"))});
    QTest::newRow("C++21")
            << QString("c++21")
            << QVariantMap({std::make_pair(QString("gcc"), QString("-std=c++21")),
                            std::make_pair(QString("msvc-new"), QString("/std:c++latest"))
                           })
            << QVariantMap({std::make_pair(QString("msvc-old"), QString("/std:"))});
    QTest::newRow("default")
            << QString()
            << QVariantMap()
            << QVariantMap({std::make_pair(QString("gcc"), QString("-std=")),
                            std::make_pair(QString("msvc-old"), QString("/std:")),
                            std::make_pair(QString("msvc-new"), QString("/std:"))});
}

void TestBlackbox::cpuFeatures()
{
    QDir::setCurrent(testDataDir + "/cpu-features");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    const bool isX86 = m_qbsStdout.contains("is x86: true");
    const bool isX64 = m_qbsStdout.contains("is x64: true");
    if (!isX86 && !isX64) {
        QVERIFY2(m_qbsStdout.contains("is x86: false") && m_qbsStdout.contains("is x64: false"),
                 m_qbsStdout.constData());
        QSKIP("Not an x86 host");
    }
    const bool isGcc = m_qbsStdout.contains("is gcc: true");
    const bool isMsvc = m_qbsStdout.contains("is msvc: true");
    if (!isGcc && !isMsvc) {
        QVERIFY2(m_qbsStdout.contains("is gcc: false") && m_qbsStdout.contains("is msvc: false"),
                 m_qbsStdout.constData());
        QSKIP("Neither GCC nor MSVC");
    }
    QbsRunParameters params(QStringList{"--command-echo-mode", "command-line"});
    params.expectFailure = true;
    runQbs(params);
    if (isGcc) {
        QVERIFY2(m_qbsStdout.contains("-msse2") && m_qbsStdout.contains("-mavx")
                 && m_qbsStdout.contains("-mno-avx512f"), m_qbsStdout.constData());
    } else {
        QVERIFY2(m_qbsStdout.contains("/arch:AVX"), m_qbsStdout.constData());
        QVERIFY2(!m_qbsStdout.contains("/arch:AVX2"), m_qbsStdout.constData());
        QVERIFY2(m_qbsStdout.contains("/arch:SSE2") == isX86, m_qbsStdout.constData());
    }
}

void TestBlackbox::renameDependency()
{
    QDir::setCurrent(testDataDir + "/renameDependency");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/renameDependency/work");
    QCOMPARE(runQbs(), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    QFile::remove("lib.h");
    QFile::remove("lib.cpp");
    ccp("../after", ".");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
}

void TestBlackbox::separateDebugInfo()
{
    QDir::setCurrent(testDataDir + "/separate-debug-info");
    QCOMPARE(runQbs(QbsRunParameters(QStringList("qbs.debugInformation:true"))), 0);

    const SettingsPtr s = settings();
    Profile buildProfile(profileName(), s.get());
    QStringList toolchain = buildProfile.value("qbs.toolchain").toStringList();
    std::string targetPlatform = buildProfile.value("qbs.targetPlatform").toString().toStdString();
    std::vector<std::string> targetOS = HostOsInfo::canonicalOSIdentifiers(targetPlatform);
    if (qbs::Internal::contains(targetOS, "darwin")
            || (targetPlatform.empty() && HostOsInfo::isMacosHost())) {
        QVERIFY(directoryExists(relativeProductBuildDir("app1") + "/app1.app.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app1")
            + "/app1.app.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app1")
            + "/app1.app.dSYM/Contents/Resources/DWARF/app1"));
        QCOMPARE(QDir(relativeProductBuildDir("app1")
            + "/app1.app.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(!QFile::exists(relativeProductBuildDir("app2") + "/app2.app.dSYM"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("app3") + "/app3.app.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app3")
            + "/app3.app/Contents/MacOS/app3.dwarf"));
        QVERIFY(directoryExists(relativeProductBuildDir("app4") + "/app4.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app4")
            + "/app4.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app4")
            + "/app4.dSYM/Contents/Resources/DWARF/app4"));
        QCOMPARE(QDir(relativeProductBuildDir("app4")
            + "/app4.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(regularFileExists(relativeProductBuildDir("app5") + "/app5.dwarf"));
        QVERIFY(directoryExists(relativeProductBuildDir("foo1") + "/foo1.framework.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo1")
            + "/foo1.framework.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo1")
            + "/foo1.framework.dSYM/Contents/Resources/DWARF/foo1"));
        QCOMPARE(QDir(relativeProductBuildDir("foo1")
            + "/foo1.framework.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(!QFile::exists(relativeProductBuildDir("foo2") + "/foo2.framework.dSYM"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("foo3") + "/foo3.framework.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo3")
            + "/foo3.framework/Versions/A/foo3.dwarf"));
        QVERIFY(directoryExists(relativeProductBuildDir("foo4") + "/libfoo4.dylib.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo4")
            + "/libfoo4.dylib.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo4")
            + "/libfoo4.dylib.dSYM/Contents/Resources/DWARF/libfoo4.dylib"));
        QCOMPARE(QDir(relativeProductBuildDir("foo4")
            + "/libfoo4.dylib.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(regularFileExists(relativeProductBuildDir("foo5") + "/libfoo5.dylib.dwarf"));
        QVERIFY(directoryExists(relativeProductBuildDir("bar1") + "/bar1.bundle.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar1")
            + "/bar1.bundle.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar1")
            + "/bar1.bundle.dSYM/Contents/Resources/DWARF/bar1"));
        QCOMPARE(QDir(relativeProductBuildDir("bar1")
            + "/bar1.bundle.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(!QFile::exists(relativeProductBuildDir("bar2") + "/bar2.bundle.dSYM"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("bar3") + "/bar3.bundle.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar3")
            + "/bar3.bundle/Contents/MacOS/bar3.dwarf"));
        QVERIFY(directoryExists(relativeProductBuildDir("bar4") + "/bar4.bundle.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar4")
            + "/bar4.bundle.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar4")
            + "/bar4.bundle.dSYM/Contents/Resources/DWARF/bar4.bundle"));
        QCOMPARE(QDir(relativeProductBuildDir("bar4")
            + "/bar4.bundle.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(regularFileExists(relativeProductBuildDir("bar5") + "/bar5.bundle.dwarf"));
    } else if (toolchain.contains("gcc")) {
        const bool isWindows = qbs::Internal::contains(targetOS, "windows");
        const QString exeSuffix = isWindows ? ".exe" : "";
        const QString dllPrefix = isWindows ? "" : "lib";
        const QString dllSuffix = isWindows ? ".dll" : ".so";
        QVERIFY(QFile::exists(relativeProductBuildDir("app1") + "/app1" + exeSuffix + ".debug"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("app2") + "/app2" + exeSuffix + ".debug"));
        QVERIFY(QFile::exists(relativeProductBuildDir("foo1")
                              + '/' + dllPrefix + "foo1" + dllSuffix + ".debug"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("foo2")
                               + '/' + "foo2" + dllSuffix + ".debug"));
        QVERIFY(QFile::exists(relativeProductBuildDir("bar1")
                              + '/' + dllPrefix +  "bar1" + dllSuffix + ".debug"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("bar2")
                               + '/' + dllPrefix + "bar2" + dllSuffix + ".debug"));
    } else if (toolchain.contains("msvc")) {
        QVERIFY(QFile::exists(relativeProductBuildDir("app1") + "/app1.pdb"));
        QVERIFY(QFile::exists(relativeProductBuildDir("foo1") + "/foo1.pdb"));
        QVERIFY(QFile::exists(relativeProductBuildDir("bar1") + "/bar1.pdb"));
        // MSVC's linker even creates a pdb file if /Z7 is passed to the compiler.
    } else {
        QSKIP("Unsupported toolchain. Skipping.");
    }
}

void TestBlackbox::trackAddFile()
{
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackAddFile");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackAddFile/work");
    const QbsRunParameters runParams("run", QStringList{"-qp", "someapp"});
    QCOMPARE(runQbs(runParams), 0);

    output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
    QString unchangedObjectFile = relativeBuildDir()
            + objectFileName("/someapp/narf.cpp", profileName());
    QDateTime unchangedObjectFileTime1 = QFileInfo(unchangedObjectFile).lastModified();

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../after", ".");
    touch("trackAddFile.qbs");
    touch("main.cpp");
    QCOMPARE(runQbs(runParams), 0);

    output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "ZORT!");

    // the object file of the untouched source should not have changed
    QDateTime unchangedObjectFileTime2 = QFileInfo(unchangedObjectFile).lastModified();
    QCOMPARE(unchangedObjectFileTime1, unchangedObjectFileTime2);
}

void TestBlackbox::trackExternalProductChanges()
{
    QDir::setCurrent(testDataDir + "/trackExternalProductChanges");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    QbsRunParameters params;
    params.environment.insert("QBS_TEST_PULL_IN_FILE_VIA_ENV", "1");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY2(!m_qbsStdout.contains("compiling jsFileChange.cpp"), m_qbsStdout.constData());
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    WAIT_FOR_NEW_TIMESTAMP();
    QFile jsFile("fileList.js");
    QVERIFY(jsFile.open(QIODevice::ReadWrite));
    QByteArray jsCode = jsFile.readAll();
    jsCode.replace("return []", "return ['jsFileChange.cpp']");
    jsFile.resize(0);
    jsFile.write(jsCode);
    jsFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    rmDirR(relativeBuildDir());
    QVERIFY(jsFile.open(QIODevice::ReadWrite));
    jsCode = jsFile.readAll();
    jsCode.replace("['jsFileChange.cpp']", "[]");
    jsFile.resize(0);
    jsFile.write(jsCode);
    jsFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    QFile cppFile("fileExists.cpp");
    QVERIFY(cppFile.open(QIODevice::WriteOnly));
    cppFile.write("void fileExists() { }\n");
    cppFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling fileExists.cpp"));

    rmDirR(relativeBuildDir());
    const SettingsPtr s = settings();
    const Profile profile(profileName(), s.get());
    const QStringList toolchainTypes = profile.value("qbs.toolchain").toStringList();
    if (!toolchainTypes.contains("gcc"))
        QSKIP("Need GCC-like compiler to run this test");
    params.environment = QProcessEnvironment::systemEnvironment();
    params.environment.insert("INCLUDE_PATH_TEST", "1");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("hiddenheaderqbs.h"), m_qbsStderr.constData());
    params.command = "resolve";
    params.environment.insert("CPLUS_INCLUDE_PATH",
                              QDir::toNativeSeparators(QDir::currentPath() + "/hidden"));
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::trackGroupConditionChange()
{
    QbsRunParameters params;
    params.expectFailure = true;
    QDir::setCurrent(testDataDir + "/group-condition-change");
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("jibbetnich"));

    params.command = "resolve";
    params.arguments = QStringList("project.kaputt:false");
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::trackRemoveFile()
{
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackAddFile");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    ccp("after", "work");
    QDir::setCurrent(testDataDir + "/trackAddFile/work");
    const QbsRunParameters runParams("run", QStringList{"-qp", "someapp"});
    QCOMPARE(runQbs(runParams), 0);
    output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "ZORT!");
    QString unchangedObjectFile = relativeBuildDir()
            + objectFileName("/someapp/narf.cpp", profileName());
    QDateTime unchangedObjectFileTime1 = QFileInfo(unchangedObjectFile).lastModified();

    WAIT_FOR_NEW_TIMESTAMP();
    QFile::remove("trackAddFile.qbs");
    QFile::remove("main.cpp");
    QFile::copy("../before/trackAddFile.qbs", "trackAddFile.qbs");
    QFile::copy("../before/main.cpp", "main.cpp");
    QVERIFY(QFile::remove("zort.h"));
    QVERIFY(QFile::remove("zort.cpp"));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("resolve"))), 0);

    touch("main.cpp");
    touch("trackAddFile.qbs");
    QCOMPARE(runQbs(runParams), 0);
    output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");

    // the object file of the untouched source should not have changed
    QDateTime unchangedObjectFileTime2 = QFileInfo(unchangedObjectFile).lastModified();
    QCOMPARE(unchangedObjectFileTime1, unchangedObjectFileTime2);

    // the object file for the removed cpp file should have vanished too
    QVERIFY(!regularFileExists(relativeBuildDir()
                               + objectFileName("/someapp/zort.cpp", profileName())));
}

void TestBlackbox::trackAddFileTag()
{
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackFileTags");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackFileTags/work");
    const QbsRunParameters runParams("run", QStringList{"-qp", "someapp"});
    QCOMPARE(runQbs(runParams), 0);
    output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's no foo here");

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../after", ".");
    touch("main.cpp");
    touch("trackFileTags.qbs");
    QCOMPARE(runQbs(runParams), 0);
    output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's 15 foo here");
}

void TestBlackbox::trackRemoveFileTag()
{
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackFileTags");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("after", "work");
    QDir::setCurrent(testDataDir + "/trackFileTags/work");
    const QbsRunParameters runParams("run", QStringList{"-qp", "someapp"});
    QCOMPARE(runQbs(runParams), 0);

    // check if the artifacts are here that will become stale in the 2nd step
    QVERIFY(regularFileExists(relativeProductBuildDir("someapp") + '/' + inputDirHash(".")
                              + objectFileName("/main_foo.cpp", profileName())));
    QVERIFY(regularFileExists(relativeProductBuildDir("someapp") + "/main_foo.cpp"));
    QVERIFY(regularFileExists(relativeProductBuildDir("someapp") + "/main.foo"));
    output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's 15 foo here");

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../before", ".");
    touch("main.cpp");
    touch("trackFileTags.qbs");
    QCOMPARE(runQbs(runParams), 0);
    output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's no foo here");

    // check if stale artifacts have been removed
    QCOMPARE(regularFileExists(relativeProductBuildDir("someapp") + '/' + inputDirHash(".")
                               + objectFileName("/main_foo.cpp", profileName())), false);
    QCOMPARE(regularFileExists(relativeProductBuildDir("someapp") + "/main_foo.cpp"), false);
    QCOMPARE(regularFileExists(relativeProductBuildDir("someapp") + "/main.foo"), false);
}

void TestBlackbox::trackAddProduct()
{
    QDir::setCurrent(testDataDir + "/trackProducts");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackProducts/work");
    QbsRunParameters params(QStringList() << "-f" << "trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product1"));
    QVERIFY(m_qbsStdout.contains("linking product2"));

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../after", ".");
    touch("trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling zoo.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product3"));
    QVERIFY(!m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product1"));
    QVERIFY(!m_qbsStdout.contains("linking product2"));
}

void TestBlackbox::trackRemoveProduct()
{
    QDir::setCurrent(testDataDir + "/trackProducts");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    ccp("after", "work");
    QDir::setCurrent(testDataDir + "/trackProducts/work");
    QbsRunParameters params(QStringList() << "-f" << "trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling zoo.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product1"));
    QVERIFY(m_qbsStdout.contains("linking product2"));
    QVERIFY(m_qbsStdout.contains("linking product3"));

    WAIT_FOR_NEW_TIMESTAMP();
    QFile::remove("zoo.cpp");
    QFile::remove("product3.qbs");
    copyFileAndUpdateTimestamp("../before/trackProducts.qbs", "trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zoo.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product1"));
    QVERIFY(!m_qbsStdout.contains("linking product2"));
    QVERIFY(!m_qbsStdout.contains("linking product3"));
}

void TestBlackbox::wildcardRenaming()
{
    QDir::setCurrent(testDataDir + "/wildcard_renaming");
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/pioniere.txt").exists());
    WAIT_FOR_NEW_TIMESTAMP();
    QFile::rename(QDir::currentPath() + "/pioniere.txt", QDir::currentPath() + "/fdj.txt");
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("install"),
                                     QStringList("--clean-install-root"))), 0);
    QVERIFY(!QFileInfo(defaultInstallRoot + "/pioniere.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/fdj.txt").exists());
}

void TestBlackbox::recursiveRenaming()
{
    QDir::setCurrent(testDataDir + "/recursive_renaming");
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/wasser.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/subdir/blubb.txt").exists());
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(QFile::rename(QDir::currentPath() + "/dir/wasser.txt", QDir::currentPath() + "/dir/wein.txt"));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("install"),
                                     QStringList("--clean-install-root"))), 0);
    QVERIFY(!QFileInfo(defaultInstallRoot + "/dir/wasser.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/wein.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/subdir/blubb.txt").exists());
}

void TestBlackbox::recursiveWildcards()
{
    QDir::setCurrent(testDataDir + "/recursive_wildcards");
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/file1.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/file2.txt").exists());
    QFile outputFile(defaultInstallRoot + "/output.txt");
    QVERIFY2(outputFile.open(QIODevice::ReadOnly), qPrintable(outputFile.errorString()));
    QCOMPARE(outputFile.readAll(), QByteArray("file1.txtfile2.txt"));
    outputFile.close();
    WAIT_FOR_NEW_TIMESTAMP();
    QFile newFile("dir/subdir/file3.txt");
    QVERIFY2(newFile.open(QIODevice::WriteOnly), qPrintable(newFile.errorString()));
    newFile.close();
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/file3.txt").exists());
    QVERIFY2(outputFile.open(QIODevice::ReadOnly), qPrintable(outputFile.errorString()));
    QCOMPARE(outputFile.readAll(), QByteArray("file1.txtfile2.txtfile3.txt"));
    outputFile.close();
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(newFile.remove(), qPrintable(newFile.errorString()));
    QVERIFY2(!newFile.exists(), qPrintable(newFile.fileName()));
    QCOMPARE(runQbs(QbsRunParameters("install", QStringList{ "--clean-install-root"})), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/file1.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/file2.txt").exists());
    QVERIFY(!QFileInfo(defaultInstallRoot + "/dir/file3.txt").exists());
    QVERIFY2(outputFile.open(QIODevice::ReadOnly), qPrintable(outputFile.errorString()));
    QCOMPARE(outputFile.readAll(), QByteArray("file1.txtfile2.txt"));
}

void TestBlackbox::referenceErrorInExport()
{
    QDir::setCurrent(testDataDir + "/referenceErrorInExport");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains(
        "referenceErrorInExport.qbs:17:12 ReferenceError: Can't find variable: includePaths"));
}

void TestBlackbox::reproducibleBuild()
{
    const SettingsPtr s = settings();
    const Profile profile(profileName(), s.get());
    const QStringList toolchains = profile.value("qbs.toolchain").toStringList();
    if (!toolchains.contains("gcc") || toolchains.contains("clang"))
        QSKIP("reproducible builds only supported for gcc");

    QFETCH(bool, reproducible);

    QDir::setCurrent(testDataDir + "/reproducible-build");
    QbsRunParameters params;
    params.arguments << QString("modules.cpp.enableReproducibleBuilds:")
                        + (reproducible ? "true" : "false");
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(params), 0);
    QFile object(relativeProductBuildDir("the product") + '/' + inputDirHash(".") + '/'
                 + objectFileName("file1.cpp", profileName()));
    QVERIFY2(object.open(QIODevice::ReadOnly), qPrintable(object.fileName()));
    const QByteArray oldContents = object.readAll();
    object.close();
    QCOMPARE(runQbs(QbsRunParameters("clean")), 0);
    QVERIFY(!object.exists());
    QCOMPARE(runQbs(params), 0);
    if (reproducible) {
        QVERIFY(object.open(QIODevice::ReadOnly));
        const QByteArray newContents = object.readAll();
        QVERIFY(oldContents == newContents);
        object.close();
    }
    QCOMPARE(runQbs(QbsRunParameters("clean")), 0);
}

void TestBlackbox::reproducibleBuild_data()
{
    QTest::addColumn<bool>("reproducible");
    QTest::newRow("non-reproducible build") << false;
    QTest::newRow("reproducible build") << true;
}

void TestBlackbox::responseFiles()
{
    QDir::setCurrent(testDataDir + "/response-files");
    QbsRunParameters params;
    params.command = "install";
    params.arguments << "--install-root" << "installed";
    QCOMPARE(runQbs(params), 0);
    QFile file("installed/response-file-content.txt");
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QList<QByteArray> expected = QList<QByteArray>()
            << "foo" << qbs::Internal::shellQuote(QStringLiteral("with space")).toUtf8()
            << "bar" << "";
    QList<QByteArray> lines = file.readAll().split('\n');
    for (auto &line : lines)
        line = line.trimmed();
    QCOMPARE(lines, expected);
}

void TestBlackbox::ruleConditions()
{
    QDir::setCurrent(testDataDir + "/ruleConditions");
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFileInfo(relativeExecutableFilePath("zorted")).exists());
    QVERIFY(QFileInfo(relativeExecutableFilePath("unzorted")).exists());
    QVERIFY(QFileInfo(relativeProductBuildDir("zorted") + "/zorted.foo.narf.zort").exists());
    QVERIFY(!QFileInfo(relativeProductBuildDir("unzorted") + "/unzorted.foo.narf.zort").exists());
}

void TestBlackbox::ruleCycle()
{
    QDir::setCurrent(testDataDir + "/ruleCycle");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("Cycle detected in rule dependencies"));
}

void TestBlackbox::ruleWithNoInputs()
{
    QDir::setCurrent(testDataDir + "/rule-with-no-inputs");
    QVERIFY2(runQbs() == 0, m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
    QVERIFY2(runQbs() == 0, m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
    QbsRunParameters params("resolve", QStringList() << "products.theProduct.version:1");
    QVERIFY2(runQbs(params) == 0, m_qbsStderr.constData());
    params.command = "build";
    QVERIFY2(runQbs(params) == 0, m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
    params.command = "resolve";
    params.arguments = QStringList() << "products.theProduct.version:2";
    QVERIFY2(runQbs(params) == 0, m_qbsStderr.constData());
    params.command = "build";
    QVERIFY2(runQbs(params) == 0, m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
}

void TestBlackbox::ruleWithNonRequiredInputs()
{
    QDir::setCurrent(testDataDir + "/rule-with-non-required-inputs");
    QbsRunParameters params("build", {"products.p.enableTagger:false"});
    QCOMPARE(runQbs(params), 0);
    QFile outFile(relativeProductBuildDir("p") + "/output.txt");
    QVERIFY2(outFile.open(QIODevice::ReadOnly), qPrintable(outFile.errorString()));
    QByteArray output = outFile.readAll();
    QCOMPARE(output, QByteArray("()"));
    outFile.close();
    params.command = "resolve";
    params.arguments = QStringList({"products.p.enableTagger:true"});
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(outFile.open(QIODevice::ReadOnly), qPrintable(outFile.errorString()));
    output = outFile.readAll();
    QCOMPARE(output, QByteArray("(a.inp,b.inp,c.inp,)"));
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("Generating"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("a.inp");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Generating"), m_qbsStdout.constData());
}

void TestBlackbox::setupBuildEnvironment()
{
    QDir::setCurrent(testDataDir + "/setup-build-environment");
    QCOMPARE(runQbs(), 0);
    QFile f(relativeProductBuildDir("first_product") + QLatin1String("/m.output"));
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.errorString()));
    QCOMPARE(f.readAll().trimmed(), QByteArray("1"));
    f.close();
    f.setFileName(relativeProductBuildDir("second_product") + QLatin1String("/m.output"));
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.errorString()));
    QCOMPARE(f.readAll().trimmed(), QByteArray());
}

void TestBlackbox::setupRunEnvironment()
{
    QDir::setCurrent(testDataDir + "/setup-run-environment");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    QbsRunParameters failParams("run", QStringList({"--setup-run-env-config",
                                                    "ignore-lib-dependencies"}));
    failParams.expectFailure = true;
    failParams.expectCrash = m_qbsStdout.contains("is windows");
    QVERIFY(runQbs(QbsRunParameters(failParams)) != 0);
    QVERIFY2(failParams.expectCrash || m_qbsStderr.contains("lib"), m_qbsStderr.constData());
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QbsRunParameters dryRunParams("run", QStringList("--dry-run"));
    dryRunParams.buildDirectory = "dryrun";
    QCOMPARE(runQbs(dryRunParams), 0);
    const QString appFilePath = QDir::currentPath() + "/dryrun/"
            + relativeExecutableFilePath("app");
    QVERIFY2(m_qbsStdout.contains("Would start target")
             && m_qbsStdout.contains(QDir::toNativeSeparators(appFilePath).toLocal8Bit()),
             m_qbsStdout.constData());
}

void TestBlackbox::smartRelinking()
{
    QDir::setCurrent(testDataDir + "/smart-relinking");
    rmDirR(relativeBuildDir());
    QFETCH(bool, strictChecking);
    QbsRunParameters params(QStringList() << (QString("modules.cpp.exportedSymbolsCheckMode:%1")
            .arg(strictChecking ? "strict" : "ignore-undefined")));
    QCOMPARE(runQbs(params), 0);
    if (m_qbsStdout.contains("project disabled"))
        QSKIP("Test does not apply on this target");
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Irrelevant change.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("lib.cpp");
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Add new private symbol.
    WAIT_FOR_NEW_TIMESTAMP();
    params.command = "resolve";
    params.arguments << "products.lib.defines:PRIV2";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Remove private symbol.
    WAIT_FOR_NEW_TIMESTAMP();
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Add new public symbol.
    WAIT_FOR_NEW_TIMESTAMP();
    params.arguments << "products.lib.defines:PUB2";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Remove public symbol.
    WAIT_FOR_NEW_TIMESTAMP();
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Add new undefined symbol.
    WAIT_FOR_NEW_TIMESTAMP();
    params.arguments << "products.lib.defines:PRINTF";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(strictChecking == m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Remove undefined symbol.
    WAIT_FOR_NEW_TIMESTAMP();
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(strictChecking == m_qbsStdout.contains("linking app"), m_qbsStdout.constData());
}

void TestBlackbox::smartRelinking_data()
{
    QTest::addColumn<bool>("strictChecking");
    QTest::newRow("strict checking") << true;
    QTest::newRow("ignore undefined") << false;
}


static QString soName(const QString &readElfPath, const QString &libFilePath)
{
    QProcess readElf;
    readElf.start(readElfPath, QStringList() << "-a" << libFilePath);
    if (!readElf.waitForStarted() || !readElf.waitForFinished() || readElf.exitCode() != 0) {
        qDebug() << readElf.errorString() << readElf.readAllStandardError();
        return QString();
    }
    const QByteArray output = readElf.readAllStandardOutput();
    const QByteArray magicString = "Library soname: [";
    const int magicStringIndex = output.indexOf(magicString);
    if (magicStringIndex == -1)
        return QString();
    const int endIndex = output.indexOf(']', magicStringIndex);
    if (endIndex == -1)
        return QString();
    const int nameIndex = magicStringIndex + magicString.size();
    const QByteArray theName = output.mid(nameIndex, endIndex - nameIndex);
    return QString::fromLatin1(theName);
}

void TestBlackbox::soVersion()
{
    const QString readElfPath = findExecutable(QStringList("readelf"));
    if (readElfPath.isEmpty() || readElfPath.endsWith("exe"))
        QSKIP("soversion test not applicable on this system");
    QDir::setCurrent(testDataDir + "/soversion");

    QFETCH(QString, soVersion);
    QFETCH(bool, useVersion);
    QFETCH(QString, expectedSoName);

    QbsRunParameters params;
    params.arguments << ("products.mylib.useVersion:" + QString((useVersion ? "true" : "false")));
    if (!soVersion.isNull())
        params.arguments << ("modules.cpp.soVersion:" + soVersion);
    const QString libFilePath = relativeProductBuildDir("mylib") + "/libmylib.so"
            + (useVersion ? ".1.2.3" : QString());
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(regularFileExists(libFilePath), qPrintable(libFilePath));
    QCOMPARE(soName(readElfPath, libFilePath), expectedSoName);
}

void TestBlackbox::soVersion_data()
{
    QTest::addColumn<QString>("soVersion");
    QTest::addColumn<bool>("useVersion");
    QTest::addColumn<QString>("expectedSoName");

    QTest::newRow("default") << QString() << true << QString("libmylib.so.1");
    QTest::newRow("explicit soVersion") << QString("1.2") << true << QString("libmylib.so.1.2");
    QTest::newRow("empty soVersion") << QString("") << true << QString("libmylib.so.1.2.3");
    QTest::newRow("no version, explicit soVersion") << QString("5") << false
                                                    << QString("libmylib.so.5");
    QTest::newRow("no version, default soVersion") << QString() << false << QString("libmylib.so");
    QTest::newRow("no version, empty soVersion") << QString("") << false << QString("libmylib.so");
}

void TestBlackbox::overrideProjectProperties()
{
    QDir::setCurrent(testDataDir + "/overrideProjectProperties");
    QCOMPARE(runQbs(QbsRunParameters(QStringList()
                                     << QLatin1String("-f")
                                     << QLatin1String("overrideProjectProperties.qbs")
                                     << QLatin1String("project.nameSuffix:ForYou")
                                     << QLatin1String("project.someBool:false")
                                     << QLatin1String("project.someInt:156")
                                     << QLatin1String("project.someStringList:one")
                                     << QLatin1String("products.MyAppForYou.mainFile:main.cpp"))),
             0);
    QVERIFY(regularFileExists(relativeExecutableFilePath("MyAppForYou")));
    QVERIFY(QFile::remove(relativeBuildGraphFilePath()));
    QbsRunParameters params;
    params.arguments << QLatin1String("-f") << QLatin1String("project_using_helper_lib.qbs");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);

    rmDirR(relativeBuildDir());
    params.arguments = QStringList() << QLatin1String("-f")
            << QLatin1String("project_using_helper_lib.qbs")
            << QLatin1String("project.linkSuccessfully:true");
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::pchChangeTracking()
{
    QDir::setCurrent(testDataDir + "/pch-change-tracking");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("precompiling pch.h (cpp)"));
    WAIT_FOR_NEW_TIMESTAMP();
    touch("header1.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("precompiling pch.h (cpp)"));
    QVERIFY(m_qbsStdout.contains("compiling header2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    WAIT_FOR_NEW_TIMESTAMP();
    touch("header2.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("precompiling pch.h (cpp)"), m_qbsStdout.constData());
}

void TestBlackbox::perGroupDefineInExportItem()
{
    QDir::setCurrent(testDataDir + "/per-group-define-in-export-item");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::pkgConfigProbe()
{
    const QString exe = findExecutable(QStringList() << "pkg-config");
    if (exe.isEmpty())
        QSKIP("This test requires the pkg-config tool");

    QDir::setCurrent(testDataDir + "/pkg-config-probe");

    QFETCH(QString, packageBaseName);
    QFETCH(QStringList, found);
    QFETCH(QStringList, libs);
    QFETCH(QStringList, cflags);
    QFETCH(QStringList, version);

    rmDirR(relativeBuildDir());
    QbsRunParameters params(QStringList() << ("project.packageBaseName:" + packageBaseName));
    QCOMPARE(runQbs(params), 0);
    const QString stdOut = m_qbsStdout;
    QVERIFY2(stdOut.contains("theProduct1 found: " + found.at(0)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct2 found: " + found.at(1)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct1 libs: " + libs.at(0)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct2 libs: " + libs.at(1)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct1 cflags: " + cflags.at(0)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct2 cflags: " + cflags.at(1)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct1 version: " + version.at(0)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct2 version: " + version.at(1)), m_qbsStdout.constData());
}

void TestBlackbox::pkgConfigProbe_data()
{
    QTest::addColumn<QString>("packageBaseName");
    QTest::addColumn<QStringList>("found");
    QTest::addColumn<QStringList>("libs");
    QTest::addColumn<QStringList>("cflags");
    QTest::addColumn<QStringList>("version");

    QTest::newRow("existing package")
            << "dummy" << (QStringList() << "true" << "true")
            << (QStringList() << "[\"-Ldummydir1\",\"-ldummy1\"]"
                << "[\"-Ldummydir2\",\"-ldummy2\"]")
            << (QStringList() << "[]" << "[]") << (QStringList() << "0.0.1" << "0.0.2");

    // Note: The array values should be "undefined", but we lose that information when
    //       converting to QVariants in the ProjectResolver.
    QTest::newRow("non-existing package")
            << "blubb" << (QStringList() << "false" << "false") << (QStringList() << "[]" << "[]")
            << (QStringList() << "[]" << "[]") << (QStringList() << "undefined" << "undefined");
}

void TestBlackbox::pkgConfigProbeSysroot()
{
    const QString exe = findExecutable(QStringList() << "pkg-config");
    if (exe.isEmpty())
        QSKIP("This test requires the pkg-config tool");

    QDir::setCurrent(testDataDir + "/pkg-config-probe-sysroot");
    QCOMPARE(runQbs(QStringList("-v")), 0);
    QCOMPARE(m_qbsStderr.count("PkgConfigProbe: found packages"), 2);
    const QString outputTemplate = "theProduct%1 libs: [\"-L%2/usr/dummy\",\"-ldummy1\"]";
    QVERIFY2(m_qbsStdout.contains(outputTemplate
                                  .arg("1", QDir::currentPath() + "/sysroot1").toLocal8Bit()),
             m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains(outputTemplate
                                  .arg("2", QDir::currentPath() + "/sysroot2").toLocal8Bit()),
             m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains(outputTemplate
                                  .arg("3", QDir::currentPath() + "/sysroot1").toLocal8Bit()),
             m_qbsStdout.constData());
}

void TestBlackbox::pluginDependency()
{
    QDir::setCurrent(testDataDir + "/plugin-dependency");

    // Build the plugins and the helper2 lib.
    QCOMPARE(runQbs(QStringList{"--products", "plugin1,plugin2,plugin3,plugin4,helper2"}), 0);
    QVERIFY(m_qbsStdout.contains("plugin1"));
    QVERIFY(m_qbsStdout.contains("plugin2"));
    QVERIFY(m_qbsStdout.contains("plugin3"));
    QVERIFY(m_qbsStdout.contains("plugin4"));
    QVERIFY(m_qbsStdout.contains("helper2"));
    QVERIFY(!m_qbsStderr.contains("SOFT ASSERT"));

    // Build the app. Plugins 1 and 2 must not be linked. Plugin 3 must be linked.
    QCOMPARE(runQbs(QStringList{"--command-echo-mode", "command-line"}), 0);
    QByteArray output = m_qbsStdout + '\n' + m_qbsStderr;
    QVERIFY(!output.contains("plugin1"));
    QVERIFY(!output.contains("plugin2"));
    QVERIFY(!output.contains("helper2"));

    // Check that the build dependency still works.
    QCOMPARE(runQbs(QStringLiteral("clean")), 0);
    QCOMPARE(runQbs(QStringList{"--products", "myapp", "--command-echo-mode", "command-line"}), 0);
    QVERIFY(m_qbsStdout.contains("plugin1"));
    QVERIFY(m_qbsStdout.contains("plugin2"));
    QVERIFY(m_qbsStdout.contains("plugin3"));
    QVERIFY(m_qbsStdout.contains("plugin4"));
}

void TestBlackbox::precompiledAndPrefixHeaders()
{
    QDir::setCurrent(testDataDir + "/precompiled-and-prefix-headers");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::probeChangeTracking()
{
    QDir::setCurrent(testDataDir + "/probe-change-tracking");

    // Product probe disabled, other probes enabled.
    QbsRunParameters params;
    params.command = "resolve";
    params.arguments = QStringList("products.theProduct.runProbe:false");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(m_qbsStdout.contains("running subProbe"));
    QVERIFY(!m_qbsStdout.contains("running productProbe"));

    // Product probe newly enabled.
    params.arguments = QStringList("products.theProduct.runProbe:true");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(!m_qbsStdout.contains("running subProbe"));
    QVERIFY(m_qbsStdout.contains("running productProbe: 12"));

    // Re-resolving with unchanged probe.
    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile("probe-change-tracking.qbs");
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray content = projectFile.readAll();
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(!m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(!m_qbsStdout.contains("running subProbe"));
    QVERIFY(!m_qbsStdout.contains("running productProbe"));

    // Re-resolving with changed configure scripts.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    content = projectFile.readAll();
    content.replace("console.info", " console.info");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(m_qbsStdout.contains("running subProbe"));
    QVERIFY(m_qbsStdout.contains("running productProbe: 12"));

    // Re-resolving with added property.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    content = projectFile.readAll();
    content.replace("condition: product.runProbe",
                    "condition: product.runProbe\nproperty string something: 'x'");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(!m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(!m_qbsStdout.contains("running subProbe"));
    QVERIFY(m_qbsStdout.contains("running productProbe: 12"));

    // Re-resolving with changed property.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    content = projectFile.readAll();
    content.replace("'x'", "'y'");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(!m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(!m_qbsStdout.contains("running subProbe"));
    QVERIFY(m_qbsStdout.contains("running productProbe: 12"));

    // Re-resolving with removed property.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    content = projectFile.readAll();
    content.replace("property string something: 'y'", "");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(!m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(!m_qbsStdout.contains("running subProbe"));
    QVERIFY(m_qbsStdout.contains("running productProbe: 12"));

    // Re-resolving with unchanged probe again.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    content = projectFile.readAll();
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(!m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(!m_qbsStdout.contains("running subProbe"));
    QVERIFY(!m_qbsStdout.contains("running productProbe"));

    // Enforcing re-running via command-line option.
    params.arguments.prepend("--force-probe-execution");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(m_qbsStdout.contains("running subProbe"));
    QVERIFY(m_qbsStdout.contains("running productProbe: 12"));
}

void TestBlackbox::probeProperties()
{
    QDir::setCurrent(testDataDir + "/probeProperties");
    const QByteArray dir = QDir::cleanPath(testDataDir).toLatin1() + "/probeProperties";
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("probe1.fileName=bin/tool"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe1.path=" + dir), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe1.filePath=" + dir + "/bin/tool"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe2.fileName=tool"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe2.path=" + dir + "/bin"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe2.filePath=" + dir + "/bin/tool"), m_qbsStdout.constData());
}

void TestBlackbox::probeInExportedModule()
{
    QDir::setCurrent(testDataDir + "/probe-in-exported-module");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << QLatin1String("-f")
                                     << QLatin1String("probe-in-exported-module.qbs"))), 0);
    QVERIFY2(m_qbsStdout.contains("found: true"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("prop: yes"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("listProp: my,myother"), m_qbsStdout.constData());
}

void TestBlackbox::probesAndArrayProperties()
{
    QDir::setCurrent(testDataDir + "/probes-and-array-properties");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("prop: [\"probe\"]"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile("probes-and-array-properties.qbs");
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray content = projectFile.readAll();
    content.replace("//", "");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("prop: [\"product\",\"probe\"]"), m_qbsStdout.constData());
}

void TestBlackbox::productProperties()
{
    QDir::setCurrent(testDataDir + "/productproperties");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << QLatin1String("-f")
                                     << QLatin1String("productproperties.qbs"))), 0);
    QVERIFY(regularFileExists(relativeExecutableFilePath("blubb_user")));
}

void TestBlackbox::propertyAssignmentOnNonPresentModule()
{
    QDir::setCurrent(testDataDir + "/property-assignment-on-non-present-module");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
}

void TestBlackbox::propertyAssignmentInFailedModule()
{
    QDir::setCurrent(testDataDir + "/property-assignment-in-failed-module");
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("modules.m.doFail:false"))), 0);
    QbsRunParameters failParams;
    failParams.expectFailure = true;
    QVERIFY(runQbs(failParams) != 0);
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("modules.m.doFail:true"))), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QEXPECT_FAIL(0, "circular dependency between module merging and validation", Continue);
    QCOMPARE(runQbs(failParams), 0);
}

void TestBlackbox::propertyChanges()
{
    QDir::setCurrent(testDataDir + "/propertyChanges");
    QFile projectFile("propertyChanges.qbs");
    QbsRunParameters params(QStringList({"-f", "propertyChanges.qbs", "qbs.enableDebugCode:true"}));

    // Initial build.
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.debug"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(m_qbsStdout.contains("Making output from input"));
    QVERIFY(m_qbsStdout.contains("default value"));
    QVERIFY(m_qbsStdout.contains("Making output from other output"));
    QFile generatedFile(relativeProductBuildDir("generated text file") + "/generated.txt");
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 1contents 1suffix 1"));
    generatedFile.close();

    // Incremental build with no changes.
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build with no changes, but updated project file timestamp.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite | QIODevice::Append));
    projectFile.write("\n");
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, input property changed for first product
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray contents = projectFile.readAll();
    contents.replace("blubb1", "blubb01");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.debug"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("linking library"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, input property changed via project for second product.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("blubb2", "blubb02");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, input property changed via command line for second product.
    params.command = "resolve";
    params.arguments << "project.projectDefines:blubb002";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, input property changed via environment for third product.
    params.environment.insert("QBS_BLACKBOX_DEFINE", "newvalue");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.environment.remove("QBS_BLACKBOX_DEFINE");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));
    params.environment.insert("QBS_BLACKBOX_DEFINE", "newvalue");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.environment.remove("QBS_BLACKBOX_DEFINE");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, module property changed via command line.
    params.arguments << "qbs.enableDebugCode:false";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.release"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.debug"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, non-essential dependency removed.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("Depends { name: 'library' }", "// Depends { name: 'library' }");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("linking product 2"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("linking library"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, prepare script of a transformer changed.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("contents 1", "contents 2");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 1contents 2suffix 1"));
    generatedFile.close();

    // Incremental build, product property used in JavaScript command changed.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("prefix 1", "prefix 2");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 2contents 2suffix 1"));
    generatedFile.close();

    // Incremental build, project property used in JavaScript command changed.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("suffix 1", "suffix 2");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 2contents 2suffix 2"));
    generatedFile.close();

    // Incremental build, module property used in JavaScript command changed.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("default value", "new value");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(m_qbsStdout.contains("Making output from input"));
    QVERIFY(m_qbsStdout.contains("Making output from other output"));
    QVERIFY(m_qbsStdout.contains("new value"));

    // Incremental build, prepare script of a rule in a module changed.
    WAIT_FOR_NEW_TIMESTAMP();
    QFile moduleFile("modules/TestModule/module.qbs");
    QVERIFY(moduleFile.open(QIODevice::ReadWrite));
    contents = moduleFile.readAll();
    contents.replace("// console.info('Change in source code')",
                     "console.info('Change in source code')");
    moduleFile.resize(0);
    moduleFile.write(contents);
    moduleFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(m_qbsStdout.contains("Making output from input"));
    QVERIFY(m_qbsStdout.contains("Making output from other output"));
}

void TestBlackbox::qtBug51237()
{
    const QString profileName = "profile-qtBug51237";
    const QString propertyName = "mymodule.theProperty";
    {
        const SettingsPtr s = settings();
        Profile profile(profileName, s.get());
        profile.setValue(propertyName, QStringList());
    }
    QDir::setCurrent(testDataDir + "/QTBUG-51237");
    QbsRunParameters params;
    params.profile = profileName;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::dynamicMultiplexRule()
{
    const QString testDir = testDataDir + "/dynamicMultiplexRule";
    QDir::setCurrent(testDir);
    QCOMPARE(runQbs(), 0);
    const QString outputFilePath = relativeProductBuildDir("dynamicMultiplexRule") + "/stuff-from-3-inputs";
    QVERIFY(regularFileExists(outputFilePath));
    WAIT_FOR_NEW_TIMESTAMP();
    touch("two.txt");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(outputFilePath));
}

void TestBlackbox::dynamicProject()
{
    const QString testDir = testDataDir + "/dynamic-project";
    QDir::setCurrent(testDir);
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.count("compiling main.cpp"), 2);
}

void TestBlackbox::dynamicRuleOutputs()
{
    const QString testDir = testDataDir + "/dynamicRuleOutputs";
    QDir::setCurrent(testDir);
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDir + "/work");
    QCOMPARE(runQbs(), 0);

    const QString appFile = relativeExecutableFilePath("genlexer");
    const QString headerFile1 = relativeProductBuildDir("genlexer") + "/GeneratedFiles/numberscanner.h";
    const QString sourceFile1 = relativeProductBuildDir("genlexer") + "/GeneratedFiles/numberscanner.c";
    const QString sourceFile2 = relativeProductBuildDir("genlexer") + "/GeneratedFiles/lex.yy.c";

    // Check build #1: source and header file name are specified in numbers.l
    QVERIFY(regularFileExists(appFile));
    QVERIFY(regularFileExists(headerFile1));
    QVERIFY(regularFileExists(sourceFile1));
    QVERIFY(!QFile::exists(sourceFile2));

    QDateTime appFileTimeStamp1 = QFileInfo(appFile).lastModified();
    WAIT_FOR_NEW_TIMESTAMP();
    copyFileAndUpdateTimestamp("../after/numbers.l", "numbers.l");
    QCOMPARE(runQbs(), 0);

    // Check build #2: no file names are specified in numbers.l
    //                 flex will default to lex.yy.c without header file.
    QDateTime appFileTimeStamp2 = QFileInfo(appFile).lastModified();
    QVERIFY(appFileTimeStamp1 < appFileTimeStamp2);
    QVERIFY(!QFile::exists(headerFile1));
    QVERIFY(!QFile::exists(sourceFile1));
    QVERIFY(regularFileExists(sourceFile2));

    WAIT_FOR_NEW_TIMESTAMP();
    copyFileAndUpdateTimestamp("../before/numbers.l", "numbers.l");
    QCOMPARE(runQbs(), 0);

    // Check build #3: source and header file name are specified in numbers.l
    QDateTime appFileTimeStamp3 = QFileInfo(appFile).lastModified();
    QVERIFY(appFileTimeStamp2 < appFileTimeStamp3);
    QVERIFY(regularFileExists(appFile));
    QVERIFY(regularFileExists(headerFile1));
    QVERIFY(regularFileExists(sourceFile1));
    QVERIFY(!QFile::exists(sourceFile2));
}

void TestBlackbox::erroneousFiles_data()
{
    QTest::addColumn<QString>("errorMessage");
    QTest::newRow("nonexistentWorkingDir")
            << "The working directory '.does.not.exist' for process '.*ls.*' is invalid.";
    QTest::newRow("outputArtifacts-missing-filePath")
            << "Error in Rule\\.outputArtifacts\\[0\\]\n\r?"
               "Property filePath must be a non-empty string\\.";
    QTest::newRow("outputArtifacts-missing-fileTags")
            << "Error in Rule\\.outputArtifacts\\[0\\]\n\r?"
               "Property fileTags for artifact 'outputArtifacts-missing-fileTags\\.txt' "
               "must be a non-empty string list\\.";
}

void TestBlackbox::erroneousFiles()
{
    QFETCH(QString, errorMessage);
    QDir::setCurrent(testDataDir + "/erroneous/" + QTest::currentDataTag());
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QString err = QString::fromLocal8Bit(m_qbsStderr);
    if (!err.contains(QRegExp(errorMessage))) {
        qDebug() << "Output:  " << err;
        qDebug() << "Expected: " << errorMessage;
        QFAIL("Unexpected error message.");
    }
}

void TestBlackbox::errorInfo()
{
    QDir::setCurrent(testDataDir + "/error-info");
    QCOMPARE(runQbs(), 0);

    QbsRunParameters resolveParams;
    QbsRunParameters buildParams;
    buildParams.expectFailure = true;

    resolveParams.command = "resolve";
    resolveParams.arguments = QStringList() << "project.fail1:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("error-info.qbs:25"), m_qbsStderr);

    resolveParams.arguments = QStringList() << "project.fail2:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("error-info.qbs:37"), m_qbsStderr);

    resolveParams.arguments = QStringList() << "project.fail3:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("error-info.qbs:52"), m_qbsStderr);

    resolveParams.arguments = QStringList() << "project.fail4:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("error-info.qbs:67"), m_qbsStderr);

    resolveParams.arguments = QStringList() << "project.fail5:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("helper.js:4"), m_qbsStderr);

    resolveParams.arguments = QStringList() << "project.fail6:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("helper.js:8"), m_qbsStderr);

    resolveParams.arguments = QStringList() << "project.fail7:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("JavaScriptCommand.sourceCode"), m_qbsStderr);
    QVERIFY2(m_qbsStderr.contains("error-info.qbs:58"), m_qbsStderr);
}

void TestBlackbox::escapedLinkerFlags()
{
    const SettingsPtr s = settings();
    const Profile buildProfile(profileName(), s.get());
    const QStringList toolchain = buildProfile.value("qbs.toolchain").toStringList();
    if (!toolchain.contains("gcc") || targetOs() == HostOsInfo::HostOsMacos)
        QSKIP("escaped linker flags test only applies with gcc and GNU ld");
    QDir::setCurrent(testDataDir + "/escaped-linker-flags");
    QbsRunParameters params(QStringList("products.app.escapeLinkerFlags:false"));
    QCOMPARE(runQbs(params), 0);
    params.command = "resolve";
    params.arguments = QStringList() << "products.app.escapeLinkerFlags:true";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Encountered escaped linker flag"), m_qbsStderr.constData());
}

void TestBlackbox::exportedDependencyInDisabledProduct()
{
    QDir::setCurrent(testDataDir + "/exported-dependency-in-disabled-product");
    QFETCH(QString, depCondition);
    QFETCH(bool, compileExpected);
    rmDirR(relativeBuildDir());
    const QString propertyArg = "products.dep.conditionString:" + depCondition;
    QCOMPARE(runQbs(QStringList(propertyArg)), 0);
    QEXPECT_FAIL("dependency directly disabled", "QBS-1250", Continue);
    QEXPECT_FAIL("dependency disabled via non-present module", "QBS-1250", Continue);
    QEXPECT_FAIL("dependency disabled via failed module", "QBS-1250", Continue);
    QCOMPARE(m_qbsStdout.contains("compiling"), compileExpected);
}

void TestBlackbox::exportedDependencyInDisabledProduct_data()
{
    QTest::addColumn<QString>("depCondition");
    QTest::addColumn<bool>("compileExpected");
    QTest::newRow("dependency enabled") << "true" << true;
    QTest::newRow("dependency directly disabled") << "false" << false;
    QTest::newRow("dependency disabled via non-present module") << "nosuchmodule.present" << false;
    QTest::newRow("dependency disabled via failed module") << "broken.present" << false;
}

void TestBlackbox::exportedPropertyInDisabledProduct()
{
    QDir::setCurrent(testDataDir + "/exported-property-in-disabled-product");
    QFETCH(QString, depCondition);
    QFETCH(bool, successExpected);
    const QString propertyArg = "products.dep.conditionString:" + depCondition;
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList(propertyArg))), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QbsRunParameters buildParams;
    buildParams.expectFailure = !successExpected;
    QCOMPARE(runQbs(buildParams) == 0, successExpected);
}

void TestBlackbox::exportedPropertyInDisabledProduct_data()
{
    QTest::addColumn<QString>("depCondition");
    QTest::addColumn<bool>("successExpected");
    QTest::newRow("dependency enabled") << "true" << false;
    QTest::newRow("dependency directly disabled") << "false" << true;
    QTest::newRow("dependency disabled via non-present module") << "nosuchmodule.present" << true;
    QTest::newRow("dependency disabled via failed module") << "broken.present" << true;
}

void TestBlackbox::systemRunPaths()
{
    const SettingsPtr s = settings();
    const Profile buildProfile(profileName(), s.get());
    switch (targetOs()) {
    case HostOsInfo::HostOsLinux:
    case HostOsInfo::HostOsMacos:
    case HostOsInfo::HostOsOtherUnix:
        break;
    default:
        QSKIP("only applies on Unix");
    }

    const QString lddFilePath = findExecutable(QStringList() << "ldd");
    if (lddFilePath.isEmpty())
        QSKIP("ldd not found");
    QDir::setCurrent(testDataDir + "/system-run-paths");
    QFETCH(bool, setRunPaths);
    rmDirR(relativeBuildDir());
    QbsRunParameters params;
    params.arguments << QString("project.setRunPaths:") + (setRunPaths ? "true" : "false");
    QCOMPARE(runQbs(params), 0);
    QProcess ldd;
    ldd.start(lddFilePath, QStringList() << relativeExecutableFilePath("app"));
    QVERIFY2(ldd.waitForStarted(), qPrintable(ldd.errorString()));
    QVERIFY2(ldd.waitForFinished(), qPrintable(ldd.errorString()));
    QVERIFY2(ldd.exitCode() == 0, ldd.readAllStandardError().constData());
    const QByteArray output = ldd.readAllStandardOutput();
    const QList<QByteArray> outputLines = output.split('\n');
    QByteArray libLine;
    for (const auto &line : outputLines) {
        if (line.contains("theLib")) {
            libLine = line;
            break;
        }
    }
    QVERIFY2(!libLine.isEmpty(), output.constData());
    QVERIFY2(setRunPaths == libLine.contains("not found"), libLine.constData());
}

void TestBlackbox::systemRunPaths_data()
{
    QTest::addColumn<bool>("setRunPaths");
    QTest::newRow("do not set system run paths") << false;
    QTest::newRow("do set system run paths") << true;
}

void TestBlackbox::exportRule()
{
    QDir::setCurrent(testDataDir + "/export-rule");
    QbsRunParameters params(QStringList{"modules.blubber.enableTagger:false"});
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    params.command = "resolve";
    params.arguments = QStringList{"modules.blubber.enableTagger:true"};
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Creating C++ source file"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("compiling myapp.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::exportToOutsideSearchPath()
{
    QDir::setCurrent(testDataDir + "/export-to-outside-searchpath");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Dependency 'aModule' not found for product 'theProduct'."),
             m_qbsStderr.constData());
}

void TestBlackbox::externalLibs()
{
    QDir::setCurrent(testDataDir + "/external-libs");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::fileDependencies()
{
    QDir::setCurrent(testDataDir + "/fileDependencies");
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling zort.cpp"));
    const QString productFileName = relativeExecutableFilePath("myapp");
    QVERIFY2(regularFileExists(productFileName), qPrintable(productFileName));

    // Incremental build without changes.
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling"));
    QVERIFY(!m_qbsStdout.contains("linking"));

    // Incremental build with changed file dependency.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("awesomelib/awesome.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zort.cpp"));

    // Incremental build with changed 2nd level file dependency.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("awesomelib/magnificent.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zort.cpp"));

    // Change the product in between to force the list of dependencies to get rescued.
    QFile projectFile("fileDependencies.qbs");
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray contents = projectFile.readAll();
    contents.replace("//", "");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY(!m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zort.cpp"));
    WAIT_FOR_NEW_TIMESTAMP();
    touch("awesomelib/magnificent.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zort.cpp"));
}

void TestBlackbox::installedTransformerOutput()
{
    QDir::setCurrent(testDataDir + "/installed-transformer-output");
    QCOMPARE(runQbs(), 0);
    const QString installedFilePath = defaultInstallRoot + "/textfiles/HelloWorld.txt";
    QVERIFY2(QFile::exists(installedFilePath), qPrintable(installedFilePath));
}

void TestBlackbox::inputsFromDependencies()
{
    QDir::setCurrent(testDataDir + "/inputs-from-dependencies");
    QCOMPARE(runQbs(), 0);
    const QList<QByteArray> output = m_qbsStdout.trimmed().split('\n');
    QVERIFY2(output.contains((QDir::currentPath() + "/file1.txt").toUtf8()),
             m_qbsStdout.constData());
    QVERIFY2(output.contains((QDir::currentPath() + "/file2.txt").toUtf8()),
             m_qbsStdout.constData());
    QVERIFY2(output.contains((QDir::currentPath() + "/file3.txt").toUtf8()),
             m_qbsStdout.constData());
    QVERIFY2(!output.contains((QDir::currentPath() + "/file4.txt").toUtf8()),
             m_qbsStdout.constData());
}

void TestBlackbox::installPackage()
{
    if (HostOsInfo::hostOs() == HostOsInfo::HostOsWindows)
        QSKIP("Beware of the msys tar");
    QString binary = findArchiver("tar");
    if (binary.isEmpty())
        QSKIP("tar not found");
    MacosTarHealer tarHealer;
    QDir::setCurrent(testDataDir + "/installpackage");
    QCOMPARE(runQbs(), 0);
    const QString tarFilePath = relativeProductBuildDir("tar-package") + "/tar-package.tar.gz";
    QVERIFY2(regularFileExists(tarFilePath), qPrintable(tarFilePath));
    QProcess tarList;
    tarList.start(binary, QStringList() << "tf" << tarFilePath);
    QVERIFY2(tarList.waitForStarted(), qPrintable(tarList.errorString()));
    QVERIFY2(tarList.waitForFinished(), qPrintable(tarList.errorString()));
    const QList<QByteArray> outputLines = tarList.readAllStandardOutput().split('\n');
    QList<QByteArray> cleanOutputLines;
    for (const QByteArray &line : outputLines) {
        const QByteArray trimmedLine = line.trimmed();
        if (!trimmedLine.isEmpty())
            cleanOutputLines.push_back(trimmedLine);
    }
    QCOMPARE(cleanOutputLines.size(), 3);
    for (const QByteArray &line : qAsConst(cleanOutputLines)) {
        QVERIFY2(line.contains("public_tool") || line.contains("mylib") || line.contains("lib.h"),
                 line.constData());
    }
}

void TestBlackbox::installRootFromProjectFile()
{
    QDir::setCurrent(testDataDir + "/install-root-from-project-file");
    const QString installRoot = QDir::currentPath() + '/' + relativeBuildDir()
            + "/my-install-root/";
    QCOMPARE(runQbs(QbsRunParameters(QStringList("products.p.installRoot:" + installRoot))), 0);
    const QString installedFile = installRoot + "/install-prefix/install-dir/file.txt";
    QVERIFY2(QFile::exists(installedFile), qPrintable(installedFile));
}

void TestBlackbox::installable()
{
    QDir::setCurrent(testDataDir + "/installable");
    QCOMPARE(runQbs(), 0);
    QFile installList(relativeProductBuildDir("install-list") + "/installed-files.txt");
    QVERIFY2(installList.open(QIODevice::ReadOnly), qPrintable(installList.errorString()));
    QCOMPARE(installList.readAll().count('\n'), 2);
}

void TestBlackbox::installableAsAuxiliaryInput()
{
    QDir::setCurrent(testDataDir + "/installable-as-auxiliary-input");
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(m_qbsStdout.contains("f-impl"), m_qbsStdout.constData());
}

void TestBlackbox::installTree()
{
    QDir::setCurrent(testDataDir + "/install-tree");
    QbsRunParameters params;
    params.command = "install";
    QCOMPARE(runQbs(params), 0);
    const QString installRoot = relativeBuildDir() + "/install-root/";
    QVERIFY(QFile::exists(installRoot + "content/foo.txt"));
    QVERIFY(QFile::exists(installRoot + "content/subdir1/bar.txt"));
    QVERIFY(QFile::exists(installRoot + "content/subdir2/baz.txt"));
}

void TestBlackbox::invalidCommandProperty()
{
    QDir::setCurrent(testDataDir + "/invalid-command-property");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("unsuitable"), m_qbsStderr.constData());
}

void TestBlackbox::invalidLibraryNames()
{
    QDir::setCurrent(testDataDir + "/invalid-library-names");
    QFETCH(QString, index);
    QFETCH(bool, success);
    QFETCH(QStringList, diagnostics);
    QbsRunParameters params(QStringList("project.valueIndex:" + index));
    params.expectFailure = !success;
    QCOMPARE(runQbs(params) == 0, success);
    for (const QString &diag : qAsConst(diagnostics))
        QVERIFY2(m_qbsStderr.contains(diag.toLocal8Bit()), m_qbsStderr.constData());
}

void TestBlackbox::invalidLibraryNames_data()
{
    QTest::addColumn<QString>("index");
    QTest::addColumn<bool>("success");
    QTest::addColumn<QStringList>("diagnostics");

    QTest::newRow("null") << "0" << false << QStringList("is null");
    QTest::newRow("undefined") << "1" << false << QStringList("is undefined");
    QTest::newRow("number") << "2" << false << QStringList("does not have string type");
    QTest::newRow("array") << "3" << false << QStringList("does not have string type");
    QTest::newRow("empty string") << "4" << true << (QStringList()
                                  << "WARNING: Removing empty string from value of property "
                                     "'cpp.dynamicLibraries' in product 'invalid-library-names'."
                                  << "WARNING: Removing empty string from value of property "
                                     "'cpp.staticLibraries' in product 'invalid-library-names'.");
}

void TestBlackbox::invalidExtensionInstantiation()
{
    rmDirR(relativeBuildDir());
    QDir::setCurrent(testDataDir + "/invalid-extension-instantiation");
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << (QString("products.theProduct.extension:") + QTest::currentDataTag());
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("invalid-extension-instantiation.qbs:18")
             && m_qbsStderr.contains('\'' + QByteArray(QTest::currentDataTag())
                                     + "' cannot be instantiated"),
             m_qbsStderr.constData());
}

void TestBlackbox::invalidExtensionInstantiation_data()
{
    QTest::addColumn<bool>("dummy");

    QTest::newRow("Environment");
    QTest::newRow("File");
    QTest::newRow("FileInfo");
    QTest::newRow("Utilities");
}

void TestBlackbox::invalidInstallDir()
{
    QDir::setCurrent(testDataDir + "/invalid-install-dir");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("outside of install root"), m_qbsStderr.constData());
}

void TestBlackbox::cli()
{
    int status;
    findCli(&status);
    QCOMPARE(status, 0);

    const SettingsPtr s = settings();
    Profile p("qbs_autotests-cli", s.get());
    const QStringList toolchain = p.value("qbs.toolchain").toStringList();
    if (!p.exists() || !(toolchain.contains("dotnet") || toolchain.contains("mono")))
        QSKIP("No suitable Common Language Infrastructure test profile");

    QDir::setCurrent(testDataDir + "/cli");
    QbsRunParameters params(QStringList() << "-f" << "dotnettest.qbs");
    params.profile = p.name();

    status = runQbs(params);
    if (p.value("cli.toolchainInstallPath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("toolchainInstallPath"))
        QSKIP("cli.toolchainInstallPath not set and automatic detection failed");

    QCOMPARE(status, 0);
    rmDirR(relativeBuildDir());

    QbsRunParameters params2(QStringList() << "-f" << "fshello.qbs");
    params2.profile = p.name();
    QCOMPARE(runQbs(params2), 0);
    rmDirR(relativeBuildDir());
}

void TestBlackbox::combinedSources()
{
    QDir::setCurrent(testDataDir + "/combined-sources");
    QbsRunParameters params(QStringList("modules.cpp.combineCxxSources:false"));
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling combinable.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling uncombinable.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling amalgamated_theapp.cpp"));
    params.arguments = QStringList("modules.cpp.combineCxxSources:true");
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    touch("combinable.cpp");
    touch("main.cpp");
    touch("uncombinable.cpp");
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling combinable.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling uncombinable.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling amalgamated_theapp.cpp"));
}

void TestBlackbox::commandFile()
{
    QDir::setCurrent(testDataDir + "/command-file");
    QbsRunParameters params(QStringList() << "-p" << "theLib");
    QCOMPARE(runQbs(params), 0);
    params.arguments = QStringList() << "-p" << "theApp";
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::compilerDefinesByLanguage()
{
    QDir::setCurrent(testDataDir + "/compilerDefinesByLanguage");
    QbsRunParameters params(QStringList { "-f", "compilerDefinesByLanguage.qbs" });
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::jsExtensionsFile()
{
    QDir::setCurrent(testDataDir + "/jsextensions-file");
    QFile fileToMove("tomove.txt");
    QVERIFY2(fileToMove.open(QIODevice::WriteOnly), qPrintable(fileToMove.errorString()));
    fileToMove.close();
    fileToMove.setPermissions(fileToMove.permissions() & ~(QFile::ReadUser | QFile::ReadOwner
                                                           | QFile::ReadGroup | QFile::ReadOther));
    QbsRunParameters params(QStringList() << "-f" << "file.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!QFileInfo("original.txt").exists());
    QFile copy("copy.txt");
    QVERIFY(copy.exists());
    QVERIFY(copy.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = copy.readAll().trimmed().split('\n');
    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines.at(0).trimmed().constData(), "false");
    QCOMPARE(lines.at(1).trimmed().constData(), "true");
}

void TestBlackbox::jsExtensionsFileInfo()
{
    QDir::setCurrent(testDataDir + "/jsextensions-fileinfo");
    QbsRunParameters params(QStringList() << "-f" << "fileinfo.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile output("output.txt");
    QVERIFY(output.exists());
    QVERIFY(output.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = output.readAll().trimmed().split('\n');
    QCOMPARE(lines.size(), 25);
    int i = 0;
    QCOMPARE(lines.at(i++).trimmed().constData(), "blubb");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/usr/bin");
    QCOMPARE(lines.at(i++).trimmed().constData(), "blubb.tar");
    QCOMPARE(lines.at(i++).trimmed().constData(), "blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "c:/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "true");
    QCOMPARE(lines.at(i++).trimmed().constData(), HostOsInfo::isWindowsHost() ? "true" : "false");
    QCOMPARE(lines.at(i++).trimmed().constData(), "false");
    QCOMPARE(lines.at(i++).trimmed().constData(), "true");
    QCOMPARE(lines.at(i++).trimmed().constData(), "false");
    QCOMPARE(lines.at(i++).trimmed().constData(), "false");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/");
    QCOMPARE(lines.at(i++).trimmed().constData(), HostOsInfo::isWindowsHost() ? "d:/" : "d:");
    QCOMPARE(lines.at(i++).trimmed().constData(), "d:");
    QCOMPARE(lines.at(i++).trimmed().constData(), "d:/");
    QCOMPARE(lines.at(i++).trimmed().constData(), "blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "../blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "\\tmp\\blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "c:\\tmp\\blubb.tar.gz");
}

void TestBlackbox::jsExtensionsProcess()
{
    QDir::setCurrent(testDataDir + "/jsextensions-process");
    QbsRunParameters params(QStringList() << "-f" << "process.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile output("output.txt");
    QVERIFY(output.exists());
    QVERIFY(output.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = output.readAll().trimmed().split('\n');
    QCOMPARE(lines.size(), 8);
    QCOMPARE(lines.at(0).trimmed().constData(), "0");
    QVERIFY(lines.at(1).startsWith("qbs "));
    QCOMPARE(lines.at(2).trimmed().constData(), "true");
    QCOMPARE(lines.at(3).trimmed().constData(), "true");
    QCOMPARE(lines.at(4).trimmed().constData(), "0");
    QVERIFY(lines.at(5).startsWith("qbs "));
    QCOMPARE(lines.at(6).trimmed().constData(), "false");
    QCOMPARE(lines.at(7).trimmed().constData(), "should be");
}

void TestBlackbox::jsExtensionsPropertyList()
{
    if (!HostOsInfo::isMacosHost())
        QSKIP("temporarily only applies on macOS");

    QDir::setCurrent(testDataDir + "/jsextensions-propertylist");
    QbsRunParameters params(QStringList() << "-nf" << "propertylist.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile file1("test.json");
    QVERIFY(file1.exists());
    QVERIFY(file1.open(QIODevice::ReadOnly));
    QFile file2("test.xml");
    QVERIFY(file2.exists());
    QVERIFY(file2.open(QIODevice::ReadOnly));
    QFile file3("test2.json");
    QVERIFY(file3.exists());
    QVERIFY(file3.open(QIODevice::ReadOnly));
    QByteArray file1Contents = file1.readAll();
    QCOMPARE(file3.readAll(), file1Contents);
    //QCOMPARE(file1Contents, file2.readAll()); // keys don't have guaranteed order
    QJsonParseError err1, err2;
    QCOMPARE(QJsonDocument::fromJson(file1Contents, &err1),
             QJsonDocument::fromJson(file2.readAll(), &err2));
    QVERIFY(err1.error == QJsonParseError::NoError && err2.error == QJsonParseError::NoError);
    QFile file4("test.openstep.plist");
    QVERIFY(file4.exists());
    QFile file5("test3.json");
    QVERIFY(file5.exists());
    QVERIFY(file5.open(QIODevice::ReadOnly));
    QVERIFY(file1Contents != file5.readAll());
}

void TestBlackbox::jsExtensionsTemporaryDir()
{
    QDir::setCurrent(testDataDir + "/jsextensions-temporarydir");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::jsExtensionsTextFile()
{
    QDir::setCurrent(testDataDir + "/jsextensions-textfile");
    QbsRunParameters params(QStringList() << "-f" << "textfile.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile file1("file1.txt");
    QVERIFY(file1.exists());
    QVERIFY(file1.open(QIODevice::ReadOnly));
    QCOMPARE(file1.size(), qint64(0));
    QFile file2("file2.txt");
    QVERIFY(file2.exists());
    QVERIFY(file2.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = file2.readAll().trimmed().split('\n');
    QCOMPARE(lines.size(), 6);
    QCOMPARE(lines.at(0).trimmed().constData(), "false");
    QCOMPARE(lines.at(1).trimmed().constData(), "First line.");
    QCOMPARE(lines.at(2).trimmed().constData(), "Second line.");
    QCOMPARE(lines.at(3).trimmed().constData(), "Third line.");
    QCOMPARE(lines.at(4).trimmed().constData(), qPrintable(QDir::currentPath() + "/file1.txt"));
    QCOMPARE(lines.at(5).trimmed().constData(), "true");
}

void TestBlackbox::jsExtensionsBinaryFile()
{
    QDir::setCurrent(testDataDir + "/jsextensions-binaryfile");
    QbsRunParameters params(QStringList() << "-f" << "binaryfile.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile source("source.dat");
    QVERIFY(source.exists());
    QVERIFY(source.open(QIODevice::ReadOnly));
    QCOMPARE(source.size(), qint64(0));
    QFile destination("destination.dat");
    QVERIFY(destination.exists());
    QVERIFY(destination.open(QIODevice::ReadOnly));
    const QByteArray data = destination.readAll();
    QCOMPARE(data.size(), 8);
    QCOMPARE(data.at(0), char(0x00));
    QCOMPARE(data.at(1), char(0x01));
    QCOMPARE(data.at(2), char(0x02));
    QCOMPARE(data.at(3), char(0x03));
    QCOMPARE(data.at(4), char(0x04));
    QCOMPARE(data.at(5), char(0x05));
    QCOMPARE(data.at(6), char(0x06));
    QCOMPARE(data.at(7), char(0xFF));
}

void TestBlackbox::ld()
{
    QDir::setCurrent(testDataDir + "/ld");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::symbolLinkMode()
{
    if (!HostOsInfo::isAnyUnixHost())
        QSKIP("only applies on Unix");

    QDir::setCurrent(testDataDir + "/symbolLinkMode");

    QbsRunParameters params;
    params.command = "run";
    const QStringList commonArgs{"-p", "driver", "--setup-run-env-config",
                                 "ignore-lib-dependencies"};

    rmDirR(relativeBuildDir());
    params.arguments = QStringList() << commonArgs << "project.shouldInstallLibrary:true";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("somefunction existed and it returned 42"),
             m_qbsStdout.constData());

    rmDirR(relativeBuildDir());
    params.arguments = QStringList() << commonArgs << "project.shouldInstallLibrary:false";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("somefunction did not exist"), m_qbsStdout.constData());

    rmDirR(relativeBuildDir());
    params.arguments = QStringList() << commonArgs << "project.lazy:false";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("Lib was loaded!\nmeow\n"), m_qbsStdout.constData());

    if (HostOsInfo::isMacosHost()) {
        rmDirR(relativeBuildDir());
        params.arguments = QStringList() << commonArgs << "project.lazy:true";
        QCOMPARE(runQbs(params), 0);
        QVERIFY2(m_qbsStdout.contains("meow\nLib was loaded!\n"), m_qbsStdout.constData());
    }
}

void TestBlackbox::linkerMode()
{
    if (!HostOsInfo::isAnyUnixHost())
        QSKIP("only applies on Unix");

    QDir::setCurrent(testDataDir + "/linkerMode");
    QCOMPARE(runQbs(), 0);

    auto testCondition = [&](const QString &lang,
            const std::function<bool(const QByteArray &)> &condition) {
        if ((lang == "Objective-C" || lang == "Objective-C++")
                && HostOsInfo::hostOs() != HostOsInfo::HostOsMacos)
            return;
        const QString binary = defaultInstallRoot + "/LinkedProduct-" + lang;
        QProcess deptool;
        if (HostOsInfo::hostOs() == HostOsInfo::HostOsMacos)
            deptool.start("otool", QStringList() << "-L" << binary);
        else
            deptool.start("readelf", QStringList() << "-a" << binary);
        QVERIFY(deptool.waitForStarted());
        QVERIFY(deptool.waitForFinished());
        QByteArray deptoolOutput = deptool.readAllStandardOutput();
        if (HostOsInfo::hostOs() != HostOsInfo::HostOsMacos) {
            QList<QByteArray> lines = deptoolOutput.split('\n');
            int sz = lines.size();
            for (int i = 0; i < sz; ++i) {
                if (!lines.at(i).contains("NEEDED")) {
                    lines.removeAt(i--);
                    sz--;
                }
            }

            deptoolOutput = lines.join('\n');
        }
        QCOMPARE(deptool.exitCode(), 0);
        QVERIFY2(condition(deptoolOutput), deptoolOutput.constData());
    };

    const QStringList nocpplangs = QStringList() << "Assembly" << "C" << "Objective-C";
    for (const QString &lang : nocpplangs)
        testCondition(lang, [](const QByteArray &lddOutput) { return !lddOutput.contains("c++"); });

    const QStringList cpplangs = QStringList() << "C++" << "Objective-C++";
    for (const QString &lang : cpplangs)
        testCondition(lang, [](const QByteArray &lddOutput) { return lddOutput.contains("c++"); });

    const QStringList objclangs = QStringList() << "Objective-C" << "Objective-C++";
    for (const QString &lang : objclangs)
        testCondition(lang, [](const QByteArray &lddOutput) { return lddOutput.contains("objc"); });
}

void TestBlackbox::lexyacc()
{
    if (findExecutable(QStringList("lex")).isEmpty()
            || findExecutable(QStringList("yacc")).isEmpty()) {
        QSKIP("lex or yacc not present");
    }
    QDir::setCurrent(testDataDir + "/lexyacc/one-grammar");
    QCOMPARE(runQbs(), 0);
    const QString parserBinary = relativeExecutableFilePath("one-grammar");
    QProcess p;
    p.start(parserBinary);
    QVERIFY2(p.waitForStarted(), qPrintable(p.errorString()));
    p.write("a && b || c && !d");
    p.closeWriteChannel();
    QVERIFY2(p.waitForFinished(), qPrintable(p.errorString()));
    QVERIFY2(p.exitCode() == 0, p.readAllStandardError().constData());
    const QByteArray parserOutput = p.readAllStandardOutput();
    QVERIFY2(parserOutput.contains("OR AND a b AND c NOT d"), parserOutput.constData());

    QDir::setCurrent(testDataDir + "/lexyacc/two-grammars");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);

    params.expectFailure = false;
    params.command = "resolve";
    params.arguments << (QStringList() << "modules.lex_yacc.uniqueSymbolPrefix:true");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStderr.contains("whatever"), m_qbsStderr.constData());
    params.arguments << "modules.lex_yacc.enableCompilerWarnings:true";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStderr.contains("whatever"), m_qbsStderr.constData());
}

void TestBlackbox::linkerScripts()
{
    const SettingsPtr s = settings();
    Profile buildProfile(profileName(), s.get());
    QStringList toolchain = buildProfile.value("qbs.toolchain").toStringList();
    if (!toolchain.contains("gcc") || targetOs() != HostOsInfo::HostOsLinux)
        QSKIP("linker script test only applies to Linux ");
    QDir::setCurrent(testDataDir + "/linkerscripts");
    QCOMPARE(runQbs(QbsRunParameters(QStringList("-q")
                                     << ("qbs.installRoot:" + QDir::currentPath()))), 0);
    const QString output = QString::fromLocal8Bit(m_qbsStderr);
    QRegExp pattern(".*---(.*)---.*");
    QVERIFY2(pattern.exactMatch(output), qPrintable(output));
    QCOMPARE(pattern.captureCount(), 1);
    const QString nmPath = pattern.capturedTexts().at(1);
    if (!QFile::exists(nmPath))
        QSKIP("Cannot check for symbol presence: No nm found.");
    QProcess nm;
    nm.start(nmPath, QStringList(QDir::currentPath() + "/liblinkerscripts.so"));
    QVERIFY(nm.waitForStarted());
    QVERIFY(nm.waitForFinished());
    const QByteArray nmOutput = nm.readAllStandardOutput();
    QCOMPARE(nm.exitCode(), 0);
    QVERIFY2(nmOutput.contains("TEST_SYMBOL1"), nmOutput.constData());
    QVERIFY2(nmOutput.contains("TEST_SYMBOL2"), nmOutput.constData());
}

void TestBlackbox::listProducts()
{
    QDir::setCurrent(testDataDir + "/list-products");
    QCOMPARE(runQbs(QbsRunParameters("list-products")), 0);
    m_qbsStdout.replace("\r\n", "\n");
    QVERIFY2(m_qbsStdout.contains(
                 "a\n"
                 "b {\"architecture\":\"mips\",\"buildVariant\":\"debug\"}\n"
                 "b {\"architecture\":\"mips\",\"buildVariant\":\"release\"}\n"
                 "b {\"architecture\":\"vax\",\"buildVariant\":\"debug\"}\n"
                 "b {\"architecture\":\"vax\",\"buildVariant\":\"release\"}\n"
                 "c\n"), m_qbsStdout.constData());
}

void TestBlackbox::listPropertiesWithOuter()
{
    QDir::setCurrent(testDataDir + "/list-properties-with-outer");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("listProp: [\"product\",\"higher\",\"group\"]"),
             m_qbsStdout.constData());
}

void TestBlackbox::listPropertyOrder()
{
    QDir::setCurrent(testDataDir + "/list-property-order");
    const QbsRunParameters params(QStringList() << "-qq");
    QCOMPARE(runQbs(params), 0);
    const QByteArray firstOutput = m_qbsStderr;
    for (int i = 0; i < 25; ++i) {
        rmDirR(relativeBuildDir());
        QCOMPARE(runQbs(params), 0);
        if (m_qbsStderr != firstOutput)
            break;
    }
    QCOMPARE(m_qbsStderr.constData(), firstOutput.constData());
}

void TestBlackbox::require()
{
    QDir::setCurrent(testDataDir + "/require");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::requireDeprecated()
{
    QDir::setCurrent(testDataDir + "/require-deprecated");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStderr.contains("loadExtension() function is deprecated"),
             m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains("loadFile() function is deprecated"),
             m_qbsStderr.constData());
}

void TestBlackbox::rescueTransformerData()
{
    QDir::setCurrent(testDataDir + "/rescue-transformer-data");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp") && m_qbsStdout.contains("m.p: undefined"),
             m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("main.cpp");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp") && !m_qbsStdout.contains("m.p: "),
             m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("modules.m.p:true"))), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp") && m_qbsStdout.contains("m.p: true"),
             m_qbsStdout.constData());
}

void TestBlackbox::multipleChanges()
{
    QDir::setCurrent(testDataDir + "/multiple-changes");
    QCOMPARE(runQbs(), 0);
    QFile newFile("test.blubb");
    QVERIFY(newFile.open(QIODevice::WriteOnly));
    newFile.close();
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList() << "project.prop:true")), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("prop: true"));
}

void TestBlackbox::multipleConfigurations()
{
    QDir::setCurrent(testDataDir + "/multiple-configurations");
    QbsRunParameters params(QStringList{"config:x", "config:y", "config:z"});
    params.profile.clear();
    struct DefaultProfileSwitcher
    {
        DefaultProfileSwitcher()
        {
            const SettingsPtr s = settings();
            oldDefaultProfile = s->defaultProfile();
            s->setValue("defaultProfile", profileName());
            s->sync();
        }
        ~DefaultProfileSwitcher()
        {
            const SettingsPtr s = settings();
            s->setValue("defaultProfile", oldDefaultProfile);
            s->sync();
        }
        QVariant oldDefaultProfile;
    };
    DefaultProfileSwitcher dps;
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStdout.count("compiling lib.cpp"), 3);
    QCOMPARE(m_qbsStdout.count("compiling file.cpp"), 3);
    QCOMPARE(m_qbsStdout.count("compiling main.cpp"), 3);
}

void TestBlackbox::nestedGroups()
{
    QDir::setCurrent(testDataDir + "/nested-groups");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeExecutableFilePath("nested-groups")));
}

void TestBlackbox::nestedProperties()
{
    QDir::setCurrent(testDataDir + "/nested-properties");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("value in higherlevel"), m_qbsStdout.constData());
}

void TestBlackbox::newOutputArtifact()
{
    QDir::setCurrent(testDataDir + "/new-output-artifact");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeBuildDir() + "/install-root/output_98.out"));
    const QString the100thArtifact = relativeBuildDir() + "/install-root/output_99.out";
    QVERIFY(!regularFileExists(the100thArtifact));
    QbsRunParameters params("resolve", QStringList() << "products.theProduct.artifactCount:100");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(the100thArtifact));
}

void TestBlackbox::noProfile()
{
    QDir::setCurrent(testDataDir + "/no-profile");
    QbsRunParameters params;
    params.profile = "none";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("profile: none"), m_qbsStdout.constData());
}

void TestBlackbox::nonBrokenFilesInBrokenProduct()
{
    QDir::setCurrent(testDataDir + "/non-broken-files-in-broken-product");
    QbsRunParameters params(QStringList() << "-k");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStdout.contains("fine.cpp"));
    QVERIFY(runQbs(params) != 0);
    QVERIFY(!m_qbsStdout.contains("fine.cpp")); // The non-broken file must not be recompiled.
}

void TestBlackbox::nonDefaultProduct()
{
    QDir::setCurrent(testDataDir + "/non-default-product");
    const QString defaultAppExe = relativeExecutableFilePath("default app");
    const QString nonDefaultAppExe = relativeExecutableFilePath("non-default app");

    QCOMPARE(runQbs(), 0);
    QVERIFY2(QFile::exists(defaultAppExe), qPrintable(defaultAppExe));
    QVERIFY2(!QFile::exists(nonDefaultAppExe), qPrintable(nonDefaultAppExe));

    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "--all-products")), 0);
    QVERIFY2(QFile::exists(nonDefaultAppExe), qPrintable(nonDefaultAppExe));
}

static void switchProfileContents(qbs::Profile &p, qbs::Settings *s, bool on)
{
    const QString scalarKey = "leaf.scalarProp";
    const QString listKey = "leaf.listProp";
    if (on) {
        p.setValue(scalarKey, "profile");
        p.setValue(listKey, QStringList() << "profile");
    } else {
        p.remove(scalarKey);
        p.remove(listKey);
    }
    s->sync();
}

static void switchFileContents(QFile &f, bool on)
{
    f.seek(0);
    QByteArray contents = f.readAll();
    f.resize(0);
    if (on)
        contents.replace("// leaf.", "leaf.");
    else
        contents.replace("leaf.", "// leaf.");
    f.write(contents);
    f.flush();
}

void TestBlackbox::propertyPrecedence()
{
    QDir::setCurrent(testDataDir + "/property-precedence");
    const SettingsPtr s = settings();
    qbs::Internal::TemporaryProfile profile("qbs_autotests_propPrecedence", s.get());
    profile.p.setValue("qbs.architecture", "x86"); // Profiles must not be empty...
    s->sync();
    const QStringList args = QStringList() << "-f" << "property-precedence.qbs";
    QbsRunParameters params(args);
    params.profile = profile.p.name();
    QbsRunParameters resolveParams = params;
    resolveParams.command = "resolve";

    // Case 1: [cmdline=0,prod=0,export=0,nonleaf=0,profile=0]
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: leaf\n")
             && m_qbsStdout.contains("list prop: [\"leaf\"]\n"),
             m_qbsStdout.constData());
    params.arguments.clear();

    // Case 2: [cmdline=0,prod=0,export=0,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: profile\n")
             && m_qbsStdout.contains("list prop: [\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 3: [cmdline=0,prod=0,export=0,nonleaf=1,profile=0]
    QFile nonleafFile("modules/nonleaf/nonleaf.qbs");
    QVERIFY2(nonleafFile.open(QIODevice::ReadWrite), qPrintable(nonleafFile.errorString()));
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: nonleaf\n")
             && m_qbsStdout.contains("list prop: [\"nonleaf\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 4: [cmdline=0,prod=0,export=0,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: nonleaf\n")
             && m_qbsStdout.contains("list prop: [\"nonleaf\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 5: [cmdline=0,prod=0,export=1,nonleaf=0,profile=0]
    QFile depFile("dep.qbs");
    QVERIFY2(depFile.open(QIODevice::ReadWrite), qPrintable(depFile.errorString()));
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: export\n")
             && m_qbsStdout.contains("list prop: [\"export\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 6: [cmdline=0,prod=0,export=1,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: export\n")
             && m_qbsStdout.contains("list prop: [\"export\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 7: [cmdline=0,prod=0,export=1,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: export\n")
             && m_qbsStdout.contains("list prop: [\"export\",\"nonleaf\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 8: [cmdline=0,prod=0,export=1,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: export\n")
             && m_qbsStdout.contains("list prop: [\"export\",\"nonleaf\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 9: [cmdline=0,prod=1,export=0,nonleaf=0,profile=0]
    QFile productFile("property-precedence.qbs");
    QVERIFY2(productFile.open(QIODevice::ReadWrite), qPrintable(productFile.errorString()));
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, false);
    switchFileContents(productFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 10: [cmdline=0,prod=1,export=0,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 11: [cmdline=0,prod=1,export=0,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"nonleaf\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 12: [cmdline=0,prod=1,export=0,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"nonleaf\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 13: [cmdline=0,prod=1,export=1,nonleaf=0,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"export\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 14: [cmdline=0,prod=1,export=1,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"export\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 15: [cmdline=0,prod=1,export=1,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"export\",\"nonleaf\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 16: [cmdline=0,prod=1,export=1,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"export\",\"nonleaf\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Command line properties wipe everything, including lists.
    // Case 17: [cmdline=1,prod=0,export=0,nonleaf=0,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, false);
    switchFileContents(productFile, false);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 18: [cmdline=1,prod=0,export=0,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 19: [cmdline=1,prod=0,export=0,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 20: [cmdline=1,prod=0,export=0,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 21: [cmdline=1,prod=0,export=1,nonleaf=0,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 22: [cmdline=1,prod=0,export=1,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 23: [cmdline=1,prod=0,export=1,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 24: [cmdline=1,prod=0,export=1,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 25: [cmdline=1,prod=1,export=0,nonleaf=0,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, false);
    switchFileContents(productFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 26: [cmdline=1,prod=1,export=0,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 27: [cmdline=1,prod=1,export=0,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 28: [cmdline=1,prod=1,export=0,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 29: [cmdline=1,prod=1,export=1,nonleaf=0,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 30: [cmdline=1,prod=1,export=1,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 31: [cmdline=1,prod=1,export=1,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 32: [cmdline=1,prod=1,export=1,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());
}

void TestBlackbox::productDependenciesByType()
{
    QDir::setCurrent(testDataDir + "/product-dependencies-by-type");
    QCOMPARE(runQbs(), 0);
    QFile appListFile(relativeProductBuildDir("app list") + "/app-list.txt");
    QVERIFY2(appListFile.open(QIODevice::ReadOnly), qPrintable(appListFile.fileName()));
    const QList<QByteArray> appList = appListFile.readAll().trimmed().split('\n');
    QCOMPARE(appList.size(), 3);
    QStringList apps = QStringList()
            << QDir::currentPath() + '/' + relativeExecutableFilePath("app1")
            << QDir::currentPath() + '/' + relativeExecutableFilePath("app2")
            << QDir::currentPath() + '/' + relativeExecutableFilePath("app3");
    for (const QByteArray &line : appList) {
        const QString cleanLine = QString::fromLocal8Bit(line.trimmed());
        QVERIFY2(apps.removeOne(cleanLine), qPrintable(cleanLine));
    }
    QVERIFY(apps.empty());
}

void TestBlackbox::properQuoting()
{
    QDir::setCurrent(testDataDir + "/proper quoting");
    QCOMPARE(runQbs(), 0);
    QbsRunParameters params(QLatin1String("run"), QStringList() << "-q" << "-p" << "Hello World");
    params.expectFailure = true; // Because the exit code is non-zero.
    QCOMPARE(runQbs(params), 156);
    const char * const expectedOutput = "whitespaceless\ncontains space\ncontains\ttab\n"
            "backslash\\\nHello World! The magic number is 156.";
    QCOMPARE(unifiedLineEndings(m_qbsStdout).constData(), expectedOutput);
}

void TestBlackbox::propertiesInExportItems()
{
    QDir::setCurrent(testDataDir + "/properties-in-export-items");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeExecutableFilePath("p1")));
    QVERIFY(regularFileExists(relativeExecutableFilePath("p2")));
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
}

void TestBlackbox::pseudoMultiplexing()
{
    // This is "pseudo-multiplexing" on all platforms that initialize qbs.architectures
    // to an array with one element. See QBS-1243.
    QDir::setCurrent(testDataDir + "/pseudo-multiplexing");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::radAfterIncompleteBuild_data()
{
    QTest::addColumn<QString>("projectFileName");
    QTest::newRow("Project with Rule") << "project_with_rule.qbs";
    QTest::newRow("Project with Transformer") << "project_with_transformer.qbs";
}

void TestBlackbox::radAfterIncompleteBuild()
{
    QDir::setCurrent(testDataDir + "/rad-after-incomplete-build");
    rmDirR(relativeBuildDir());
    const QString projectFileName = "project_with_rule.qbs";

    // Step 1: Have a directory where a file used to be.
    QbsRunParameters params(QStringList() << "-f" << projectFileName);
    QCOMPARE(runQbs(params), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile(projectFileName);
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray content = projectFile.readAll();
    content.replace("oldfile", "oldfile/newfile");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    WAIT_FOR_NEW_TIMESTAMP();
    content.replace("oldfile/newfile", "newfile");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    content.replace("newfile", "oldfile/newfile");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    QCOMPARE(runQbs(params), 0);

    // Step 2: Have a file where a directory used to be.
    WAIT_FOR_NEW_TIMESTAMP();
    content.replace("oldfile/newfile", "oldfile");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    WAIT_FOR_NEW_TIMESTAMP();
    content.replace("oldfile", "newfile");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    content.replace("newfile", "oldfile");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::subProfileChangeTracking()
{
    QDir::setCurrent(testDataDir + "/subprofile-change-tracking");
    const SettingsPtr s = settings();
    qbs::Internal::TemporaryProfile subProfile("qbs-autotests-subprofile", s.get());
    subProfile.p.setValue("baseProfile", profileName());
    subProfile.p.setValue("cpp.includePaths", QStringList("/tmp/include1"));
    s->sync();
    QCOMPARE(runQbs(), 0);

    subProfile.p.setValue("cpp.includePaths", QStringList("/tmp/include2"));
    s->sync();
    QbsRunParameters params;
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("main1.cpp"));
    QVERIFY(m_qbsStdout.contains("main2.cpp"));
}

void TestBlackbox::successiveChanges()
{
    QDir::setCurrent(testDataDir + "/successive-changes");
    QCOMPARE(runQbs(), 0);

    QbsRunParameters params("resolve", QStringList() << "products.theProduct.type:output,blubb");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);

    params.arguments << "project.version:2";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QFile output(relativeProductBuildDir("theProduct") + "/output.out");
    QVERIFY2(output.open(QIODevice::ReadOnly), qPrintable(output.errorString()));
    const QByteArray version = output.readAll();
    QCOMPARE(version.constData(), "2");
}

void TestBlackbox::installedApp()
{
    QDir::setCurrent(testDataDir + "/installed_artifact");

    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/usr/bin/installedApp"))));

    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("qbs.installRoot:" + testDataDir
                                                            + "/installed-app"))), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(testDataDir
            + HostOsInfo::appendExecutableSuffix("/installed-app/usr/bin/installedApp")));

    QFile addedFile(defaultInstallRoot + QLatin1String("/blubb.txt"));
    QVERIFY(addedFile.open(QIODevice::WriteOnly));
    addedFile.close();
    QVERIFY(addedFile.exists());
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "--clean-install-root")), 0);
    QVERIFY(regularFileExists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/usr/bin/installedApp"))));
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/usr/src/main.cpp")));
    QVERIFY(!addedFile.exists());

    // Check whether changing install parameters on the product causes re-installation.
    QFile projectFile("installed_artifact.qbs");
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray content = projectFile.readAll();
    content.replace("qbs.installPrefix: \"/usr\"", "qbs.installPrefix: '/usr/local'");
    WAIT_FOR_NEW_TIMESTAMP();
    projectFile.resize(0);
    projectFile.write(content);
    QVERIFY(projectFile.flush());
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/usr/local/bin/installedApp"))));
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/usr/local/src/main.cpp")));

    // Check whether changing install parameters on the artifact causes re-installation.
    content.replace("qbs.installDir: \"bin\"", "qbs.installDir: 'custom'");
    WAIT_FOR_NEW_TIMESTAMP();
    projectFile.resize(0);
    projectFile.write(content);
    QVERIFY(projectFile.flush());
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/usr/local/custom/installedApp"))));

    // Check whether changing install parameters on a source file causes re-installation.
    content.replace("qbs.installDir: \"src\"", "qbs.installDir: 'source'");
    WAIT_FOR_NEW_TIMESTAMP();
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/usr/local/source/main.cpp")));

    // Check whether changing install parameters on the command line causes re-installation.
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("qbs.installRoot:" + relativeBuildDir()
                                                 + "/blubb"))), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeBuildDir() + "/blubb/usr/local/source/main.cpp"));

    // Check --no-install
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "--no-install")), 0);
    QCOMPARE(QDir(defaultInstallRoot).entryList(QDir::NoDotAndDotDot).size(), 0);

    // Check --no-build (with and without an existing build graph)
    QbsRunParameters params("install", QStringList() << "--no-build");
    QCOMPARE(runQbs(params), 0);
    rmDirR(relativeBuildDir());
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Build graph not found"), m_qbsStderr.constData());
}

void TestBlackbox::installDuplicates()
{
    QDir::setCurrent(testDataDir + "/install-duplicates");

    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("Cannot install files"));
}

void TestBlackbox::installDuplicatesNoError()
{
    QDir::setCurrent(testDataDir + "/install-duplicates-no-error");

    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::installedSourceFiles()
{
    QDir::setCurrent(testDataDir + "/installed-source-files");

    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/readme.txt")));
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/main.cpp")));
}

void TestBlackbox::toolLookup()
{
    QbsRunParameters params(QLatin1String("setup-toolchains"), QStringList("--help"));
    params.profile.clear();
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::topLevelSearchPath()
{
    QDir::setCurrent(testDataDir + "/toplevel-searchpath");

    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("MyProduct"), m_qbsStderr.constData());
    params.arguments << ("project.qbsSearchPaths:" + QDir::currentPath() + "/qbs-resources");
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::checkProjectFilePath()
{
    QDir::setCurrent(testDataDir + "/project_filepath_check");
    QbsRunParameters params(QStringList("-f") << "project1.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("main.cpp"), m_qbsStdout.constData());
    QCOMPARE(runQbs(params), 0);

    params.arguments = QStringList("-f") << "project2.qbs";
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("project file"));

    params.arguments = QStringList("-f") << "project2.qbs";
    params.command = "resolve";
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("main2.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::checkTimestamps()
{
    QDir::setCurrent(testDataDir + "/check-timestamps");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling file.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY(QFile::remove(relativeBuildGraphFilePath()));
    WAIT_FOR_NEW_TIMESTAMP();
    touch("file.h");
    QCOMPARE(runQbs(QStringList("--check-timestamps")), 0);
    QVERIFY2(m_qbsStdout.contains("compiling file.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::chooseModuleInstanceByPriority()
{
    QFETCH(QString, idol);
    QFETCH(QStringList, expectedSubStrings);
    QFETCH(bool, expectSuccess);
    QDir::setCurrent(testDataDir + "/choose-module-instance");
    rmDirR(relativeBuildDir());
    QbsRunParameters params(QStringList("modules.qbs.targetPlatform:" + idol));
    params.expectFailure = !expectSuccess;
    if (expectSuccess) {
        QCOMPARE(runQbs(params), 0);
    } else {
        QVERIFY(runQbs(params) != 0);
        return;
    }

    const QString installRoot = relativeBuildDir() + "/install-root/";
    QVERIFY(QFile::exists(installRoot + "/gerbil.txt"));
    QFile file(installRoot + "/gerbil.txt");
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(file.readAll());
    for (auto str : expectedSubStrings) {
        if (content.contains(str))
            continue;
        qDebug() << "content:" << content;
        qDebug() << "substring:" << str;
        QFAIL("missing substring");
    }
}

void TestBlackbox::chooseModuleInstanceByPriority_data()
{
    QTest::addColumn<QString>("idol");
    QTest::addColumn<QStringList>("expectedSubStrings");
    QTest::addColumn<bool>("expectSuccess");
    QTest::newRow("ringo")
            << "Beatles" << QStringList() << false;
    QTest::newRow("ritchie1")
            << "Deep Purple" << QStringList{"slipped", "litchi", "ritchie"} << true;
    QTest::newRow("ritchie2")
            << "Rainbow" << QStringList{"slipped", "litchi", "ritchie"} << true;
    QTest::newRow("lord")
            << "Whitesnake" << QStringList{"chewed", "cord", "lord"} << true;
}

class TemporaryDefaultProfileRemover
{
public:
    TemporaryDefaultProfileRemover(qbs::Settings *settings)
        : m_settings(settings), m_defaultProfile(settings->defaultProfile())
    {
        m_settings->remove(QLatin1String("defaultProfile"));
    }

    ~TemporaryDefaultProfileRemover()
    {
        if (!m_defaultProfile.isEmpty())
            m_settings->setValue(QLatin1String("defaultProfile"), m_defaultProfile);
    }

private:
    qbs::Settings *m_settings;
    const QString m_defaultProfile;
};

void TestBlackbox::assembly()
{
    QDir::setCurrent(testDataDir + "/assembly");
    QVERIFY(runQbs() == 0);

    const QVariantMap properties = ([&]() {
        QFile propertiesFile(relativeProductBuildDir("assembly") + "/properties.json");
        if (propertiesFile.open(QIODevice::ReadOnly))
            return QJsonDocument::fromJson(propertiesFile.readAll()).toVariant().toMap();
        return QVariantMap();
    })();
    QVERIFY(!properties.empty());
    const auto toolchain = properties.value("qbs.toolchain").toStringList();
    QVERIFY(!toolchain.empty());
    const bool haveGcc = toolchain.contains("gcc");
    const bool haveMSVC = toolchain.contains("msvc");

    QCOMPARE(m_qbsStdout.contains("assembling testa.s"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("compiling testb.S"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("compiling testc.sx"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("creating libtesta.a"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("creating libtestb.a"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("creating libtestc.a"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("creating testd.lib"), haveMSVC);
}

void TestBlackbox::auxiliaryInputsFromDependencies()
{
    QDir::setCurrent(testDataDir + "/aux-inputs-from-deps");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating dummy.out"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("products.dep.sleep:false"))), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating dummy.out"), m_qbsStdout.constData());
}

static bool haveMakeNsis()
{
    QStringList regKeys;
    regKeys << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\NSIS")
            << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\NSIS");

    QStringList paths = QProcessEnvironment::systemEnvironment().value("PATH")
            .split(HostOsInfo::pathListSeparator(), QString::SkipEmptyParts);

    for (const QString &key : qAsConst(regKeys)) {
        QSettings settings(key, QSettings::NativeFormat);
        QString str = settings.value(QLatin1String(".")).toString();
        if (!str.isEmpty())
            paths.prepend(str);
    }

    bool haveMakeNsis = false;
    for (const QString &path : qAsConst(paths)) {
        if (regularFileExists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QLatin1String("/makensis")))) {
            haveMakeNsis = true;
            break;
        }
    }

    return haveMakeNsis;
}

void TestBlackbox::nsis()
{
    if (!haveMakeNsis()) {
        QSKIP("makensis is not installed");
        return;
    }

    bool targetIsWindows = targetOs() == HostOsInfo::HostOsWindows;
    QDir::setCurrent(testDataDir + "/nsis");
    QVERIFY(runQbs() == 0);
    QCOMPARE((bool)m_qbsStdout.contains("compiling hello.nsi"), targetIsWindows);
    QCOMPARE((bool)m_qbsStdout.contains("SetCompressor ignored due to previous call with the /FINAL switch"), targetIsWindows);
    QVERIFY(!QFile::exists(defaultInstallRoot + "/you-should-not-see-a-file-with-this-name.exe"));
}

void TestBlackbox::nsisDependencies()
{
    if (!haveMakeNsis()) {
        QSKIP("makensis is not installed");
        return;
    }

    bool targetIsWindows = targetOs() == HostOsInfo::HostOsWindows;
    QDir::setCurrent(testDataDir + "/nsisDependencies");
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.contains("compiling hello.nsi"), targetIsWindows);
}

void TestBlackbox::enableExceptions()
{
    QFETCH(QString, file);
    QFETCH(bool, enable);
    QFETCH(bool, expectSuccess);

    QDir::setCurrent(testDataDir + QStringLiteral("/enableExceptions"));

    QbsRunParameters params;
    params.arguments = QStringList() << "-f" << file
                                     << (QStringLiteral("modules.cpp.enableExceptions:")
                                         + (enable ? "true" : "false"));
    params.expectFailure = !expectSuccess;
    rmDirR(relativeBuildDir());
    if (!params.expectFailure)
        QCOMPARE(runQbs(params), 0);
    else
        QVERIFY(runQbs(params) != 0);
}

void TestBlackbox::enableExceptions_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<bool>("enable");
    QTest::addColumn<bool>("expectSuccess");

    QTest::newRow("no exceptions, enabled") << "none.qbs" << true << true;
    QTest::newRow("no exceptions, disabled") << "none.qbs" << false << true;

    QTest::newRow("C++ exceptions, enabled") << "exceptions.qbs" << true << true;
    QTest::newRow("C++ exceptions, disabled") << "exceptions.qbs" << false << false;

    if (HostOsInfo::isMacosHost()) {
        QTest::newRow("Objective-C exceptions, enabled") << "exceptions-objc.qbs" << true << true;
        QTest::newRow("Objective-C exceptions in Objective-C++ source, enabled") << "exceptions-objcpp.qbs" << true << true;
        QTest::newRow("C++ exceptions in Objective-C++ source, enabled") << "exceptions-objcpp-cpp.qbs" << true << true;
        QTest::newRow("Objective-C, disabled") << "exceptions-objc.qbs" << false << false;
        QTest::newRow("Objective-C exceptions in Objective-C++ source, disabled") << "exceptions-objcpp.qbs" << false << false;
        QTest::newRow("C++ exceptions in Objective-C++ source, disabled") << "exceptions-objcpp-cpp.qbs" << false << false;
    }
}

void TestBlackbox::enableRtti()
{
    QDir::setCurrent(testDataDir + QStringLiteral("/enableRtti"));

    QbsRunParameters params;

    params.arguments = QStringList() << "modules.cpp.enableRtti:true";
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(params), 0);

    if (HostOsInfo::isMacosHost()) {
        params.arguments = QStringList() << "modules.cpp.enableRtti:true"
                                         << "project.treatAsObjcpp:true";
        rmDirR(relativeBuildDir());
        QCOMPARE(runQbs(params), 0);
    }

    params.expectFailure = true;

    params.arguments = QStringList() << "modules.cpp.enableRtti:false";
    rmDirR(relativeBuildDir());
    QVERIFY(runQbs(params) != 0);

    if (HostOsInfo::isMacosHost()) {
        params.arguments = QStringList() << "modules.cpp.enableRtti:false"
                                         << "project.treatAsObjcpp:true";
        rmDirR(relativeBuildDir());
        QVERIFY(runQbs(params) != 0);
    }
}

void TestBlackbox::envMerging()
{
    QDir::setCurrent(testDataDir + "/env-merging");
    QbsRunParameters params;
    QString pathVal = params.environment.value("PATH");
    pathVal.prepend(HostOsInfo::pathListSeparator()).prepend("/opt/blackbox/bin");
    const QString keyName = HostOsInfo::isWindowsHost() ? "pATh" : "PATH";
    params.environment.insert(keyName, pathVal);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains(QByteArray("PATH=/opt/tool/bin")
                                  + HostOsInfo::pathListSeparator().toLatin1())
             && m_qbsStdout.contains(HostOsInfo::pathListSeparator().toLatin1()
                                     + QByteArray("/opt/blackbox/bin")),
             m_qbsStdout.constData());
}

void TestBlackbox::envNormalization()
{
    QDir::setCurrent(testDataDir + "/env-normalization");
    QbsRunParameters params;
    params.environment.insert("myvar", "x");
    QCOMPARE(runQbs(params), 0);
    if (HostOsInfo::isWindowsHost())
        QVERIFY2(m_qbsStdout.contains("\"MYVAR\":\"x\""), m_qbsStdout.constData());
    else
        QVERIFY2(m_qbsStdout.contains("\"myvar\":\"x\""), m_qbsStdout.constData());
}

void TestBlackbox::generatedArtifactAsInputToDynamicRule()
{
    QDir::setCurrent(testDataDir + "/generated-artifact-as-input-to-dynamic-rule");
    QCOMPARE(runQbs(), 0);
    const QString oldFile = relativeProductBuildDir("p") + "/old.txt";
    QVERIFY2(regularFileExists(oldFile), qPrintable(oldFile));
    WAIT_FOR_NEW_TIMESTAMP();
    QFile inputFile("input.txt");
    QVERIFY2(inputFile.open(QIODevice::WriteOnly), qPrintable(inputFile.errorString()));
    inputFile.resize(0);
    inputFile.write("new.txt");
    inputFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!regularFileExists(oldFile), qPrintable(oldFile));
    const QString newFile = relativeProductBuildDir("p") + "/new.txt";
    QVERIFY2(regularFileExists(newFile), qPrintable(oldFile));
    QVERIFY2(m_qbsStdout.contains("generating"), m_qbsStdout.constData());
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("generating"), m_qbsStdout.constData());
}

static bool haveWiX(const Profile &profile)
{
    if (profile.value("wix.toolchainInstallPath").isValid() &&
            profile.value("wix.toolchainInstallRoot").isValid()) {
        return true;
    }

    QStringList regKeys;
    regKeys << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows Installer XML\\")
            << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Installer XML\\");

    QStringList paths = QProcessEnvironment::systemEnvironment().value("PATH")
            .split(HostOsInfo::pathListSeparator(), QString::SkipEmptyParts);

    for (const QString &key : qAsConst(regKeys)) {
        const QStringList versions = QSettings(key, QSettings::NativeFormat).childGroups();
        for (const QString &version : versions) {
            QSettings settings(key + version, QSettings::NativeFormat);
            QString str = settings.value(QLatin1String("InstallRoot")).toString();
            if (!str.isEmpty())
                paths.prepend(str);
        }
    }

    for (const QString &path : qAsConst(paths)) {
        if (regularFileExists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QLatin1String("/candle"))) &&
            regularFileExists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QLatin1String("/light")))) {
            return true;
        }
    }

    return false;
}

void TestBlackbox::wix()
{
    const SettingsPtr s = settings();
    Profile profile(profileName(), s.get());

    if (!haveWiX(profile)) {
        QSKIP("WiX is not installed");
        return;
    }

    QByteArray arch = profile.value("qbs.architecture").toString().toLatin1();
    if (arch.isEmpty())
        arch = QByteArrayLiteral("x86");

    QDir::setCurrent(testDataDir + "/wix");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling QbsSetup.wxs"), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains("linking qbs.msi"), m_qbsStdout);
    QVERIFY(regularFileExists(relativeProductBuildDir("QbsSetup") + "/qbs.msi"));

    if (HostOsInfo::isWindowsHost()) {
        QVERIFY2(m_qbsStdout.contains("compiling QbsBootstrapper.wxs"), m_qbsStdout);
        QVERIFY2(m_qbsStdout.contains("linking qbs-setup-" + arch + ".exe"), m_qbsStdout);
        QVERIFY(regularFileExists(relativeProductBuildDir("QbsBootstrapper")
                                  + "/qbs-setup-" + arch + ".exe"));
    }
}

void TestBlackbox::wixDependencies()
{
    const SettingsPtr s = settings();
    Profile profile(profileName(), s.get());

    if (!haveWiX(profile)) {
        QSKIP("WiX is not installed");
        return;
    }

    QByteArray arch = profile.value("qbs.architecture").toString().toLatin1();
    if (arch.isEmpty())
        arch = QByteArrayLiteral("x86");

    QDir::setCurrent(testDataDir + "/wixDependencies");
    QbsRunParameters params;
    if (!HostOsInfo::isWindowsHost())
        params.arguments << "qbs.targetOS:windows";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("compiling QbsSetup.wxs"), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains("linking qbs.msi"), m_qbsStdout);
    QVERIFY(regularFileExists(relativeBuildDir() + "/qbs.msi"));
}

void TestBlackbox::nodejs()
{
    const SettingsPtr s = settings();
    Profile p(profileName(), s.get());

    int status;
    findNodejs(&status);
    QCOMPARE(status, 0);

    QDir::setCurrent(testDataDir + QLatin1String("/nodejs"));

    status = runQbs();
    if (p.value("nodejs.toolchainInstallPath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("toolchainInstallPath")) {
        QSKIP("nodejs.toolchainInstallPath not set and automatic detection failed");
    }

    if (p.value("nodejs.packageManagerPrefixPath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("nodejs.packageManagerPrefixPath")) {
        QSKIP("nodejs.packageManagerFilePath not set and automatic detection failed");
    }

    QCOMPARE(status, 0);

    QbsRunParameters params;
    params.command = QLatin1String("run");
    QCOMPARE(runQbs(params), 0);
    QVERIFY((bool)m_qbsStdout.contains("hello world"));
    QVERIFY(regularFileExists(relativeProductBuildDir("hello") + "/hello.js"));
}

void TestBlackbox::typescript()
{
    const SettingsPtr s = settings();
    Profile p(profileName(), s.get());

    int status;
    findTypeScript(&status);
    QCOMPARE(status, 0);

    QDir::setCurrent(testDataDir + QLatin1String("/typescript"));

    QbsRunParameters params;
    params.expectFailure = true;
    status = runQbs(params);
    if (p.value("typescript.toolchainInstallPath").toString().isEmpty() && status != 0) {
        if (m_qbsStderr.contains("Path\" must be specified"))
            QSKIP("typescript probe failed");
        if (m_qbsStderr.contains("typescript.toolchainInstallPath"))
            QSKIP("typescript.toolchainInstallPath not set and automatic detection failed");
        if (m_qbsStderr.contains("nodejs.interpreterFilePath"))
            QSKIP("nodejs.interpreterFilePath not set and automatic detection failed");
    }

    if (status != 0)
        qDebug() << m_qbsStderr;
    QCOMPARE(status, 0);

    params.expectFailure = false;
    params.command = QLatin1String("run");
    params.arguments = QStringList() << "-p" << "animals";
    QCOMPARE(runQbs(params), 0);

    QVERIFY(regularFileExists(relativeProductBuildDir("animals") + "/animals.js"));
    QVERIFY(regularFileExists(relativeProductBuildDir("animals") + "/extra.js"));
    QVERIFY(regularFileExists(relativeProductBuildDir("animals") + "/main.js"));
}

void TestBlackbox::importInPropertiesCondition()
{
    QDir::setCurrent(testDataDir + "/import-in-properties-condition");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::importSearchPath()
{
    QDir::setCurrent(testDataDir + "/import-searchpath");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling somefile.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::importingProduct()
{
    QDir::setCurrent(testDataDir + "/importing-product");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::importsConflict()
{
    QDir::setCurrent(testDataDir + "/imports-conflict");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::includeLookup()
{
    QDir::setCurrent(testDataDir + "/includeLookup");
    QbsRunParameters params;
    params.command = "run";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("definition.."), m_qbsStdout.constData());
}

static bool haveInnoSetup(const Profile &profile)
{
    if (profile.value("innosetup.toolchainInstallPath").isValid())
        return true;

    QStringList regKeys;
    regKeys << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Inno Setup 5_is1")
            << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Inno Setup 5_is1");

    QStringList paths = QProcessEnvironment::systemEnvironment().value("PATH")
            .split(HostOsInfo::pathListSeparator(), QString::SkipEmptyParts);

    for (const QString &key : regKeys) {
        QSettings settings(key, QSettings::NativeFormat);
        QString str = settings.value(QLatin1String("InstallLocation")).toString();
        if (!str.isEmpty())
            paths.prepend(str);
    }

    for (const QString &path : paths) {
        if (regularFileExists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QLatin1String("/ISCC"))))
            return true;
    }

    return false;
}

void TestBlackbox::innoSetup()
{
    const SettingsPtr s = settings();
    Profile profile(profileName(), s.get());

    if (!haveInnoSetup(profile)) {
        QSKIP("Inno Setup is not installed");
        return;
    }

    QDir::setCurrent(testDataDir + "/innosetup");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling test.iss"));
    QVERIFY(m_qbsStdout.contains("compiling Example1.iss"));
    QVERIFY(regularFileExists(relativeProductBuildDir("QbsSetup") + "/qbs.setup.test.exe"));
    QVERIFY(regularFileExists(relativeProductBuildDir("Example1") + "/Example1.exe"));
}

void TestBlackbox::innoSetupDependencies()
{
    const SettingsPtr s = settings();
    Profile profile(profileName(), s.get());

    if (!haveInnoSetup(profile)) {
        QSKIP("Inno Setup is not installed");
        return;
    }

    QDir::setCurrent(testDataDir + "/innosetupDependencies");
    QbsRunParameters params;
    if (!HostOsInfo::isWindowsHost())
        params.arguments << "qbs.targetOS:windows";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling test.iss"));
    QVERIFY(regularFileExists(relativeBuildDir() + "/qbs.setup.test.exe"));
}

void TestBlackbox::outputArtifactAutoTagging()
{
    QDir::setCurrent(testDataDir + QLatin1String("/output-artifact-auto-tagging"));

    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeExecutableFilePath("output-artifact-auto-tagging")));
}

void TestBlackbox::wildCardsAndRules()
{
    QDir::setCurrent(testDataDir + "/wildcards-and-rules");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("Creating output artifact"));
    QFile output(relativeProductBuildDir("wildcards-and-rules") + "/test.mytype");
    QVERIFY2(output.open(QIODevice::ReadOnly), qPrintable(output.errorString()));
    QCOMPARE(output.readAll().count('\n'), 1);
    output.close();

    // Add input.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("input2.inp");
    QbsRunParameters params;
    params.expectFailure = true;
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Creating output artifact"));
    QVERIFY2(output.open(QIODevice::ReadOnly), qPrintable(output.errorString()));
    QCOMPARE(output.readAll().count('\n'), 2);
    output.close();

    // Add "explicitlyDependsOn".
    WAIT_FOR_NEW_TIMESTAMP();
    touch("dep.dep");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("Creating output artifact"));

    // Add nothing.
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("Creating output artifact"));
}

void TestBlackbox::loadableModule()
{
    QDir::setCurrent(testDataDir + QLatin1String("/loadablemodule"));

    QbsRunParameters params;
    params.command = "run";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("foo = 42"), m_qbsStdout.constData());
}

void TestBlackbox::localDeployment()
{
    QDir::setCurrent(testDataDir + "/localDeployment");
    QFile main("main.cpp");
    QVERIFY(main.open(QIODevice::ReadOnly));
    QByteArray content = main.readAll();
    content.replace('\r', "");
    QbsRunParameters params;
    params.command = "run";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains(content), m_qbsStdout.constData());
}

void TestBlackbox::minimumSystemVersion()
{
    rmDirR(relativeBuildDir());
    QDir::setCurrent(testDataDir + "/minimumSystemVersion");
    QFETCH(QString, file);
    QFETCH(QString, output);
    QbsRunParameters params({ "-f", file + ".qbs" });
    params.command = "run";
    QCOMPARE(runQbs(params), 0);
    if (!m_qbsStdout.contains(output.toUtf8())) {
        qDebug() << "expected output:" << qPrintable(output);
        qDebug() << "actual output:" << m_qbsStdout.constData();
    }
    QVERIFY(m_qbsStdout.contains(output.toUtf8()));
}

static qbs::Version fromMinimumDeploymentTargetValue(int v, bool isMacOS)
{
    if (isMacOS && v < 100000)
        return qbs::Version(v / 100, v / 10 % 10, v % 10);
    return qbs::Version(v / 10000, v / 100 % 100, v % 100);
}

static int toMinimumDeploymentTargetValue(const qbs::Version &v, bool isMacOS)
{
    if (isMacOS && v < qbs::Version(10, 10))
        return (v.majorVersion() * 100) + (v.minorVersion() * 10) + v.patchLevel();
    return (v.majorVersion() * 10000) + (v.minorVersion() * 100) + v.patchLevel();
}

static qbs::Version defaultClangMinimumDeploymentTarget()
{
    QProcess process;
    process.start("/usr/bin/xcrun", {"-sdk", "macosx", "clang++",
                                     "-target", "x86_64-apple-macosx-macho",
                                     "-dM", "-E", "-x", "objective-c++", "/dev/null"});
    if (waitForProcessSuccess(process)) {
        const auto lines = process.readAllStandardOutput().split('\n');
        for (const auto &line : lines) {
            static const QByteArray prefix =
                "#define __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ ";
            if (line.startsWith(prefix)) {
                bool ok = false;
                int v = line.mid(prefix.size()).trimmed().toInt(&ok);
                if (ok)
                    return fromMinimumDeploymentTargetValue(v, true);
                break;
            }
        }
    }

    return qbs::Version();
}

void TestBlackbox::minimumSystemVersion_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("output");

    // Don't check for the full "version X.Y.Z\n" on macOS as some older versions of otool don't
    // show the patch version. Instead, simply check for "version X.Y" with no trailing \n.

    const QString unspecified = []() -> QString {
        if (HostOsInfo::isMacosHost()) {
            const auto v = defaultClangMinimumDeploymentTarget();
            return "__MAC_OS_X_VERSION_MIN_REQUIRED="
                    + QString::number(toMinimumDeploymentTargetValue(v, true))
                    + "\nversion "
                    + QString::number(v.majorVersion()) + "." + QString::number(v.minorVersion());
        }

        if (HostOsInfo::isWindowsHost())
            return "WINVER is not defined\n";

        return "";
    }();

    const QString specific = []() -> QString {
        if (HostOsInfo::isMacosHost())
            return "__MAC_OS_X_VERSION_MIN_REQUIRED=1060\nversion 10.6\n";

        if (HostOsInfo::isWindowsHost())
            return "WINVER=1536\n6.00 operating system version\n6.00 subsystem version\n";

        return "";
    }();

    QTest::newRow("unspecified") << "unspecified" << unspecified;
    QTest::newRow("unspecified-forced") << "unspecified-forced" << unspecified;
    if (HostOsInfo::isWindowsHost() || HostOsInfo::isMacosHost())
        QTest::newRow("specific") << "specific" << specific;
    if (HostOsInfo::isWindowsHost())
        QTest::newRow("fakewindows") << "fakewindows" << "WINVER=1283\n5.03 operating system "
                                                         "version\n5.03 subsystem version\n";
    if (HostOsInfo::isMacosHost())
        QTest::newRow("macappstore") << "macappstore" << "__MAC_OS_X_VERSION_MIN_REQUIRED=1068\n"
                                                         "version 10.6";
}

void TestBlackbox::missingBuildGraph()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QDir::setCurrent(tmpDir.path());
    QFETCH(QString, configName);
    const QStringList commands({"clean", "dump-nodes-tree", "status", "update-timestamps"});
    const QString actualConfigName = configName.isEmpty() ? QString("default") : configName;
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << QLatin1String("config:") + actualConfigName;
    for (const QString &command : qAsConst(commands)) {
        params.command = command;
        QVERIFY2(runQbs(params) != 0, qPrintable(command));
        const QString expectedErrorMessage = QString("Build graph not found for "
                                                     "configuration '%1'").arg(actualConfigName);
        if (!m_qbsStderr.contains(expectedErrorMessage.toLocal8Bit())) {
            qDebug() << command;
            qDebug() << expectedErrorMessage;
            qDebug() << m_qbsStderr;
            QFAIL("unexpected error message");
        }
    }
}

void TestBlackbox::missingBuildGraph_data()
{
    QTest::addColumn<QString>("configName");
    QTest::newRow("implicit config name") << QString();
    QTest::newRow("explicit config name") << QString("customConfig");
}

void TestBlackbox::missingDependency()
{
    QDir::setCurrent(testDataDir + "/missing-dependency");
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << "-p" << "theApp";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(!m_qbsStderr.contains("ASSERT"), m_qbsStderr.constData());
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "-p" << "theDep")), 0);
    params.expectFailure = false;
    params.arguments << "-vv";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStderr.contains("false positive"));
}

void TestBlackbox::missingProjectFile()
{
    QDir::setCurrent(testDataDir + "/missing-project-file/empty-dir");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("No project file given and none found in current directory"),
             m_qbsStderr.constData());
    QDir::setCurrent(testDataDir + "/missing-project-file");
    params.arguments << "-f" << "empty-dir";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("No project file found in directory"), m_qbsStderr.constData());
    params.arguments = QStringList() << "-f" << "ambiguous-dir";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("More than one project file found in directory"),
             m_qbsStderr.constData());
    params.expectFailure = false;
    params.arguments = QStringList() << "-f" << "project-dir";
    QCOMPARE(runQbs(params), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    touch("project-dir/file.cpp");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling file.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::missingOverridePrefix()
{
    QDir::setCurrent(testDataDir + "/missing-override-prefix");
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << "blubb.whatever:false";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Property override key 'blubb.whatever' not understood"),
             m_qbsStderr.constData());
}

void TestBlackbox::movedFileDependency()
{
    QDir::setCurrent(testDataDir + "/moved-file-dependency");
    const QString subdir2 = QDir::currentPath() + "/subdir2";
    QVERIFY(QDir::current().mkdir(subdir2));
    const QString oldHeaderFilePath = QDir::currentPath() + "/subdir1/theheader.h";
    const QString newHeaderFilePath = subdir2 + "/theheader.h";
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());

    QFile f(oldHeaderFilePath);
    QVERIFY2(f.rename(newHeaderFilePath), qPrintable(f.errorString()));
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());

    f.setFileName(newHeaderFilePath);
    QVERIFY2(f.rename(oldHeaderFilePath), qPrintable(f.errorString()));
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::badInterpreter()
{
    if (!HostOsInfo::isAnyUnixHost())
        QSKIP("only applies on Unix");

    QDir::setCurrent(testDataDir + QLatin1String("/badInterpreter"));
    QCOMPARE(runQbs(), 0);

    QbsRunParameters params("run");
    params.expectFailure = true;

    const QRegExp reNoSuchFileOrDir("bad interpreter:.* No such file or directory");
    const QRegExp rePermissionDenied("bad interpreter:.* Permission denied");

    params.arguments = QStringList() << "-p" << "script-interp-missing";
    QCOMPARE(runQbs(params), 1);
    QString strerr = QString::fromLocal8Bit(m_qbsStderr);
    QVERIFY(strerr.contains(reNoSuchFileOrDir));

    params.arguments = QStringList() << "-p" << "script-interp-noexec";
    QCOMPARE(runQbs(params), 1);
    strerr = QString::fromLocal8Bit(m_qbsStderr);
    QVERIFY(strerr.contains(reNoSuchFileOrDir) || strerr.contains(rePermissionDenied));

    params.arguments = QStringList() << "-p" << "script-noexec";
    QCOMPARE(runQbs(params), 1);
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList() << "-p" << "script-ok")), 0);
}

void TestBlackbox::qbsVersion()
{
    const auto v = qbs::LanguageInfo::qbsVersion();
    QDir::setCurrent(testDataDir + QLatin1String("/qbsVersion"));
    QbsRunParameters params;
    params.arguments = QStringList()
            << "project.qbsVersion:" + v.toString()
            << "project.qbsVersionMajor:" + QString::number(v.majorVersion())
            << "project.qbsVersionMinor:" + QString::number(v.minorVersion())
            << "project.qbsVersionPatch:" + QString::number(v.patchLevel());
    QCOMPARE(runQbs(params), 0);

    params.arguments.push_back("project.qbsVersionPatch:" + QString::number(v.patchLevel() + 1));
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
}

void TestBlackbox::transitiveOptionalDependencies()
{
    QDir::setCurrent(testDataDir + "/transitive-optional-dependencies");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::groupsInModules()
{
    QDir::setCurrent(testDataDir + "/groups-in-modules");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compile rock.coal => rock.diamond"));
    QVERIFY(m_qbsStdout.contains("compile chunk.coal => chunk.diamond"));
    QVERIFY(m_qbsStdout.contains("compiling helper2.c"));
    QVERIFY(!m_qbsStdout.contains("compiling helper3.c"));
    QVERIFY(m_qbsStdout.contains("compiling helper4.c"));
    QVERIFY(m_qbsStdout.contains("compiling helper5.c"));
    QVERIFY(!m_qbsStdout.contains("compiling helper6.c"));

    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compile rock.coal => rock.diamond"));
    QVERIFY(!m_qbsStdout.contains("compile chunk.coal => chunk.diamond"));

    WAIT_FOR_NEW_TIMESTAMP();
    touch("modules/helper/diamondc.c");

    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling diamondc.c"));
    QVERIFY(m_qbsStdout.contains("compile rock.coal => rock.diamond"));
    QVERIFY(m_qbsStdout.contains("compile chunk.coal => chunk.diamond"));
    QVERIFY(regularFileExists(relativeProductBuildDir("groups-in-modules") + "/rock.diamond"));
    QFile output(relativeProductBuildDir("groups-in-modules") + "/rock.diamond");
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(output.readAll().trimmed(), QByteArray("diamond"));
}

void TestBlackbox::ico()
{
    QDir::setCurrent(testDataDir + "/ico");
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << "--command-echo-mode" << "command-line";
    const int status = runQbs(params);
    if (status != 0) {
        if (m_qbsStderr.contains("Could not find icotool in any of the following locations:"))
            QSKIP("icotool is not installed");
        if (!m_qbsStderr.isEmpty())
            qDebug("%s", m_qbsStderr.constData());
        if (!m_qbsStdout.isEmpty())
            qDebug("%s", m_qbsStdout.constData());
    }
    QCOMPARE(status, 0);

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("icon") + "/icon.ico"));
    {
        QFile f(relativeProductBuildDir("icon") + "/icon.ico");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll().toStdString();
        QCOMPARE(b.at(2), '\x1'); // icon
        QCOMPARE(b.at(4), '\x2'); // 2 images
        QVERIFY(b.find("\x89PNG") == std::string::npos);
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("icon-alpha") + "/icon-alpha.ico"));
    {
        QFile f(relativeProductBuildDir("icon-alpha") + "/icon-alpha.ico");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll().toStdString();
        QCOMPARE(b.at(2), '\x1'); // icon
        QCOMPARE(b.at(4), '\x2'); // 2 images
        QVERIFY(b.find("\x89PNG") == std::string::npos);
        QVERIFY2(m_qbsStdout.contains("--alpha-threshold="), m_qbsStdout.constData());
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("icon-big") + "/icon-big.ico"));
    {
        QFile f(relativeProductBuildDir("icon-big") + "/icon-big.ico");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll().toStdString();
        QCOMPARE(b.at(2), '\x1'); // icon
        QCOMPARE(b.at(4), '\x5'); // 5 images
        QVERIFY(b.find("\x89PNG") != std::string::npos);
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("cursor") + "/cursor.cur"));
    {
        QFile f(relativeProductBuildDir("cursor") + "/cursor.cur");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll();
        QVERIFY(b.size() > 0);
        QCOMPARE(b.at(2), '\x2'); // cursor
        QCOMPARE(b.at(4), '\x2'); // 2 images
        QCOMPARE(b.at(10), '\0');
        QCOMPARE(b.at(12), '\0');
        QCOMPARE(b.at(26), '\0');
        QCOMPARE(b.at(28), '\0');
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("cursor-hotspot") + "/cursor-hotspot.cur"));
    {
        QFile f(relativeProductBuildDir("cursor-hotspot") + "/cursor-hotspot.cur");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll();
        QVERIFY(b.size() > 0);
        QCOMPARE(b.at(2), '\x2'); // cursor
        QCOMPARE(b.at(4), '\x2'); // 2 images
        const bool hasCursorHotspotBug = m_qbsStderr.contains(
                                                              "does not support setting the hotspot for cursor files with multiple images");
        if (hasCursorHotspotBug) {
            QCOMPARE(b.at(10), '\0');
            QCOMPARE(b.at(12), '\0');
            QCOMPARE(b.at(26), '\0');
            QCOMPARE(b.at(28), '\0');
            QWARN("this version of icoutil does not support setting the hotspot "
                  "for cursor files with multiple images");
        } else {
            QCOMPARE(b.at(10), '\x8');
            QCOMPARE(b.at(12), '\x9');
            QCOMPARE(b.at(26), '\x10');
            QCOMPARE(b.at(28), '\x11');
        }
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("cursor-hotspot-single")
                              + "/cursor-hotspot-single.cur"));
    {
        QFile f(relativeProductBuildDir("cursor-hotspot-single") + "/cursor-hotspot-single.cur");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll();
        QVERIFY(b.size() > 0);
        QCOMPARE(b.at(2), '\x2'); // cursor
        QCOMPARE(b.at(4), '\x1'); // 1 image

        // No version check needed because the hotspot can always be set if there's only one image
        QCOMPARE(b.at(10), '\x8');
        QCOMPARE(b.at(12), '\x9');
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("iconset") + "/dmg.ico"));
    {
        QFile f(relativeProductBuildDir("iconset") + "/dmg.ico");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll();
        QVERIFY(b.size() > 0);
        QCOMPARE(b.at(2), '\x1'); // icon
        QCOMPARE(b.at(4), '\x5'); // 5 images
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("iconset") + "/dmg.cur"));
    {
        QFile f(relativeProductBuildDir("iconset") + "/dmg.cur");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll();
        QVERIFY(b.size() > 0);
        QCOMPARE(b.at(2), '\x2'); // cursor
        QCOMPARE(b.at(4), '\x5'); // 5 images
        QCOMPARE(b.at(10), '\0');
        QCOMPARE(b.at(12), '\0');
        QCOMPARE(b.at(26), '\0');
        QCOMPARE(b.at(28), '\0');
    }
}

void TestBlackbox::importChangeTracking()
{
    QDir::setCurrent(testDataDir + "/import-change-tracking");
    QCOMPARE(runQbs(QStringList({"-f", "import-change-tracking.qbs"})), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running probe1"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in imported file that is not used in any rule or command.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("irrelevant.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe1 "), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in directly imported file only used by one prepare script.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("custom1prepare1.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe1 "), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in recursively imported file only used by one prepare script.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("custom1prepare2.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe1 "), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in imported file used only by one command.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("custom1command.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe1 "), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in file only used by one prepare script, using directory import.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("custom2prepare/custom2prepare2.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe1 "), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in file used only by one command, imported via search path.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("imports/custom2command/custom2command1.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe1 "), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in directly imported file only used by one Probe
    WAIT_FOR_NEW_TIMESTAMP();
    touch("probe1.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running probe1"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in indirectly imported file only used by one Probe
    WAIT_FOR_NEW_TIMESTAMP();
    touch("probe2.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running probe1"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change everything at once.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("irrelevant.js");
    touch("custom1prepare1.js");
    touch("custom1prepare2.js");
    touch("custom1command.js");
    touch("custom2prepare/custom2prepare1.js");
    touch("imports/custom2command/custom2command2.js");
    touch("probe2.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running probe1"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());
}

void TestBlackbox::probesInNestedModules()
{
    QDir::setCurrent(testDataDir + "/probes-in-nested-modules");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);

    QCOMPARE(m_qbsStdout.count("running probe a"), 1);
    QCOMPARE(m_qbsStdout.count("running probe b"), 1);
    QCOMPARE(m_qbsStdout.count("running probe c"), 1);
    QCOMPARE(m_qbsStdout.count("running second probe a"), 1);

    QVERIFY(m_qbsStdout.contains("product a, outer.somethingElse = goodbye"));
    QVERIFY(m_qbsStdout.contains("product b, inner.something = hahaha"));
    QVERIFY(m_qbsStdout.contains("product c, inner.something = hello"));

    QVERIFY(m_qbsStdout.contains("product a, inner.something = hahaha"));
    QVERIFY(m_qbsStdout.contains("product a, outer.something = hahaha"));
}

QTEST_MAIN(TestBlackbox)
