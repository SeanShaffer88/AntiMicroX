// SPDX-License-Identifier: GPL-3.0-or-later

#include "antimicrosettings.h"
#include "antkeymapper.h"
#include "eventhandlerfactory.h"
#include "xmlconfigreader.h"

#include "gamecontroller/gamecontroller.h"
#include "gamecontroller/gamecontrollerset.h"
#include "joyaxis.h"
#include "joybuttontypes/joybutton.h"
#include "joystick.h"
#include "logger.h"
#include "profileformat.h"
#include "vdpad.h"
#include "xml/joyaxisxml.h"
#include "xml/joybuttonxml.h"
#include <QDirIterator>
#include <QFile>

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QtTest>

#include <memory>

#if !SDL_VERSION_ATLEAST(2, 0, 14)
    #error "Profile XML tests require SDL 2.0.14 or newer for virtual joysticks"
#endif

class TestGameControllerSetXml : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void malformedProfile_data();
    void malformedProfile();
    void retryAfterMalformedProfile();
    void futureConfigVersionRejected();
    void legacyMigration_data();
    void legacyMigration();
    void communityProfiles_data();
    void communityProfiles();
    void invalidElements_data();
    void invalidElements();
    void triggerAliases_data();
    void triggerAliases();
    void validDPad();
    void reloadDoesNotRetainParsers();

  private:
    void readSet(const QString &contents);

    QTemporaryDir m_directory;
    std::unique_ptr<AntiMicroSettings> m_settings;
    std::unique_ptr<GameController> m_device;
    SDL_GameController *m_controller = nullptr;
    int m_virtual_index = -1;
};

void TestGameControllerSetXml::initTestCase()
{
    QVERIFY(m_directory.isValid());
    QCoreApplication::setOrganizationName("AntiMicroXTests");
    QCoreApplication::setApplicationName("ProfileXmlTests");
    QStandardPaths::setTestModeEnabled(true);
    Logger::createInstance(nullptr, Logger::LOG_NONE);
    QCOMPARE(SDL_Init(SDL_INIT_GAMECONTROLLER), 0);
#ifdef Q_OS_WIN
    AntKeyMapper::getInstance("sendinput");
    EventHandlerFactory::getInstance("sendinput");
#elif defined(WITH_UINPUT)
    AntKeyMapper::getInstance("uinput");
    EventHandlerFactory::getInstance("uinput");
#endif
    m_settings = std::make_unique<AntiMicroSettings>(m_directory.filePath("settings.ini"), QSettings::IniFormat);
}

void TestGameControllerSetXml::cleanupTestCase()
{
    m_settings.reset();
    SDL_Quit();
#if defined(Q_OS_WIN) || defined(WITH_UINPUT)
    AntKeyMapper::getInstance()->deleteInstance();
    EventHandlerFactory::getInstance()->deleteInstance();
#endif
    delete Logger::getInstance();
}

void TestGameControllerSetXml::init()
{
    m_virtual_index =
        SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    QVERIFY2(m_virtual_index >= 0, SDL_GetError());
    m_controller = SDL_GameControllerOpen(m_virtual_index);
    QVERIFY2(m_controller != nullptr, SDL_GetError());
    m_device = std::make_unique<GameController>(m_controller, m_virtual_index, m_settings.get(), 0, nullptr);
}

void TestGameControllerSetXml::cleanup()
{
    m_device.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if (m_controller != nullptr)
        SDL_GameControllerClose(m_controller);
    m_controller = nullptr;
    if (m_virtual_index >= 0)
        QCOMPARE(SDL_JoystickDetachVirtual(m_virtual_index), 0);
    m_virtual_index = -1;
}

void TestGameControllerSetXml::readSet(const QString &contents)
{
    QXmlStreamReader xml(QStringLiteral("<set index=\"1\">") + contents + QStringLiteral("</set>"));
    QVERIFY(xml.readNextStartElement());
    m_device->getActiveSetJoystick()->readConfig(&xml);
    QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
    QCOMPARE(xml.tokenType(), QXmlStreamReader::EndElement);
    QCOMPARE(xml.name().toString(), QStringLiteral("set"));
}

