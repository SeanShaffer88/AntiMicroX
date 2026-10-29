// SPDX-License-Identifier: GPL-3.0-or-later

#include "profileformat.h"
#include "xml/inputdevicexml.h"
#include "xmlconfigwriter.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtTest>

#include <functional>

// Link a controlled serializer instead of the SDL/device implementation. The
// production XMLConfigWriter is compiled directly, with no test-only hooks.
static std::function<void(QXmlStreamWriter *)> serialize;
InputDeviceXml::InputDeviceXml(InputDevice *device, QObject *parent)
    : QObject(parent)
    , m_inputDevice(device)
{
}
void InputDeviceXml::writeConfig(QXmlStreamWriter *xml) { serialize(xml); }
void InputDeviceXml::readConfig(QXmlStreamReader *) { QFAIL("Reader is outside the writer unit test"); }

class FailingDevice : public QIODevice
{
  public:
    FailingDevice() { open(QIODevice::WriteOnly); }

  protected:
    qint64 readData(char *, qint64) override { return -1; }
    qint64 writeData(const char *, qint64) override { return -1; }
};

class TestXmlConfigWriter : public QObject
{
    Q_OBJECT

  private slots:
    void init();
    void cleanup();
    void savesUtf8Xml();
    void preservesOldProfileUntilCommit();
    void serializationFailure_data();
    void serializationFailure();
    void canceledWritePreservesProfile();
    void commitFailureIsReported();
    void missingDirectoryIsReported();
    void missingFilenameIsReported();
    void nullSerializerPreservesProfile();
    void retryClearsError();
    void changingFilenameReleasesPreviousFile();
    void existingProfileVersion_data();
    void existingProfileVersion();

  private:
    QByteArray readFile(const QString &path);
    void seedProfile();
    QTemporaryDir m_directory;
    QString m_path;
};

void TestXmlConfigWriter::init()
{
    QVERIFY(m_directory.isValid());
    const QString directory = m_directory.filePath(QString::fromLatin1(QTest::currentTestFunction()) +
                                                   QString::fromLatin1(QTest::currentDataTag()));
    QVERIFY(QDir().mkpath(directory));
    m_path = directory + "/profile.amgp";
    serialize = [](QXmlStreamWriter *xml) {
        xml->writeStartElement("joystick");
        xml->writeTextElement("profilename", QString::fromUtf8("Caf\xc3\xa9 controller"));
        xml->writeEndElement();
    };
}

void TestXmlConfigWriter::cleanup() { serialize = nullptr; }

QByteArray TestXmlConfigWriter::readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll();
}

void TestXmlConfigWriter::seedProfile()
{
    QFile file(m_path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("original profile"), qint64(16));
}

void TestXmlConfigWriter::savesUtf8Xml()
{
    XMLConfigWriter writer;
    writer.setFileName(m_path);
    InputDeviceXml device(nullptr);
    writer.write(&device);
    QVERIFY2(!writer.hasError(), qPrintable(writer.getErrorString()));
    const QByteArray data = readFile(m_path);
    QVERIFY(data.contains("Caf\xc3\xa9 controller"));
    QXmlStreamReader xml(data);
    QVERIFY(xml.readNextStartElement());
    QCOMPARE(xml.name().toString(), QString("joystick"));
    QVERIFY(xml.readNextStartElement());
    QCOMPARE(xml.readElementText(), QString::fromUtf8("Caf\xc3\xa9 controller"));
    while (!xml.atEnd())
        xml.readNext();
    QVERIFY(!xml.hasError());
    QVERIFY(writer.getXml()->device() == nullptr);
}

void TestXmlConfigWriter::preservesOldProfileUntilCommit()
{
    seedProfile();
    bool oldProfileVisible = false;
    serialize = [&](QXmlStreamWriter *xml) {
        xml->writeStartElement("joystick");
        oldProfileVisible = readFile(m_path) == QByteArray("original profile");
        xml->writeEndElement();
    };
    XMLConfigWriter writer;
    writer.setFileName(m_path);
    InputDeviceXml device(nullptr);
    writer.write(&device);
    QVERIFY(oldProfileVisible);
    QVERIFY2(!writer.hasError(), qPrintable(writer.getErrorString()));
    QVERIFY(readFile(m_path).contains("joystick"));
}

void TestXmlConfigWriter::serializationFailure_data()
{
    QTest::addColumn<bool>("existing");
    QTest::newRow("existing-profile") << true;
    QTest::newRow("new-profile") << false;
}

void TestXmlConfigWriter::serializationFailure()
{
    QFETCH(bool, existing);
    if (existing)
        seedProfile();
    const QStringList before = QDir(QFileInfo(m_path).absolutePath()).entryList(QDir::AllEntries | QDir::Hidden);
    FailingDevice failing;
    serialize = [&](QXmlStreamWriter *xml) {
        xml->setDevice(&failing);
        xml->writeStartElement("joystick");
    };
    XMLConfigWriter writer;
    writer.setFileName(m_path);
    InputDeviceXml device(nullptr);
    writer.write(&device);
    QVERIFY(writer.hasError());
    QVERIFY(!writer.getErrorString().isEmpty());
    if (existing)
        QCOMPARE(readFile(m_path), QByteArray("original profile"));
    else
        QVERIFY(!QFileInfo::exists(m_path));
    QCOMPARE(QDir(QFileInfo(m_path).absolutePath()).entryList(QDir::AllEntries | QDir::Hidden), before);
}

