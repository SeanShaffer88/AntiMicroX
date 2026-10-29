// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TRANSLATIONS_H
#define TRANSLATIONS_H

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

/**
 * @brief Locating the packaged translation files
 *
 * Startup and the language switch in the settings dialog used to build their
 * search paths separately, so a change in one place could silently disable
 * translations in the other one. Both use these helpers now.
 *
 * Only Qt Core is required, which keeps the behaviour testable without a
 * display server or the application's SDL dependencies.
 */
namespace Translations {

/** Installation directory of the packaged application translations. */
inline QString installationSubdirectory() { return QStringLiteral("share/antimicrox/translations"); }

/** Flatpak installs everything below this prefix. */
inline QString flatpakPrefix() { return QStringLiteral("/app"); }

/**
 * @brief Directories which may contain antimicrox_*.qm files, in priority order
 * @param[in] applicationDirPath Directory of the executable
 */
inline QStringList appTranslationDirs(const QString &applicationDirPath)
{
    QStringList directories;
    directories << flatpakPrefix() + QLatin1Char('/') + installationSubdirectory();
    if (!applicationDirPath.isEmpty())
    {
        const QDir binaryDir(applicationDirPath);
        directories << QDir::cleanPath(binaryDir.filePath(QStringLiteral("../") + installationSubdirectory()));
        directories << QDir::cleanPath(binaryDir.filePath(installationSubdirectory()));
    }
    directories.removeDuplicates();
    return directories;
}

/**
 * @brief File name of an application translation
 *
 * The language code comes from a user editable settings file, so anything that
 * could escape the translation directory is rejected.
 *
 * @return Empty string when the language code is not usable
 */
inline QString appTranslationFileName(const QString &language)
{
    const QString code = language.trimmed();
    if (code.isEmpty() || code.contains(QLatin1Char('/')) || code.contains(QLatin1Char('\\')) ||
        code.contains(QStringLiteral("..")))
        return QString();

    return QStringLiteral("antimicrox_%1.qm").arg(code);
}

/** File name of a Qt translation for the given language code. */
inline QString qtTranslationFileName(const QString &language)
{
    const QString code = language.trimmed();
    if (code.isEmpty() || code.contains(QLatin1Char('/')) || code.contains(QLatin1Char('\\')) ||
        code.contains(QStringLiteral("..")))
        return QString();

    return QStringLiteral("qt_%1.qm").arg(code);
}

/** First existing translation file in the given directories. */
inline QString findTranslation(const QString &fileName, const QStringList &directories)
{
    if (fileName.isEmpty())
        return QString();

    for (const QString &directory : directories)
    {
        if (directory.isEmpty())
            continue;

        const QString candidate = QDir(directory).filePath(fileName);
        if (QFileInfo(candidate).isFile())
            return candidate;
    }

    return QString();
}

/** Application translation file for the language, using the standard layout. */
inline QString findAppTranslation(const QString &language, const QString &applicationDirPath)
{
    return findTranslation(appTranslationFileName(language), appTranslationDirs(applicationDirPath));
}

/** Directories which may contain Qt's own qt_*.qm files, in priority order. */
inline QStringList qtTranslationDirs(const QString &qtTranslationsPath, const QString &applicationDirPath)
{
    QStringList directories;
    if (!qtTranslationsPath.isEmpty())
        directories << qtTranslationsPath;
    if (!applicationDirPath.isEmpty())
    {
        const QDir binaryDir(applicationDirPath);
        directories << QDir::cleanPath(binaryDir.filePath(QStringLiteral("../share/qt/translations")));
        directories << QDir::cleanPath(binaryDir.filePath(QStringLiteral("share/qt/translations")));
    }
    directories.removeDuplicates();
    return directories;
}

/** Qt translation file for the language, using the standard layout. */
inline QString findQtTranslation(const QString &language, const QString &qtTranslationsPath,
                                 const QString &applicationDirPath)
{
    return findTranslation(qtTranslationFileName(language), qtTranslationDirs(qtTranslationsPath, applicationDirPath));
}

/**
 * @brief Language selected by the user, read from the settings
 *
 * The main key is "Language". Ini files are user editable and QSettings is
 * case insensitive on other platforms, so a differently cased spelling of the
 * same top level key is accepted instead of silently falling back to the
 * system language.
 */
inline QString languageSetting(const QSettings &settings)
{
    const QString key = QStringLiteral("Language");
    if (settings.contains(key))
        return settings.value(key).toString().trimmed();

    for (const QString &storedKey : settings.allKeys())
    {
        if (storedKey.compare(key, Qt::CaseInsensitive) == 0)
            return settings.value(storedKey).toString().trimmed();
    }

    return QString();
}

/**
 * @brief Remove the stored language so the system language is used again
 *
 * Removes the canonical key and any differently cased spelling of it, because
 * languageSetting() accepts those as well.
 *
 * @return true when something was removed
 */
inline bool clearLanguageSetting(QSettings &settings)
{
    const QString key = QStringLiteral("Language");
    bool removed = false;

    if (settings.contains(key))
    {
        settings.remove(key);
        removed = true;
    }

    for (const QString &storedKey : settings.allKeys())
    {
        if ((storedKey.compare(key, Qt::CaseInsensitive) == 0) && settings.contains(storedKey))
        {
            settings.remove(storedKey);
            removed = true;
        }
    }

    return removed;
}

} // namespace Translations

#endif // TRANSLATIONS_H