void TestGameControllerSetXml::invalidElements_data()
{
    QTest::addColumn<QString>("element");
    QTest::addColumn<QString>("index");
    const QStringList elements = {"button", "dpad", "trigger", "stick"};
    const QStringList indices = {"", "0", "-1", "-2147483648", "2147483647", "2147483648", "abc", "999"};
    for (const QString &element : elements)
    {
        for (const QString &index : indices)
        {
            const QByteArray row = (element + ":" + index).toUtf8();
            QTest::newRow(row.constData()) << element << index;
        }
    }
    QTest::newRow("unsupported-trigger-3") << QString("trigger") << QString("3");
    QTest::newRow("unsupported-trigger-4") << QString("trigger") << QString("4");
}

void TestGameControllerSetXml::invalidElements()
{
    QFETCH(QString, element);
    QFETCH(QString, index);
    // Nested names must not escape a rejected element; subsequent valid mappings must still load.
    readSet(QStringLiteral("<name>Original</name><%1 index=\"%2\"><name>Ignored</name>"
                           "<button index=\"2\"><toggle>true</toggle></button></%1>"
                           "<button index=\"1\"><toggle>true</toggle></button>")
                .arg(element, index));
    auto *set = m_device->getActiveSetJoystick();
    QCOMPARE(set->getName(), QStringLiteral("Original"));
    QVERIFY(set->getJoyButton(0)->getToggleState());
    QVERIFY(!set->getJoyButton(1)->getToggleState());
}

