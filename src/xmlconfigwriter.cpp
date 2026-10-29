/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2015 Travis Nickles <nickles.travis@gmail.com>
 * Copyright (C) 2020 Jagoda Górska <juliagoda.pl@protonmail>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "xmlconfigwriter.h"

#include "profileformat.h"
#include "xml/inputdevicexml.h"

#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {

/**
 * @brief Tell whether an existing profile may not be overwritten by this build
 *
 * Writing over a profile which was created by a newer AntiMicroX would silently
 * discard the elements this build does not understand, so such targets are left
 * untouched. Unreadable or non-profile targets keep their historical behaviour:
 * the write is attempted and any failure is reported by the caller.
 *
 * @param[in] filename Target profile path
 * @param[out] error Message explaining why the file must not be overwritten
 * @return true when the existing file declares an unsupported config version
 */
bool targetDeclaresNewerVersion(const QString &filename, QString &error)
{
    error.clear();
    QFile existing(filename);
    if (!existing.exists() || !existing.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QXmlStreamReader reader(&existing);
    while (!reader.atEnd() && !reader.isStartElement())
        reader.readNext();
    if (!reader.isStartElement())
        return false;

    const QString declared = reader.attributes().value(QStringLiteral("configversion")).toString().trimmed();
    bool converted = false;
    const int version = declared.toInt(&converted);
    if (!converted || (version <= ProfileFormat::LATEST_FILE_VERSION))
        return false;

    error = QCoreApplication::translate(
                "XMLConfigWriter",
                "Could not write to profile at %1.\nThe existing file uses config version %2, which is newer than "
                "the newest config version supported by this build (%3), so it has been left unchanged.")
                .arg(filename, declared, QString::number(ProfileFormat::LATEST_FILE_VERSION));
    return true;
}

} // namespace

XMLConfigWriter::XMLConfigWriter(QObject *parent)
    : QObject(parent)
{
    xml = new QXmlStreamWriter();
    xml->setAutoFormatting(true);
    configFile = nullptr;
    m_joystick = nullptr;
    m_joystickXml = nullptr;
    writerError = false;
}

XMLConfigWriter::~XMLConfigWriter()
{
    if (configFile != nullptr)
    {
        if (configFile->isOpen())
            configFile->close();

        delete configFile;
        configFile = nullptr;
    }

    if (xml != nullptr)
    {
        delete xml;
        xml = nullptr;
    }
}

/**
 * @brief Write input device config from the current object into XML file
 * @param[in] joystickXml InputDeviceXml which gets serialized
 */
void XMLConfigWriter::write(InputDeviceXml *joystickXml)
{
    writerError = false;
    writerErrorString.clear();

    if (configFile == nullptr || joystickXml == nullptr)
    {
        writerError = true;
        writerErrorString = tr("Could not write to profile at %1.").arg(fileName);
        return;
    }

    QString unsupportedTarget;
    if (targetDeclaresNewerVersion(configFile->fileName(), unsupportedTarget))
    {
        writerError = true;
        writerErrorString = unsupportedTarget;
        return;
    }

    QSaveFile output(configFile->fileName());
    // Never fall back to truncating the existing profile in place.
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        writerError = true;
        writerErrorString = tr("Could not write to profile at %1.\n%2").arg(fileName, output.errorString());
        return;
    }

    xml->setDevice(&output);
    xml->writeStartDocument();
    joystickXml->writeConfig(xml);
    xml->writeEndDocument();
    if (xml->hasError())
        output.cancelWriting();
    // Do not leave a pointer to the scoped output device in the stream writer.
    xml->setDevice(nullptr);

    if (!output.commit())
    {
        writerError = true;
        writerErrorString = tr("Could not write to profile at %1.\n%2").arg(fileName, output.errorString());
    }
}

/**
 * @brief Sets the filename of the to be written XML file
 */
void XMLConfigWriter::setFileName(QString filename)
{
    delete configFile;
    fileName = filename;
    configFile = new QFile(filename);
}

bool XMLConfigWriter::hasError() { return writerError; }

const QString XMLConfigWriter::getErrorString() { return writerErrorString; }

const QXmlStreamWriter *XMLConfigWriter::getXml() { return xml; }

QString const &XMLConfigWriter::getFileName() { return fileName; }

const QFile *XMLConfigWriter::getConfigFile() { return configFile; }

const InputDevice *XMLConfigWriter::getJoystick() { return m_joystick; }
