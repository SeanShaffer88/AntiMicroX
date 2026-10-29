// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CLIARGUMENTS_H
#define CLIARGUMENTS_H

#include <QString>

/**
 * @brief Classification of command line values
 *
 * Several options accept "a controller index, name, or GUID" and one accepts a
 * controller set number. Classifying those strings in a single place keeps the
 * options consistent and makes the rules testable without a QApplication.
 *
 * A value which is a number is an index: SDL controller numbers are zero based,
 * but the command line keeps treating 0 as "all controllers" for compatibility.
 * Everything else is passed on as a controller name or GUID.
 */
namespace CliArguments {

constexpr int FIRST_SET_NUMBER = 1;
constexpr int LAST_SET_NUMBER = 8;

/** What a controller selector on the command line describes. */
enum class SelectorKind
{
    Index,      //!< A zero based controller index
    Identifier, //!< A controller name or GUID
    Invalid     //!< Empty value or a number which cannot be an index (e.g. -1)
};

/**
 * @brief Classify a controller selector
 * @param[in] text Raw command line value
 * @param[out] index Set to the controller index when the result is Index
 * @return How the value has to be interpreted
 */
inline SelectorKind classifyControllerSelector(const QString &text, int &index)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return SelectorKind::Invalid;

    bool converted = false;
    const int value = trimmed.toInt(&converted);
    if (converted)
    {
        if (value < 0)
            return SelectorKind::Invalid;

        index = value;
        return SelectorKind::Index;
    }

    return SelectorKind::Identifier;
}

/**
 * @brief Classify a controller set number
 * @param[in] text Raw command line value
 * @param[out] number Set to the set number, which is one based
 * @return true when the value is a valid set number
 */
inline bool parseSetNumber(const QString &text, int &number)
{
    bool converted = false;
    const int value = text.trimmed().toInt(&converted);
    if (!converted || (value < FIRST_SET_NUMBER) || (value > LAST_SET_NUMBER))
        return false;

    number = value;
    return true;
}

/**
 * @brief Tell whether a profile file name has a supported extension
 *
 * Extensions are matched without regard to case, because profiles are regular
 * files which may come from other systems or file managers.
 */
inline bool isSupportedProfileSuffix(const QString &suffix)
{
    return (suffix.compare(QStringLiteral("amgp"), Qt::CaseInsensitive) == 0) ||
           (suffix.compare(QStringLiteral("xml"), Qt::CaseInsensitive) == 0);
}

} // namespace CliArguments

#endif // CLIARGUMENTS_H
