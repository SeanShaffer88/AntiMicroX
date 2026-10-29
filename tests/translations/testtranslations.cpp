// SPDX-License-Identifier: GPL-3.0-or-later

#include "translations.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class TestTranslations : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void languageSetting_data();
    void languageSetting();
    void languageSettingMissingFile();
    void clearLanguageSetting_data();
    void clearLanguageSetting();
    void translationFileNames_data();
    void translationFileNames();
    void appTranslationDirs_data();
    void appTranslationDirs();
    void findsInstalledTranslation();
    void prefersFlatpakInstallation();
    void ignoresMissingLanguage();
    void qtTranslationDirs_data();
    void qtTranslationDirs();
    void findsQtTranslation();

  private:
    QString uniquePath(const QString &suffix = QString()) const;
    QString writeFile(const QString &relativePath, const QByteArray &contents) const;

    QTemporaryDir m_directory;
};

void TestTranslations::initTestCase() { QVERIFY(m_directory.isValid()); }

/**
 * A QSettings instance per path is cached for the lifetime of the process and
 * only re-read when the file changes, so every data row needs its own file.
 */
QString TestTranslations::uniquePath(const QString &suffix) const
{
    return m_directory.filePath(QString::fromLatin1(QTest::currentTestFunction()) +
                                QString::fromLatin1(QTest::currentDataTag()) + suffix + QStringLiteral(".ini"));
}

QString TestTranslations::writeFile(const QString &relativePath, const QByteArray &contents) const
{
    // QVERIFY and QCOMPARE may not be used here: this function returns a value.
    const QString path = m_directory.filePath(relativePath);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return QString();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return QString();
    if (file.write(contents) != qint64(contents.size()))
        return QString();
    file.close();
    return path;
}

void TestTranslations::languageSetting_data()
{
    QTest::addColumn<QString>("ini");
    QTest::addColumn<QString>("expected");
    QTest::newRow("no-file") << QString() << QString();
    QTest::newRow("unset") << QString("[General]\nfoo=bar\n") << QString();
    QTest::newRow("lower-case-key") << QString("language=de\n") << QString("de");
    QTest::newRow("upper-case-key") << QString("[General]\nLanguage=de\n") << QString("de");
    QTest::newRow("non-language-value") << QString("[General]\nLanguage=pl\nSurvives=1\n") << QString("pl");
    // Values below another section are not the application's top level key.
    QTest::newRow("other-section") << QString("[Other]\nLanguage=pl\n") << QString();
    QTest::newRow("surrounding-space") << QString("[General]\nLanguage=  zh_CN  \n") << QString("zh_CN");
    QTest::newRow("empty-value") << QString("[General]\nLanguage=\n") << QString();
}

void TestTranslations::languageSetting()
{
    QFETCH(QString, ini);
    QFETCH(QString, expected);
    const QString path = uniquePath();
    QFile::remove(path);

    if (!ini.isNull())
        QVERIFY(!writeFile(QFileInfo(path).fileName(), ini.toUtf8()).isEmpty());

    QSettings settings(path, QSettings::IniFormat);
    QCOMPARE(Translations::languageSetting(settings), expected);
}

void TestTranslations::languageSettingMissingFile()
{
    const QString path = m_directory.filePath("never-written/settings.ini");
    QSettings settings(path, QSettings::IniFormat);
    QVERIFY(Translations::languageSetting(settings).isEmpty());
    QVERIFY(!QFileInfo::exists(path));
}

void TestTranslations::clearLanguageSetting_data()
{
    QTest::addColumn<QString>("ini");
    QTest::addColumn<bool>("removed");
    QTest::newRow("canonical") << QString("[General]\nLanguage=de\nSurvives=1\n") << true;
    QTest::newRow("lower-case-key") << QString("language=de\nSurvives=1\n") << true;
    QTest::newRow("english") << QString("[General]\nLanguage=en\n") << true;
    QTest::newRow("nothing-stored") << QString("[General]\nSurvives=1\n") << false;
}

void TestTranslations::clearLanguageSetting()
{
    QFETCH(QString, ini);
    QFETCH(bool, removed);
    const QString path = uniquePath();
    QVERIFY(!writeFile(QFileInfo(path).fileName(), ini.toUtf8()).isEmpty());

    {
        QSettings settings(path, QSettings::IniFormat);
        QCOMPARE(Translations::clearLanguageSetting(settings), removed);
        QVERIFY(settings.sync());
        QVERIFY2(Translations::languageSetting(settings).isEmpty(), qPrintable(Translations::languageSetting(settings)));
    }

    // Inspect the file itself so the checks do not depend on QSettings caching.
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString contents = QString::fromUtf8(file.readAll());
    QVERIFY2(!contents.contains("language", Qt::CaseInsensitive), qPrintable(contents));
    if (ini.contains("Survives"))
        QVERIFY2(contents.contains("Survives=1"), qPrintable(contents));
}

void TestTranslations::translationFileNames_data()
{
    QTest::addColumn<QString>("language");
    QTest::addColumn<QString>("appFile");
    QTest::addColumn<QString>("qtFile");
    QTest::newRow("simple") << QString("de") << QString("antimicrox_de.qm") << QString("qt_de.qm");
    QTest::newRow("region") << QString("zh_CN") << QString("antimicrox_zh_CN.qm") << QString("qt_zh_CN.qm");
    QTest::newRow("padded") << QString("  pt_BR  ") << QString("antimicrox_pt_BR.qm") << QString("qt_pt_BR.qm");
    QTest::newRow("empty") << QString() << QString() << QString();
    QTest::newRow("blank") << QString("   ") << QString() << QString();
    QTest::newRow("path") << QString("../../etc/passwd") << QString() << QString();
    QTest::newRow("slash") << QString("de/../../x") << QString() << QString();
    QTest::newRow("backslash") << QString("de\\..\\x") << QString() << QString();
}

