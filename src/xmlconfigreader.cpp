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

#include "xmlconfigreader.h"

#include "globalvariables.h"
#include "profileformat.h"
#include "profilemigration.h"
#include "xml/inputdevicexml.h"
#include "xmlconfigmigration.h"

#include <QFile>
#include <QStringList>
#include <QXmlStreamReader>

/**
 * @brief Main XML config writer class
 */
XMLConfigReader::XMLConfigReader(QObject *parent)
    : QObject(parent)
{
    xml = new QXmlStreamReader();
    configFile = nullptr;
    m_joystick = nullptr;
    initDeviceTypes();
}

XMLConfigReader::~XMLConfigReader()
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

void XMLConfigReader::setJoystick(InputDevice *joystick) { m_joystick = joystick; }

/**
 * @brief Sets the filename of the to be read XML file
 */
void XMLConfigReader::setFileName(QString filename)
{
    // Detach the stream before releasing its previous device, including when
    // the newly selected path does not exist yet.
    xml->clear();
    delete configFile;
    fileName = filename;
    configFile = new QFile(filename);
}

/**
 * @brief Read input device config from the current XML file into the InputDevice object
 * @param[in,out] joystick InputDevice into which the config is read
 */
void XMLConfigReader::configJoystick(InputDevice *joystick)
{
    m_joystick = joystick;
    read();
}

bool XMLConfigReader::read()
{
    xml->clear();
    if (configFile == nullptr || configFile->fileName().isEmpty())
    {
        xml->raiseError(tr("No profile filename was specified."));
        return true;
    }
    if (m_joystick == nullptr)
    {
        xml->raiseError(tr("No input device was specified for profile %1.").arg(configFile->fileName()));
        return true;
    }

    // Reopen on every read so retries see the current file and start at byte 0.
    configFile->close();
    if (!configFile->open(QFile::ReadOnly | QFile::Text))
    {
        xml->raiseError(tr("Could not open profile %1.\n%2").arg(configFile->fileName(), configFile->errorString()));
        return true;
    }
    // Validate the complete document before InputDeviceXml can reset or mutate
    // mappings. Keep this streaming to avoid buffering the whole profile.
    {
        QXmlStreamReader validation(configFile);
        while (!validation.atEnd())
            validation.readNext();
        if (validation.hasError() || configFile->error() != QFile::NoError)
        {
            const QString details = configFile->error() != QFile::NoError ? configFile->errorString()
                                                                          : tr("XML error at line %1, column %2: %3")
                                                                                .arg(validation.lineNumber())
                                                                                .arg(validation.columnNumber())
                                                                                .arg(validation.errorString());
            configFile->close();
            xml->raiseError(tr("Could not read profile %1.\n%2").arg(configFile->fileName(), details));
            return true;
        }
    }
    if (!configFile->seek(0))
    {
        const QString details = configFile->errorString();
        configFile->close();
        xml->raiseError(tr("Could not read profile %1.\n%2").arg(configFile->fileName(), details));
        return true;
    }
    xml->setDevice(configFile);
    bool error = false;
    xml->readNextStartElement();

    if (!deviceTypes.contains(xml->name().toString()))
    {
        xml->raiseError("Root node is not a joystick or controller");
    } else if (!validateConfigVersion())
    {
        // The profile declares a format this build cannot represent.
        // validateConfigVersion() has already described the problem.
    } else if (xml->name().toString() == GlobalVariables::Joystick::xmlName)
    {
        XMLConfigMigration migration(xml);

        if (migration.requiresMigration())
        {
            QString migrationString = migration.migrate();

            if (migrationString.length() > 0)
            {
                configFile->close();
                QString migrationError;
                if (ProfileMigration::save(configFile->fileName(), migrationString, migrationError))
                {
                    xml->clear();
                    xml->addData(migrationString);
                    xml->readNextStartElement();
                } else
                {
                    xml->raiseError(tr("Could not write updated profile XML to file %1.\n%2")
                                        .arg(configFile->fileName(), migrationError));
                }
            }
        }
    }

    while (!xml->atEnd())
    {
        if (xml->isStartElement() && deviceTypes.contains(xml->name().toString()))
        {
            InputDeviceXml *joystick_xml = new InputDeviceXml(m_joystick);
            joystick_xml->readConfig(xml);
            joystick_xml->deleteLater();
        } else
        {
            // If none of the above, skip the element
            xml->skipCurrentElement();
        }

        xml->readNextStartElement();
    }

    if (configFile->isOpen())
        configFile->close();

    if (xml->hasError() && (xml->error() != QXmlStreamReader::PrematureEndOfDocumentError))
    {
        error = true;
    } else if (xml->hasError() && (xml->error() == QXmlStreamReader::PrematureEndOfDocumentError))
    {
        xml->clear();
    }
    return error;
}

/**
 * @brief Check the document's "configversion" before any mapping is touched
 *
 * Profiles written by a newer AntiMicroX can contain elements which are unknown
 * to this build. Parsing them would silently drop those mappings, and a later
 * save would then overwrite the original file with the reduced version, so such
 * profiles are rejected while the file is left untouched.
 *
 * @return true when the profile may be parsed
 */
bool XMLConfigReader::validateConfigVersion()
{
    const QXmlStreamAttributes attributes = xml->attributes();
    if (!attributes.hasAttribute(QStringLiteral("configversion")))
        return true; // Profiles created before the version attribute existed.

    const QString declared = attributes.value(QStringLiteral("configversion")).toString().trimmed();
    bool converted = false;
    const int version = declared.toInt(&converted);
    if (!converted || (version < 0))
    {
        xml->raiseError(tr("Could not read profile %1.\nThe profile declares an unreadable config version "
                           "\"%2\".")
                            .arg(configFile->fileName(), declared));
        return false;
    }

    if (version > ProfileFormat::LATEST_FILE_VERSION)
    {
        QString message = tr("Could not read profile %1.\nThe profile uses config version %2, which is newer "
                             "than the newest config version supported by this build (%3). Update AntiMicroX "
                             "to use this profile.")
                              .arg(configFile->fileName(), declared, QString::number(ProfileFormat::LATEST_FILE_VERSION));
        const QString writingApp = attributes.value(QStringLiteral("appversion")).toString();
        if (!writingApp.isEmpty())
            message.append(QStringLiteral("\n")).append(tr("The profile was written by AntiMicroX %1.").arg(writingApp));

        xml->raiseError(message);
        return false;
    }

    return true;
}

const QString XMLConfigReader::getErrorString()
{
    QString temp = QString();

    if (xml->hasError())
        temp = xml->errorString();

    return temp;
}

bool XMLConfigReader::hasError() { return xml->hasError(); }

void XMLConfigReader::initDeviceTypes()
{
    deviceTypes.clear();
    deviceTypes.append(GlobalVariables::Joystick::xmlName);
    deviceTypes.append(GlobalVariables::GameController::xmlName);
}

const QXmlStreamReader *XMLConfigReader::getXml() { return xml; }

QString const &XMLConfigReader::getFileName() { return fileName; }

const QFile *XMLConfigReader::getConfigFile() { return configFile; }

const InputDevice *XMLConfigReader::getJoystick() { return m_joystick; }

QStringList const &XMLConfigReader::getDeviceTypes() { return deviceTypes; }
