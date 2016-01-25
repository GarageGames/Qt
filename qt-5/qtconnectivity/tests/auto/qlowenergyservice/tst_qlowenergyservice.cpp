/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>

#include <QtBluetooth/qlowenergyservice.h>


/*
 * This is a very simple test despite the complexity of QLowEnergyService.
 * It mostly aims to test the static API behaviors of the class. The connection
 * orientated elements are test by the test for QLowEnergyController as it
 * is impossible to test the two classes separately from each other.
 */

class tst_QLowEnergyService : public QObject
{
    Q_OBJECT

private slots:
    void tst_flags();
};

void tst_QLowEnergyService::tst_flags()
{
    QLowEnergyService::ServiceTypes flag1(QLowEnergyService::PrimaryService);
    QLowEnergyService::ServiceTypes flag2(QLowEnergyService::IncludedService);
    QLowEnergyService::ServiceTypes result;

    // test QFlags &operator|=(QFlags f)
    result = flag1 | flag2;
    QVERIFY(result.testFlag(QLowEnergyService::PrimaryService));
    QVERIFY(result.testFlag(QLowEnergyService::IncludedService));

    // test QFlags &operator|=(Enum f)
    result = flag1 | QLowEnergyService::IncludedService;
    QVERIFY(result.testFlag(QLowEnergyService::PrimaryService));
    QVERIFY(result.testFlag(QLowEnergyService::IncludedService));

    // test Q_DECLARE_OPERATORS_FOR_FLAGS(QLowEnergyService::ServiceTypes)
    result = QLowEnergyService::IncludedService | flag1;
    QVERIFY(result.testFlag(QLowEnergyService::PrimaryService));
    QVERIFY(result.testFlag(QLowEnergyService::IncludedService));
}

QTEST_MAIN(tst_QLowEnergyService)

#include "tst_qlowenergyservice.moc"
