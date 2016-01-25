# Enginio Qt Library development branch
Client library for accessing Enginio service from Qt and QML code.

# System Requirements
* Qt 5.1 or newer
* OpenSSL library
  * Mac OS X: the OpenSSL library should be preinstalled.
  * Linux: Most distributions have a preinstalled OpenSSL library. If yours doesn't, seach for `libssl` in the package repository.
  * Windows: Get the installer from http://slproweb.com/products/Win32OpenSSL.html (light version is enough, copy DLLs to windows system directory when asked).
* Perl 5.10 or newer
  * Mac and Linux: Perl should be preinstalled.
  * Windows: http://www.perl.org/get.html

# Build & Install
    * `qmake && make install`
    * Produces shared library and installs it as a globally available Qt5 module

# Usage
* In C++ applications
    * Use Enginio module by adding `QT += enginio` to application `.pro` file
    * Include Enginio headers with `<Enginio/...>` (for example: `#include <Enginio/enginioclient.h>`)
* In QML applications
    * Import Enginio components with `import Enginio 1.0`

# Contributing
* Contributing to the Enginio Qt Library works the same way as contributing to any other Qt module
    * See [Qt Contribution Guidelines](http://wiki.qt.io/Qt_Contribution_Guidelines)
    * Clone the repository from ssh://codereview.qt-project.org/qt/qtenginio.git
    * Implement the new feature and automated tests for it
    * Make sure all tests pass
    * Push your change for review to [Gerrit](http://wiki.qt.io/Gerrit_Introduction)
    * Add reviewers (You can find e-mail addresses in git log and git blame)

# Copyright
Copyright (C) 2015 The Qt Company Ltd.
Contact http://www.qt.io/contact-us


# License
Enginio Qt library is tri-licensed under the Commercial Qt License, the GNU General Public License 3.0, and the GNU Lesser General Public License 2.1.

**Commercial License Usage**
Licensees holding valid Commercial Qt Licenses may use this library in accordance with the commercial license agreement provided with the Software or, alternatively, in accordance with the terms contained in a written agreement between you and The Qt Company. For licensing terms and conditions see http://www.qt.io/terms-conditions. For further information use the contact form at http://www.qt.io/contact-us.

**GNU Lesser General Public License Usage**
Alternatively, this file may be used under the terms of the GNU Lesser General Public License version 2.1 or version 3 as published by the Free Software Foundation and appearing in the file LICENSE.LGPLv21 and LICENSE.LGPLv3 included in the packaging of this file. Please review the following information to ensure the GNU Lesser General Public License requirements will be met: https://www.gnu.org/licenses/lgpl.html and http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.

As a special exception, The Qt Company gives you certain additional rights. These rights are described in The Qt Company LGPL Exception version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
