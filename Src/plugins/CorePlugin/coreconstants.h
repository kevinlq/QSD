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

#include <QtGlobal>

namespace CorePlugin {
namespace Constants {

// Modes
const char MODE_WELCOME[]          = "Welcome";
const char MODE_EDIT[]             = "Edit";
const char MODE_DESIGN[]           = "Design";
const int  P_MODE_WELCOME          = 100;
const int  P_MODE_EDIT             = 90;
const int  P_MODE_DESIGN           = 89;

static const char RADAR_MENU_NAME[] = QT_TRANSLATE_NOOP("Menu", "Radar");
static const int RADAR_MENU_PRIORITY = 0;

static const char HELP_MENU_NAME[] = QT_TRANSLATE_NOOP("Menu", "Help");
static const int HELP_MENU_PRIORITY = 100;

static const int VIEW_LABEL_LEADING = 0;
static const int VIEW_LABEL_WIDTH = 70;

static const int VIEW_PANEL_PRIORITY = 0;
static const int PLOT_PANEL_PRIORITY = 100;
static const int TRACK_PANEL_PRIORITY = 200;
static const int REPLAY_PANEL_PRIORITY = 300;

} // namespace Constants
} // namespace Core
