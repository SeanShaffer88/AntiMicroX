// SPDX-License-Identifier: GPL-3.0-or-later

#include "translations.h"

#include <QCoreApplication>
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
    QString createTranslation(const QString &relativePath) const;

    QTemporaryDir m_directory;
};

void TestTranslations::initTestCase() { QVERIFY(m_directory.isValid()); }

QString TestTranslations::createTranslation(const QString &relativePath) const
{
    // QVERIFY and QCOMPARE may not be used here: this function returns a value.
    const QString path = m_directory.filePath(relativePath);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return QString();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return QString();
    // Content is irrelevant: only the file lookup is exercised here.
    file.write("not-a-real-translation");
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
    QTest::newRow("other-section") << QString("[Other]\nLanguage=pl\n") << QString("pl");
    QTest::newRow("surrounding-space") << QString("[General]\nLanguage=  zh_CN  \n") << QString("zh_CN");
    QTest::newRow("empty-value") << QString("[General]\nLanguage=\n") << QString();
}

void TestTranslations::languageSetting()
{
    QFETCH(QString, ini);
    QFETCH(QString, expected);
    const QString path = m_directory.filePath("language-setting.ini");
    QFile::remove(path);

    if (!ini.isNull())
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(ini.toUtf8()) > 0);
    }

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
    QTest::newRow("lower-case") << QString("language=de\nSurvives=1\n") << true;
    QTest::newRow("german-value") << QString("[General]\nLanguage=en\n") << true;
    QTest::newRow("nothing-stored") << QString("[General]\nSurvives=1\n") << false;
}

void TestTranslations::clearLanguageSetting()
{
    QFETCH(QString, ini);
    QFETCH(bool, removed);
    const QString path = m_directory.filePath("clear-language-setting.ini");
    QFile::remove(path);
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(ini.toUtf8()) > 0);
    }

    QSettings settings(path, QSettings::IniFormat);
    QCOMPARE(Translations::clearLanguageSetting(settings), removed);
    settings.sync();

    QSettings reread(path, QSettings::IniFormat);
    QVERIFY2(Translations::languageSetting(reread).isEmpty(), qPrintable(Translations::languageSetting(reread)));
    if (ini.contains("Survives"))
        QCOMPARE(reread.value("Survives").toInt(), 1);
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
    const QString expected = createTranslation("install/share/antimicrox/translations/antimicrox_de.qm");
    QVERIFY(!expected.isEmpty());
    QCOMPARE(Translations::findAppTranslation("de", binaryDir), expected);
    QVERIFY(Translations::findAppTranslation("fr", binaryDir).isEmpty());
}

void TestTranslations::prefersFlatpakInstallation()
{
    const QString binaryDir = m_directory.filePath("flatpak/bin");
    const QString local = createTranslation("flatpak/share/antimicrox/translations/antimicrox_pl.qm");
    QVERIFY(!local.isEmpty());
    // No /app tree in the test environment, so the local copy is used.
    if (QFileInfo::exists("/app/share/antimicrox/translations"))
        QSKIP("The host provides a Flatpak translation tree");
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
    const QString expected = createTranslation("qt/translations/qt_de.qm");
    QVERIFY(!expected.isEmpty());
    QCOMPARE(Translations::findQtTranslation("de", qtPath, QString()), expected);
    QCOMPARE(Translations::findQtTranslation("de", QString(), QString()), QString());
}

QTEST_GUILESS_MAIN(TestTranslations)
#include "testtranslations.moc"
