/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
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
****************************************************************************/

#pragma once

namespace Core {
namespace Constants {

#define STRINGIFY_INTERNAL(x) #x
#define STRINGIFY(x) STRINGIFY_INTERNAL(x)

const char IDE_DISPLAY_NAME[] = "$${IDE_DISPLAY_NAME}";
const char IDE_ID[] = "$${IDE_ID}";
const char IDE_CASED_ID[] = "$${IDE_CASED_ID}";

#define IDE_VERSION $${QTCREATOR_VERSION}
#define IDE_VERSION_STR STRINGIFY(IDE_VERSION)
#define IDE_VERSION_DISPLAY_DEF $${QTCREATOR_DISPLAY_VERSION}

#define IDE_VERSION_MAJOR $$replace(QTCREATOR_VERSION, "^(\\d+)\\.\\d+\\.\\d+(-.*)?$", \\1)
#define IDE_VERSION_MINOR $$replace(QTCREATOR_VERSION, "^\\d+\\.(\\d+)\\.\\d+(-.*)?$", \\1)
#define IDE_VERSION_RELEASE $$replace(QTCREATOR_VERSION, "^\\d+\\.\\d+\\.(\\d+)(-.*)?$", \\1)

const char * const IDE_VERSION_LONG      = IDE_VERSION_STR;
const char * const IDE_VERSION_DISPLAY   = STRINGIFY(IDE_VERSION_DISPLAY_DEF);
const char * const IDE_AUTHOR            = "The Qt Company Ltd";
const char * const IDE_YEAR              = "$${QTCREATOR_COPYRIGHT_YEAR}";

#ifdef IDE_REVISION
const char * const IDE_REVISION_STR      = STRINGIFY(IDE_REVISION);
#else
const char * const IDE_REVISION_STR      = "\\";
#endif

// changes the path where the settings are saved to
#ifdef IDE_SETTINGSVARIANT
const char * const IDE_SETTINGSVARIANT_STR      = STRINGIFY(IDE_SETTINGSVARIANT);
#else
const char * const IDE_SETTINGSVARIANT_STR      = "QtProject";
#endif

#ifdef IDE_COPY_SETTINGS_FROM_VARIANT
const char * const IDE_COPY_SETTINGS_FROM_VARIANT_STR = STRINGIFY(IDE_COPY_SETTINGS_FROM_VARIANT);
#else
const char * const IDE_COPY_SETTINGS_FROM_VARIANT_STR = "Nokia";
#endif

#undef IDE_VERSION_DISPLAY_DEF
#undef IDE_VERSION
#undef IDE_VERSION_STR
#undef STRINGIFY
#undef STRINGIFY_INTERNAL

} // Constants
} // Core