void TestXmlConfigWriter::canceledWritePreservesProfile()
{
    seedProfile();
    serialize = [](QXmlStreamWriter *xml) {
        xml->writeStartElement("joystick");
        auto *file = qobject_cast<QSaveFile *>(xml->device());
        QVERIFY(file != nullptr);
        file->cancelWriting();
        xml->writeEndElement();
    };
    XMLConfigWriter writer;
    writer.setFileName(m_path);
    InputDeviceXml device(nullptr);
    writer.write(&device);
    QVERIFY(writer.hasError());
    QVERIFY(!writer.getErrorString().isEmpty());
    QCOMPARE(readFile(m_path), QByteArray("original profile"));
}

void TestXmlConfigWriter::commitFailureIsReported()
{
    serialize = [&](QXmlStreamWriter *xml) {
        xml->writeStartElement("joystick");
        QVERIFY(QDir().mkdir(m_path));
        xml->writeEndElement();
    };
    XMLConfigWriter writer;
    writer.setFileName(m_path);
    InputDeviceXml device(nullptr);
    writer.write(&device);
    QVERIFY(writer.hasError());
    QVERIFY(!writer.getErrorString().isEmpty());
    QVERIFY(QFileInfo(m_path).isDir());
}

void TestXmlConfigWriter::missingDirectoryIsReported()
{
    XMLConfigWriter writer;
    writer.setFileName(m_path + "/missing/profile.amgp");
    InputDeviceXml device(nullptr);
    writer.write(&device);
    QVERIFY(writer.hasError());
    QVERIFY(!writer.getErrorString().isEmpty());
    QVERIFY(!QFileInfo::exists(m_path));
}

void TestXmlConfigWriter::missingFilenameIsReported()
{
    XMLConfigWriter writer;
    InputDeviceXml device(nullptr);
    writer.write(&device);
    QVERIFY(writer.hasError());
    QVERIFY(!writer.getErrorString().isEmpty());
}

void TestXmlConfigWriter::nullSerializerPreservesProfile()
{
    seedProfile();
    XMLConfigWriter writer;
    writer.setFileName(m_path);
    writer.write(nullptr);
    QVERIFY(writer.hasError());
    QCOMPARE(readFile(m_path), QByteArray("original profile"));
}

void TestXmlConfigWriter::retryClearsError()
{
    XMLConfigWriter writer;
    InputDeviceXml device(nullptr);
    writer.setFileName(m_path + "/missing/profile.amgp");
    writer.write(&device);
    QVERIFY(writer.hasError());
    writer.setFileName(m_path);
    writer.write(&device);
    QVERIFY2(!writer.hasError(), qPrintable(writer.getErrorString()));
    QVERIFY(writer.getErrorString().isEmpty());
}

void TestXmlConfigWriter::existingProfileVersion_data()
{
    QTest::addColumn<int>("version");
    QTest::addColumn<bool>("overwritten");
    QTest::newRow("supported") << ProfileFormat::LATEST_FILE_VERSION << true;
    QTest::newRow("legacy") << 2 << true;
    QTest::newRow("newer") << ProfileFormat::LATEST_FILE_VERSION + 1 << false;
    QTest::newRow("much-newer") << 9999 << false;
}
void TestXmlConfigWriter::existingProfileVersion()
{
    QFETCH(int, version);
    QFETCH(bool, overwritten);
    const QByteArray existing =
        QString("<joystick configversion=\"%1\" appversion=\"99.1.2\"><profilename>Existing</profilename></joystick>")
            .arg(version)
            .toUtf8();
    {
        QFile file(m_path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(existing), qint64(existing.size()));
    }
    XMLConfigWriter writer;
    writer.setFileName(m_path);
    InputDeviceXml device(nullptr);
    writer.write(&device);
    if (overwritten)
    {
        QVERIFY2(!writer.hasError(), qPrintable(writer.getErrorString()));
        QVERIFY(readFile(m_path).contains("Caf\xc3\xa9 controller"));
        return;
    }
    QVERIFY(writer.hasError());
    QVERIFY2(writer.getErrorString().contains(QString::number(version)), qPrintable(writer.getErrorString()));
    QVERIFY2(writer.getErrorString().contains(QString::number(ProfileFormat::LATEST_FILE_VERSION)),
             qPrintable(writer.getErrorString()));
    QCOMPARE(readFile(m_path), existing);
}
void TestXmlConfigWriter::changingFilenameReleasesPreviousFile()
{
    XMLConfigWriter writer;
    writer.setFileName(m_path);
    bool destroyed = false;
    connect(writer.getConfigFile(), &QObject::destroyed, this, [&] { destroyed = true; });
    writer.setFileName(m_path + ".other");
    QVERIFY(destroyed);
    QCOMPARE(writer.getConfigFile()->fileName(), m_path + ".other");
}

QTEST_GUILESS_MAIN(TestXmlConfigWriter)
#include "testxmlconfigwriter.moc"
