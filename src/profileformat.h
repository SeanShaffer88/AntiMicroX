// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PROFILEFORMAT_H
#define PROFILEFORMAT_H

/**
 * @brief Profile file format ("configversion") constants
 *
 * Kept free of SDL, widgets and generated headers so lightweight profile
 * tools and tests can share the same values as the application.
 */
namespace ProfileFormat {
// Highest profile format version which this build is able to read and write.
constexpr int LATEST_FILE_VERSION = 19;

// Last known profile version that requires a migration before it can be used.
constexpr int LATEST_MIGRATION_VERSION = 5;
} // namespace ProfileFormat

#endif // PROFILEFORMAT_H