void TestGameControllerSetXml::triggerAliases_data()
{
    QTest::addColumn<int>("index");
    QTest::addColumn<int>("axis");
    QTest::newRow("legacy-left") << 1 << int(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    QTest::newRow("legacy-right") << 2 << int(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    QTest::newRow("left") << 5 << int(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    QTest::newRow("right") << 6 << int(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
}

void TestGameControllerSetXml::triggerAliases()
{
    QFETCH(int, index);
    QFETCH(int, axis);
    readSet(QStringLiteral("<trigger index=\"%1\"><deadZone>4321</deadZone></trigger>"
                           "<button index=\"1\"><toggle>true</toggle></button>")
                .arg(index));
    auto *set = m_device->getActiveSetJoystick();
    QCOMPARE(set->getJoyAxis(axis)->getDeadZone(), 4321);
    QVERIFY(set->getJoyButton(0)->getToggleState());
}

void TestGameControllerSetXml::validDPad()
{
    readSet(QStringLiteral("<dpad index=\"1\"><mode>eight-way</mode></dpad>"
                           "<button index=\"1\"><toggle>true</toggle></button>"));
    auto *set = m_device->getActiveSetJoystick();
    QCOMPARE(set->getVDPad(0)->getJoyMode(), JoyDPad::EightWayMode);
    QVERIFY(set->getJoyButton(0)->getToggleState());
}

void TestGameControllerSetXml::reloadDoesNotRetainParsers()
{
    auto *set = m_device->getActiveSetJoystick();
    for (int i = 0; i < 20; i++)
        readSet(QStringLiteral("<button index=\"1\"><toggle>true</toggle></button>"
                               "<trigger index=\"5\"><deadZone>4321</deadZone></trigger>"
                               "<dpad index=\"1\"><mode>eight-way</mode></dpad>"));
    QVERIFY(set->findChildren<JoyButtonXml *>(QString(), Qt::FindDirectChildrenOnly).isEmpty());
    QVERIFY(set->findChildren<JoyAxisXml *>(QString(), Qt::FindDirectChildrenOnly).isEmpty());
}

void TestGameControllerSetXml::malformedProfile_data()
{
    QTest::addColumn<QByteArray>("contents");
    const QByteArray prefix = "<gamecontroller configversion=\"19\"><profilename>Changed</profilename>";
    QTest::newRow("truncated") << prefix + "<sets>";
    QTest::newRow("mismatched") << prefix + "</wrong>";
    QTest::newRow("undefined-entity") << prefix + "<description>&missing;</description></gamecontroller>";
    QTest::newRow("trailing-junk") << prefix + "</gamecontroller>junk";
    QTest::newRow("multiple-roots") << prefix + "</gamecontroller><gamecontroller/>";
}
void TestGameControllerSetXml::malformedProfile()
{
    QFETCH(QByteArray, contents);
    const QString path = m_directory.filePath("malformed.amgp");
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), qint64(contents.size()));
    }
    m_device->setProfileName("Before");
    m_device->getActiveSetJoystick()->getJoyButton(0)->setToggle(true);
    XMLConfigReader reader;
    reader.setJoystick(m_device.get());
    reader.setFileName(path);
    QVERIFY(reader.read());
    QVERIFY(reader.hasError());
    QVERIFY(!reader.getErrorString().isEmpty());
    QCOMPARE(m_device->getProfileName(), QString("Before"));
    QVERIFY(m_device->getActiveSetJoystick()->getJoyButton(0)->getToggleState());
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), contents);
}
void TestGameControllerSetXml::retryAfterMalformedProfile()
{
    const QString path = m_directory.filePath("retry.amgp");
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("<gamecontroller configversion=\"19\"><profilename>Changed</profilename><sets>") > 0);
    }
    m_device->setProfileName("Before");
    XMLConfigReader reader;
    reader.setJoystick(m_device.get());
    reader.setFileName(path);
    QVERIFY(reader.read());
    QCOMPARE(m_device->getProfileName(), QString("Before"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("<gamecontroller configversion=\"19\"><profilename>Loaded</profilename>"
                           "<sets><set index=\"1\"><button index=\"1\"><toggle>true</toggle></button></set></sets>"
                           "</gamecontroller>") > 0);
    }
    QVERIFY(!reader.read());
    QVERIFY(!reader.hasError());
    QCOMPARE(m_device->getProfileName(), QString("Loaded"));
    QVERIFY(m_device->getActiveSetJoystick()->getJoyButton(0)->getToggleState());
}
void TestGameControllerSetXml::futureConfigVersionRejected()
{
    const QString path = m_directory.filePath("future.amgp");
    const QByteArray contents =
        "<gamecontroller configversion=\"20\" appversion=\"99.1.2\"><profilename>Changed</profilename>"
        "<sets><set index=\"1\"><button index=\"1\"><toggle>true</toggle></button></set></sets></gamecontroller>";
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), qint64(contents.size()));
    }
    m_device->setProfileName("Before");
    m_device->getActiveSetJoystick()->getJoyButton(0)->setToggle(false);
    XMLConfigReader reader;
    reader.setJoystick(m_device.get());
    reader.setFileName(path);
    // A profile using a newer format must not be partially applied.
    QVERIFY(reader.read());
    QVERIFY(reader.hasError());
    QVERIFY2(reader.getErrorString().contains(QString::number(ProfileFormat::LATEST_FILE_VERSION + 1)),
             qPrintable(reader.getErrorString()));
    QVERIFY2(reader.getErrorString().contains("99.1.2"), qPrintable(reader.getErrorString()));
    QCOMPARE(m_device->getProfileName(), QString("Before"));
    QVERIFY(!m_device->getActiveSetJoystick()->getJoyButton(0)->getToggleState());
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), contents);
    }
    // The same content becomes readable once it declares a supported version.
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QByteArray supported = contents;
        supported.replace("configversion=\"20\"",
                          "configversion=\"" + QByteArray::number(ProfileFormat::LATEST_FILE_VERSION) + "\"");
        QCOMPARE(file.write(supported), qint64(supported.size()));
    }
    QVERIFY(!reader.read());
    QVERIFY(!reader.hasError());
    QCOMPARE(m_device->getProfileName(), QString("Changed"));
    QVERIFY(m_device->getActiveSetJoystick()->getJoyButton(0)->getToggleState());
}
void TestGameControllerSetXml::legacyMigration_data()
{
    QTest::addColumn<int>("version");
    QTest::addColumn<QString>("kind");
    for (int version = 2; version <= 5; ++version)
    {
        for (const QString &kind : {QString("unicode"), QString("empty"), QString("truncated")})
            QTest::newRow(QString("v%1-%2").arg(version).arg(kind).toUtf8().constData()) << version << kind;
    }
}
void TestGameControllerSetXml::legacyMigration()
{
    QFETCH(int, version);
    QFETCH(QString, kind);
    const QString name = QString::fromUtf8("Caf\xc3\xa9 \xe6\xbc\xa2 Game \xf0\x9f\x8e\xae");
    QString source = QString("<joystick configversion=\"%1\"").arg(version);
    if (kind == "empty")
        source += "/>";
    else if (kind == "truncated")
        source += "><profilename>Changed";
    else
        source += QString("><profilename>%1</profilename><sets><set index=\"1\">"
                          "<button index=\"1\"><toggle>true</toggle></button></set></sets></joystick>")
                      .arg(name);
    const QByteArray original = source.toUtf8();
    const QString path = m_directory.filePath("legacy.amgp");
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(original), qint64(original.size()));
    }
    const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 2, 0);
    QVERIFY2(index >= 0, SDL_GetError());
    SDL_Joystick *handle = SDL_JoystickOpen(index);
    if (handle == nullptr)
    {
        SDL_JoystickDetachVirtual(index);
        QFAIL(SDL_GetError());
    }
    bool error = false;
    bool has_error = false;
    bool toggle = false;
    QString loaded_name;
    {
        Joystick device(handle, index, m_settings.get(), nullptr);
        device.setProfileName("Before");
        device.getActiveSetJoystick()->getJoyButton(1)->setToggle(true);
        XMLConfigReader reader;
        reader.setJoystick(&device);
        reader.setFileName(path);
        error = reader.read();
        has_error = reader.hasError();
        loaded_name = device.getProfileName();
        toggle = device.getActiveSetJoystick()->getJoyButton(kind == "truncated" ? 1 : 0)->getToggleState();
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    SDL_JoystickClose(handle);
    QCOMPARE(SDL_JoystickDetachVirtual(index), 0);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray saved = file.readAll();
    if (kind == "truncated")
    {
        QVERIFY(error);
        QVERIFY(has_error);
        QCOMPARE(loaded_name, QString("Before"));
        QVERIFY(toggle);
        QCOMPARE(saved, original);
    } else
    {
        QVERIFY(!error);
        QVERIFY(!has_error);
        QXmlStreamReader xml(saved);
        QVERIFY(xml.readNextStartElement());
        QCOMPARE(xml.attributes().value("configversion").toString(), QString("6"));
        while (!xml.atEnd())
            xml.readNext();
        QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
        if (kind == "unicode")
        {
            QCOMPARE(loaded_name, name);
            QVERIFY(toggle);
            QVERIFY(saved.contains(name.toUtf8()));
        }
    }
}
void TestGameControllerSetXml::communityProfiles_data()
{
    QTest::addColumn<QString>("path");
    const QString directory = QString::fromLocal8Bit(qgetenv("ANTIMICROX_PROFILE_CORPUS"));
    if (directory.isEmpty())
        QSKIP("Set ANTIMICROX_PROFILE_CORPUS to the pinned profile corpus applications directory");
#if !defined(Q_OS_WIN) && !defined(WITH_UINPUT)
    QSKIP("Community keyboard bindings require the uinput or sendinput key mapper");
#endif
    QVERIFY(QDir(directory).exists());
    QDirIterator files(directory, {"*.gamecontroller.amgp"}, QDir::Files, QDirIterator::Subdirectories);
    int count = 0;
    while (files.hasNext())
    {
        const QString path = files.next();
        QTest::newRow(QDir(directory).relativeFilePath(path).toUtf8().constData()) << path;
        ++count;
    }
    QVERIFY2(count > 0, "The profile corpus contained no matching files");
}
void TestGameControllerSetXml::communityProfiles()
{
    QFETCH(QString, path);
    QFile before(path);
    QVERIFY(before.open(QIODevice::ReadOnly));
    const QByteArray original = before.readAll();
    before.close();
    XMLConfigReader reader;
    reader.setJoystick(m_device.get());
    reader.setFileName(path);
    QVERIFY2(!reader.read(), qPrintable(reader.getErrorString()));
    QVERIFY(!reader.hasError());
    QFile after(path);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QCOMPARE(after.readAll(), original);
}
QTEST_MAIN(TestGameControllerSetXml)
#include "testgamecontrollersetxml.moc"