void TestTranslations::translationFileNames()
{
    QFETCH(QString, language);
    QFETCH(QString, appFile);
    QFETCH(QString, qtFile);
    QCOMPARE(Translations::appTranslationFileName(language), appFile);
    QCOMPARE(Translations::qtTranslationFileName(language), qtFile);
}

void TestTranslations::appTranslationDirs_data()
{
    QTest::addColumn<QString>("applicationDirPath");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("installed") << QString("/usr/bin")
                               << QStringList({"/app/share/antimicrox/translations", "/usr/share/antimicrox/translations",
                                               "/usr/bin/share/antimicrox/translations"});
    QTest::newRow("build-tree") << QString("/home/user/build/bin")
                                << QStringList({"/app/share/antimicrox/translations",
                                                "/home/user/build/share/antimicrox/translations",
                                                "/home/user/build/bin/share/antimicrox/translations"});
    QTest::newRow("AppImage") << QString("/tmp/.mount_anti/usr/bin")
                              << QStringList({"/app/share/antimicrox/translations",
                                              "/tmp/.mount_anti/usr/share/antimicrox/translations",
                                              "/tmp/.mount_anti/usr/bin/share/antimicrox/translations"});
}

void TestTranslations::appTranslationDirs()
{
    QFETCH(QString, applicationDirPath);
    QFETCH(QStringList, expected);
    const QStringList actual = Translations::appTranslationDirs(applicationDirPath);
    QCOMPARE(actual.size(), expected.size());
    for (int index = 0; index < expected.size(); ++index)
        QCOMPARE(QDir::cleanPath(actual.at(index)), QDir::cleanPath(expected.at(index)));
}

void TestTranslations::findsInstalledTranslation()
{
    // <prefix>/bin/../share/antimicrox/translations is the normal install layout.
    const QString binaryDir = m_directory.filePath("install/bin");
    const QString expected = writeFile("install/share/antimicrox/translations/antimicrox_de.qm", "translation");
    QVERIFY(!expected.isEmpty());
    QCOMPARE(Translations::findAppTranslation("de", binaryDir), expected);
    QVERIFY(Translations::findAppTranslation("fr", binaryDir).isEmpty());
}

void TestTranslations::prefersFlatpakInstallation()
{
    if (QFileInfo::exists("/app/share/antimicrox/translations"))
        QSKIP("The host provides a Flatpak translation tree");

    // Without a Flatpak tree the copy next to the executable is used.
    const QString binaryDir = m_directory.filePath("flatpak/bin");
    const QString local = writeFile("flatpak/share/antimicrox/translations/antimicrox_pl.qm", "translation");
    QVERIFY(!local.isEmpty());
    QCOMPARE(Translations::findAppTranslation("pl", binaryDir), local);
}

void TestTranslations::ignoresMissingLanguage()
{
    const QString binaryDir = m_directory.filePath("missing/bin");
    QVERIFY(Translations::findAppTranslation("", binaryDir).isEmpty());
    QVERIFY(Translations::findAppTranslation("../../etc", binaryDir).isEmpty());
    QVERIFY(Translations::findAppTranslation("de", QString()).isEmpty());
}

void TestTranslations::qtTranslationDirs_data()
{
    QTest::addColumn<QString>("qtPath");
    QTest::addColumn<QString>("applicationDirPath");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("library-info") << QString("/usr/share/qt6/translations") << QString("/usr/bin")
                                  << QStringList({"/usr/share/qt6/translations", "/usr/share/qt/translations",
                                                  "/usr/bin/share/qt/translations"});
    QTest::newRow("no-library-info") << QString() << QString("/opt/app/bin")
                                     << QStringList(
                                            {"/opt/app/share/qt/translations", "/opt/app/bin/share/qt/translations"});
    QTest::newRow("duplicates-removed") << QString("/opt/app/share/qt/translations") << QString("/opt/app/bin")
                                        << QStringList(
                                               {"/opt/app/share/qt/translations", "/opt/app/bin/share/qt/translations"});
}

void TestTranslations::qtTranslationDirs()
{
    QFETCH(QString, qtPath);
    QFETCH(QString, applicationDirPath);
    QFETCH(QStringList, expected);
    const QStringList actual = Translations::qtTranslationDirs(qtPath, applicationDirPath);
    QCOMPARE(actual.size(), expected.size());
    for (int index = 0; index < expected.size(); ++index)
        QCOMPARE(QDir::cleanPath(actual.at(index)), QDir::cleanPath(expected.at(index)));
}

void TestTranslations::findsQtTranslation()
{
    const QString qtPath = m_directory.filePath("qt/translations");
    const QString expected = writeFile("qt/translations/qt_de.qm", "translation");
    QVERIFY(!expected.isEmpty());
    QCOMPARE(Translations::findQtTranslation("de", qtPath, QString()), expected);
    QCOMPARE(Translations::findQtTranslation("de", QString(), QString()), QString());
}

QTEST_GUILESS_MAIN(TestTranslations)
#include "testtranslations.moc"
