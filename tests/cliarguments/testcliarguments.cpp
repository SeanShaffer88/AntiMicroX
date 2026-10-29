// SPDX-License-Identifier: GPL-3.0-or-later

#include "cliarguments.h"

#include <QtTest>

class TestCliArguments : public QObject
{
    Q_OBJECT

  private slots:
    void controllerSelector_data();
    void controllerSelector();
    void setNumber_data();
    void setNumber();
    void profileSuffix_data();
    void profileSuffix();
};

void TestCliArguments::controllerSelector_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("kind");
    QTest::addColumn<int>("index");

    const int index = int(CliArguments::SelectorKind::Index);
    const int identifier = int(CliArguments::SelectorKind::Identifier);
    const int invalid = int(CliArguments::SelectorKind::Invalid);

    QTest::newRow("zero") << QString("0") << index << 0;
    QTest::newRow("first") << QString("1") << index << 1;
    QTest::newRow("many") << QString("16") << index << 16;
    QTest::newRow("padded") << QString(" 2 ") << index << 2;
    QTest::newRow("signed") << QString("+3") << index << 3;
    QTest::newRow("negative") << QString("-1") << invalid << 0;
    QTest::newRow("leading-zeros") << QString("007") << index << 7;
    QTest::newRow("name") << QString("Xbox Controller") << identifier << 0;
    QTest::newRow("guid") << QString("030000005e0400008e02000010010000") << identifier << 0;
    QTest::newRow("hex-looking") << QString("0x10") << identifier << 0;
    QTest::newRow("overflow") << QString("99999999999") << identifier << 0;
    QTest::newRow("empty") << QString() << invalid << 0;
    QTest::newRow("blank") << QString("   ") << invalid << 0;
}

void TestCliArguments::controllerSelector()
{
    QFETCH(QString, text);
    QFETCH(int, kind);
    QFETCH(int, index);

    int parsedIndex = -1;
    const CliArguments::SelectorKind result = CliArguments::classifyControllerSelector(text, parsedIndex);
    QCOMPARE(int(result), kind);
    if (result == CliArguments::SelectorKind::Index)
        QCOMPARE(parsedIndex, index);
    else
        QCOMPARE(parsedIndex, -1); // untouched for identifiers and invalid values
}

void TestCliArguments::setNumber_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<int>("number");

    for (int number = CliArguments::FIRST_SET_NUMBER; number <= CliArguments::LAST_SET_NUMBER; ++number)
        QTest::newRow(qPrintable(QString("set-%1").arg(number))) << QString::number(number) << true << number;

    QTest::newRow("padded") << QString(" 4 ") << true << 4;
    QTest::newRow("below-range") << QString("0") << false << 0;
    QTest::newRow("above-range") << QString("9") << false << 0;
    QTest::newRow("negative") << QString("-2") << false << 0;
    QTest::newRow("fraction") << QString("2.5") << false << 0;
    QTest::newRow("text") << QString("second") << false << 0;
    QTest::newRow("empty") << QString() << false << 0;
    QTest::newRow("blank") << QString("   ") << false << 0;
}

void TestCliArguments::setNumber()
{
    QFETCH(QString, text);
    QFETCH(bool, valid);
    QFETCH(int, number);

    int parsed = -1;
    QCOMPARE(CliArguments::parseSetNumber(text, parsed), valid);
    if (valid)
        QCOMPARE(parsed, number);
    else
        QCOMPARE(parsed, -1);
}

void TestCliArguments::profileSuffix_data()
{
    QTest::addColumn<QString>("suffix");
    QTest::addColumn<bool>("supported");

    QTest::newRow("amgp") << QString("amgp") << true;
    QTest::newRow("xml") << QString("xml") << true;
    QTest::newRow("upper-amgp") << QString("AMGP") << true;
    QTest::newRow("mixed-xml") << QString("Xml") << true;
    QTest::newRow("txt") << QString("txt") << false;
    QTest::newRow("empty") << QString() << false;
    QTest::newRow("padded") << QString(" xml ") << false;
    QTest::newRow("similar") << QString("amgp2") << false;
}

void TestCliArguments::profileSuffix()
{
    QFETCH(QString, suffix);
    QFETCH(bool, supported);
    QCOMPARE(CliArguments::isSupportedProfileSuffix(suffix), supported);
}

QTEST_GUILESS_MAIN(TestCliArguments)
#include "testcliarguments.moc"
